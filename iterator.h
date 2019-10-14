#pragma once
#include <iterator>
#include <string_view>
#include <string>
#include <vector>

#include <stdio.h>

#include "entry.h"
#include "heap.h"

class sst_write_iter {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = entry<slice_url_t>;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

};

class sst_read_iter {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = entry<slice_url_t>;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;
};

class read_line_iter {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::string_view;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;
};

namespace {
    template <typename Iter>
    using iter_value_t = typename std::iterator_traits<Iter>::value_type;
}

template <typename Iter, 
        std::enable_if_t<is_entry<iter_value_t<Iter>>::value, void *> = nullptr>
class merge_iter {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = iter_value_t<Iter>;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

    template <typename Iters, std::enable_if_t<std::is_same_v<iter_value_t<Iters>, Iter>, void *> = nullptr>
    merge_iter(Iters begin, Iters end)
        :_iters(begin, end), _heap(end - begin) {
        for (auto &iter: _iters) {
            _heap.add(&iter);
        }
    }

    merge_iter &operator++() {
        auto first = _heap.poll();
        first.iter->operator++();
        if (first.iter->valid()) {
            _heap.add(first);
        }
        return *this;
    }

    const value_type &operator*() {
        return _heap.front().iter->operator*();
    }

    bool valid() {
        return !_heap.empty();
    }

private:
    struct iter_p {
        Iter *iter;
        iter_p(Iter *i): iter(i) {}
        bool operator<(const iter_p &rhs) {
            return (*iter)->url > (*rhs.iter)->url;
        }
    };
    heap<iter_p> _heap;
    std::vector<Iter> _iters;
};
