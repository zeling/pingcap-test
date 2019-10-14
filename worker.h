#pragma once
#include <string>
#include <map>
#include <stddef.h>
#include <stdio.h>
#include <memory>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <thread>
#include <vector>
#include <string_view>
#include <iterator>

class master {
public:
    using key_type = std::string;
    using value_type = uint64_t;
    /* Use the transparent compare */
    using memtable_type = std::map<key_type, value_type, std::less<>>;

    master(std::string input, size_t n_shards, size_t mem_limit)
        : _n_shards(n_shards)
        , _mem_usage(0)
        , _input_file(std::move(input))
        , _memtables(std::make_unique<memtable_type[]>(n_shards))
        , _mem_usage_per_table(std::make_unique<size_t[]>(n_shards))
        , _epochs(std::make_unique<size_t[]>(n_shards))
    {}

    ~master() {
        /* Make sure when the master is dropped, all threads are waited */
        wait_for_all_workers();
    }

    template<typename F>
    void spawn_workers(size_t n_workers, F &&f) {
        for (size_t w = 0; w < n_workers; w++) {
            _worker_threads.emplace_back(std::forward<F>(f));
        }
    }

    void wait_for_all_workers() {
        for (auto &&t: _worker_threads) {
            t.join();
        }
    }

    void start();

private:
    size_t _n_shards;
    size_t _mem_usage;
    size_t _mem_limit;
    std::string _input_file;
    std::unique_ptr<memtable_type[]> _memtables;
    std::unique_ptr<size_t[]> _mem_usage_per_table;
    std::unique_ptr<size_t[]> _epochs;
    std::vector<std::thread> _worker_threads;

    int flush_memtable(size_t shard);
    size_t current_mem_usage();
    void on_new_url(std::string_view url);
    size_t estimate_map_entry_mem_usage(std::string_view url);
};

