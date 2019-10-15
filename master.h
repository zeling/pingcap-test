#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "entry.h"
#include "heap.h"
#include "types.h"

class master {
public:
  /* Use the transparent compare */
  using memtable_type = std::map<owned_url_t, count_t, std::less<>>;
  using heap_type = heap<entry<owned_url_t, false>>;

  master(std::string input, size_t n_shards, size_t mem_high_water_mark,
         size_t top_k)
      : _n_shards(n_shards), _mem_usage(0),
        _mem_high_water_mark(mem_high_water_mark), _top_k(top_k),
        _result(top_k), _input_file(std::move(input)),
        _memtables(std::make_unique<memtable_type[]>(n_shards)),
        _mem_usage_per_table(std::make_unique<size_t[]>(n_shards)),
        _epochs(std::make_unique<size_t[]>(n_shards)) {
    size_t npage;
    FILE *statm = fopen("/proc/self/statm", "r");
    assert(statm != NULL);
    assert(fscanf(statm, "%lu", &npage) == 1);
    _mem_usage = npage * getpagesize();
    assert(fclose(statm) == 0);
    for (int i = 0; i < _n_shards; i++) {
        char buf[4];
        sprintf(buf, "_%d", i);
        struct stat st = {0};
        if (stat(buf, &st) == -1) {
            mkdir(buf, 0700);
        }
    }
  }

  ~master() {
    /* Make sure when the master is dropped, all threads are waited */
    wait_for_all_workers();
    for (int i = 0; i < _n_shards; i++) {
        char buf[4];
        sprintf(buf, "_%d", i);
        rmdir(buf);
    }
  }

  template <typename F> void spawn_worker(F &&f) {
    _worker_threads.emplace_back(std::forward<F>(f));
  }

  void wait_for_all_workers() {
    std::vector<std::thread> current_threads;
    current_threads.swap(_worker_threads);
    for (auto &&t : current_threads) {
      t.join();
    }
  }

  void start();
  void merge_worker(size_t shard);

  heap_type::iterator result_begin() { return _result.begin(); }

  heap_type::iterator result_end() { return _result.end(); }

  size_t flush_all() {
    size_t flushed = 0;
    for (size_t i = 0; i < _n_shards; i++) {
      flushed += flush_memtable(i);
    }
    return flushed;
  }

private:
  size_t _n_shards;
  size_t _mem_usage;
  size_t _mem_high_water_mark;
  size_t _top_k;
  std::string _input_file;
  std::unique_ptr<memtable_type[]> _memtables;
  std::unique_ptr<size_t[]> _mem_usage_per_table;
  std::unique_ptr<size_t[]> _epochs;
  std::vector<std::thread> _worker_threads;

  std::mutex _result_mtx;
  heap_type _result;

  size_t flush_memtable(size_t shard);

  size_t current_mem_usage();
  void on_new_url(std::string_view url);
  static size_t estimate_map_entry_mem_usage(std::string_view url);
};