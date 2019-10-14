#pragma once
#include <stddef.h>

static size_t memory_usage;

template <typename T> 
struct memusage_allocator {
public:
    typedef T value_type;
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;


    template <class U> struct rebind {
        typedef memusage_allocator<U> other;
    };

    pointer address(reference value) const {
        return &value;
    }
    const_pointer address(const_reference value) const {
        return &value;
    }

    memusage_allocator() : base() {}
    memusage_allocator(const memusage_allocator &) : base() {}
    template <typename U>
    memusage_allocator(const memusage_allocator<U> &) : base() {}
    ~memusage_allocator() {}

    // return maximum number of elements that can be allocated
    size_type max_size() const throw() {
        return base.max_size();
    }

    pointer allocate(size_type num, const void * p = 0) {
        memory_usage += num * sizeof(T);
        return base.allocate(num,p);
    }

    void construct(pointer p, const T &value) {
        return base.construct(p,value);
    }

    // destroy elements of initialized storage p
    void destroy(pointer p) {
        base.destroy(p);
    }

    // deallocate storage p of deleted elements
    void deallocate(pointer p, size_type num ) {
        memory_usage -= num * sizeof(T);
        base.deallocate(p,num);
    }
    std::allocator<T> base;
};

// for our purposes, we don't want to distinguish between allocators.
template <class T1, class T2>
bool operator==(const memusage_allocator<T1> &, const T2 &) throw() {
    return true;
}

template <class T1, class T2>
bool operator!=(const memusage_allocator<T1> &, const T2 &) throw() {
    return false;
}

struct memusage_measure_guard {
    memusage_measure_guard() {
        memory_usage = 0;
    }

    size_t current_usage() {
        return memory_usage;
    }

    ~memusage_measure_guard() {
        memory_usage = 0;
    }
};