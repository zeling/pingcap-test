#pragma once
#include <iterator>
#include <string_view>
#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#include "entry.h"
#include "heap.h"

// Base class for input file
template <typename T, typename Derived>
class input_file_iter {
public:
    static constexpr size_t buf_size = 1024;
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;

    input_file_iter(FILE *input)
        :_input(input) {
        if (valid()) {
            read_buffer();
        }
    }

    value_type operator*() {
        if (valid()) {
            return construct_entry();
        } else {
            return {};
        }
    } 

    value_type *operator->() {
        new (&_e) value_type(construct_entry());
        return &_e;
    }

    Derived &operator++() {
        if (_input && !feof(_input)) {
            read_buffer();
        } else {
            _cur_len = 0;
        }
        return *static_cast<Derived *>(this);
    }

    void read_buffer() {
        _cur_len = static_cast<Derived *>(this)->read_buffer(_buf, buf_size, _input);
    }

    value_type construct_entry() {
        return static_cast<Derived *>(this)->construct_entry(_buf, _cur_len);
    }

    bool valid() {
        return _cur_len > 0 || (_input && !feof(_input));
    }

    void close() {
        assert(_input);
        assert(fclose(_input) == 0);
        _input = nullptr;
    }

private:
    FILE *_input;
    char _buf[buf_size];
    size_t _cur_len = 0;
    value_type _e;
};

class sst_read_iter: public input_file_iter<entry<slice_url_t>, sst_read_iter> {
public:
    sst_read_iter(FILE *input)
        : input_file_iter(input) {}

    size_t read_buffer(char *buf, size_t size, FILE *input) {
        size_t key_len;
        if (fread(&key_len, sizeof(key_len), 1, input) != 1) {
            return 0;
        }
        if (fread(buf, 1, key_len, input) != key_len) {
            return 0;
        }
        if (fread(&_count, sizeof(_count), 1, input) != 1) {
            return 0;
        }
        return key_len;
    }

    value_type construct_entry(char *buf, size_t size) {
        return { slice_url_t(buf, size), _count };
    }

private:
    count_t _count;
};

class read_line_iter : public input_file_iter<slice_url_t, read_line_iter> {
public:
    read_line_iter(FILE *input)
        : input_file_iter(input) {}
    
    size_t read_buffer(char *buf, size_t size, FILE *input) {
        do {
            if (!fgets(buf, buf_size, input)) {
                return 0;
            }
        } while (strncmp("\n", buf, 1) == 0);
        return strlen(buf);
    }

    value_type construct_entry(char *buf, size_t size) {
        if (size > 1 && buf[size-1] == '\n') {
            --size;
        }
        return { buf, size };
    }
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
            if (iter.valid()) {
                _heap.add(&iter);
            }
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

    value_type operator*() {
        return _heap.front().iter->operator*();
    }

    const value_type *operator->() {
        return _heap.front().iter->operator->();
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
