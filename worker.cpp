#include <algorithm>
#include "worker.h"
#include "sstable.h"

void master::on_new_url(std::string_view url) {
    std::hash<std::string_view> hasher;
    size_t shard_no = hasher(url) % _n_shards;
    auto it = _memtables[shard_no].find(url);
    if (it == _memtables[shard_no].end()) {
        _memtables[shard_no].insert({std::string(url), 1});
        /* Update the memory usage in the memtable */
        _mem_usage += estimate_map_entry_mem_usage(url);
        if (_mem_usage > _mem_limit) {
            auto evict_shard = std::max_element(_mem_usage_per_table.get(), _mem_usage_per_table.get() + _n_shards) - _mem_usage_per_table.get();
            flush_memtable(evict_shard);
        }
    } else {
        it->second++;
    }
}

void master::start() {
    // TODO: The assumption that the URL is 1024 might not hold.
    constexpr size_t buf_size = 1'024;
    char buf[buf_size];
    std::hash<std::string_view> hasher;
    FILE *input = ::fopen(_input_file.c_str(), "r");
    if (!input) {
        /* No reason to recover if I can't even open the input file */
        fprintf(stderr, "Cannot open the file: %s [%d %s]", _input_file.c_str(), errno, strerror(errno));
        std::abort();
    }
    while (fgets(buf, buf_size, input)) {
        std::string_view url(buf);
        if (!url.empty() && url[url.size()-1] == '\n') {
            url.remove_suffix(1);
        }
        on_new_url(url);
    } 
    if (feof(input)) {
        fclose(input);
    } else {
        /* No reason to recover */
        std::abort();
    }
}

int master::flush_memtable(size_t shard) {
    assert(shard < _n_shards);
    constexpr size_t filename_size = 64;
    char filename[filename_size];
    /* filename schema: state-(shard)-(epoch) */
    snprintf(filename, filename_size, "stage-%d-%d.sst", shard, _epochs[shard]);

    FILE *output = fopen(filename, "w+");
    if (!output) {
        return -errno;
    }

    for (const auto &e: _memtables[shard]) {
            size_t key_size = e.first.size();
            // encoding: key_size, key, value
            fwrite(&key_size, sizeof(size_t), 1, output);
            fwrite(e.first.data(), 1, key_size, output);
            fwrite(&e.second, sizeof(e.second), 1, output);
    }
    

    /* Release the memory */
    _memtables[shard].clear();
    _mem_usage -= _mem_usage_per_table[shard];
    _mem_usage_per_table[shard] = 0;
    return 0;
}