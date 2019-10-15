#include <algorithm>
#include <stdarg.h>

#include "iterator.h"
#include "master.h"
#include "memusage_allocator.h"

void die(const char *fmt, ...);
size_t get_entry_overhead();
static std::string get_sst_filename(size_t shard, size_t epoch);

void master::on_new_url(std::string_view url) {
  // Find the right shard
  std::hash<std::string_view> hasher;
  size_t shard_no = hasher(url) % _n_shards;
  auto it = _memtables[shard_no].find(url);
  if (it == _memtables[shard_no].end()) {
    _memtables[shard_no].insert({std::string(url), 1});
    // Update the memory usage in the memtable
    _mem_usage += estimate_map_entry_mem_usage(url);
    _mem_usage_per_table[shard_no] += estimate_map_entry_mem_usage(url);
    if (_mem_usage > _mem_high_water_mark) {
      auto evict_shard =
          std::max_element(_mem_usage_per_table.get(),
                           _mem_usage_per_table.get() + _n_shards) -
          _mem_usage_per_table.get();
      flush_memtable(evict_shard);
    }
  } else {
    it->second++;
  }
}

void master::start() {
  FILE *input = ::fopen(_input_file.c_str(), "r");
  if (!input) {
    // No reason to recover if I can't even open the input file
    die("Cannot open the file: %s [%d %s]\n", _input_file.c_str(), errno,
        strerror(errno));
  }
  read_line_iter line(input);
  while (line.valid()) {
    on_new_url(*line);
    ++line;
  }
  assert(fclose(input) == 0);
  for (int i = 0; i < _n_shards; i++) {
    flush_memtable(i);
  }
  for (size_t shard = 0; shard < _n_shards; shard++) {
    spawn_worker([this, shard] { this->merge_worker(shard); });
  }
}

void write_sst(master::memtable_type memtable, FILE *output) {
  for (const auto &e : memtable) {
    size_t key_size = e.first.size();
    // encoding: key_size, key, value
    fwrite(&key_size, sizeof(size_t), 1, output);
    fwrite(e.first.data(), 1, key_size, output);
    fwrite(&e.second, sizeof(e.second), 1, output);
  }
  fflush(output);
}

size_t master::flush_memtable(size_t shard) {
  assert(shard < _n_shards);
  auto filename = get_sst_filename(shard, _epochs[shard]);
  FILE *output = fopen(filename.c_str(), "w+b");
  if (!output) {
    die("Cannot write to sst file: %s, err: %s\n", filename.c_str(),
        strerror(errno));
  }
  write_sst(std::move(_memtables[shard]), output);
  assert(fclose(output) == 0);
  // Adjust the memory usage estimation.
  size_t saved = _mem_usage_per_table[shard];
  _mem_usage -= _mem_usage_per_table[shard];
  _mem_usage_per_table[shard] = 0;
  _epochs[shard]++;
  return saved;
}

size_t master::estimate_map_entry_mem_usage(std::string_view url) {
  static size_t entry_overhead = std::max(get_entry_overhead(), (size_t)72);
  return url.size() + entry_overhead;
}

void master::merge_worker(size_t shard) {
  std::vector<sst_read_iter> iters;
  master::heap_type private_heap(_top_k);
  // 1. create all sst_iters.
  for (size_t epoch = 0; epoch < _epochs[shard]; epoch++) {
    auto filename = get_sst_filename(shard, epoch);
    FILE *input = fopen(filename.c_str(), "rb");
    if (!input) {
      die("Cannot open the staged sst: %s\n", filename.c_str());
    }
    iters.emplace_back(input);
  }
  // 2. use merge_iter to merge the result and save it in private workspace.
  merge_iter<sst_read_iter> miter(iters.begin(), iters.end());
  owned_url_t last_url = "";
  count_t last_count = 0;
  while (miter.valid()) {
    if (miter->url != last_url) {
      if (last_url != "") {
        private_heap.add(std::move(last_url), last_count);
      }
      last_url = miter->url;
      last_count = miter->count;
    } else {
      last_count += miter->count;
    }
    ++miter;
  }
  if (last_url != "") {
    private_heap.add(std::move(last_url), last_count);
  }
  // 3. close all files;
  for (auto &iter : iters) {
    iter.close();
  }
  // 4. commit the entries from the private workspace to the result in master
  for (auto &&e : private_heap) {
    std::lock_guard<std::mutex> lk(_result_mtx);
    _result.add(std::move(e.url), e.count);
  }
  // 5. remove all the files
  for (size_t epoch = 0; epoch < _epochs[shard]; epoch++) {
    auto filename = get_sst_filename(shard, epoch);
    assert(unlink(filename.c_str()) == 0);
  }
}

size_t get_entry_overhead() {
  std::map<owned_url_t, count_t, std::less<>,
           memusage_allocator<std::pair<owned_url_t, count_t>>>
      m;
  memusage_measure_guard g;
  m.insert({"", 1});
  return g.current_usage();
}

std::string get_sst_filename(size_t shard, size_t epoch) {
  constexpr size_t filename_size = 64;
  char filename[filename_size];
  /* filename schema: stage-(shard)-(epoch) */
  int n = snprintf(filename, filename_size, "_%lu/stage-%lu.sst", shard, epoch);
  return {filename, (size_t)n};
}

void die(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  std::abort();
}