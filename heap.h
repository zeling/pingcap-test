#pragma once
#include <algorithm>
#include <assert.h>
#include <string>
#include <vector>

#include "types.h"

template <typename Entry> class heap {
public:
  using iterator = typename std::vector<Entry>::iterator;

  heap(size_t limit) : _limit(limit) { assert(limit != 0); }

  template <typename Iter>
  heap(Iter begin, Iter end) : _limit(end - begin), _heap(begin, end) {
    std::make_heap(_heap.begin(), _heap.end());
  }

  template <typename... T> void add(T &&... args) {
    if (_heap.size() < _limit) {
      _heap.emplace_back(std::forward<T>(args)...);
      std::push_heap(_heap.begin(), _heap.end());
    } else {
      Entry e(std::forward<T>(args)...);
      if (e < _heap.front()) {
        std::pop_heap(_heap.begin(), _heap.end());
        _heap.back() = std::move(e);
        std::push_heap(_heap.begin(), _heap.end());
      }
    }
  }

  Entry poll() {
    assert(!_heap.empty());
    std::pop_heap(_heap.begin(), _heap.end());
    auto ret = std::move(_heap.back());
    _heap.pop_back();
    return ret;
  }

  std::vector<Entry> get_sorted() {
    std::sort(_heap.begin(), _heap.end());
    return std::move(_heap);
  }

  iterator begin() { return _heap.begin(); }

  iterator end() { return _heap.end(); }

  bool empty() { return _heap.empty(); }

  size_t size() { return _heap.size(); }

  Entry &front() { return _heap.front(); }

private:
  size_t _limit;
  std::vector<Entry> _heap;
};
