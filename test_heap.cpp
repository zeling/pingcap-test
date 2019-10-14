#include <catch2/catch.hpp>
#define private public
#include "heap.h"
#undef private

TEST_CASE("min_heap", "[minheap spec]") {
    struct entry {
        entry(uint64_t c): count(c) {}
        uint64_t count;
        bool operator<(const entry &rhs) {
            return count > rhs.count;
        }
    };
    heap<entry> h(10);

    SECTION("should never grow beyond size limit") {
        for (size_t i = 1; i <= 10; i++) {
            h.add(i);
            REQUIRE(h._heap.size() == i);
        }

        for (size_t i = 10; i < 100; i++) {
            h.add(i);
            REQUIRE(h._heap.size() == 10);
        }
    }

    SECTION("should give correct result") {
        for (size_t i = 1; i <= 100; i++) {
            h.add(i);
        }
        std::sort(h._heap.begin(), h._heap.end());
        for (size_t i = 0; i < h._heap.size(); i++) {
            REQUIRE(h._heap[i].count == (100 - i));
        }
    }

    SECTION("should give correct result when input is reversed") {
        for (size_t i = 100; i >= 1; i--) {
            h.add(i);
        }
        std::sort(h._heap.begin(), h._heap.end());
        for (size_t i = 0; i < h._heap.size(); i++) {
            REQUIRE(h._heap[i].count == (100 - i));
        }
    }


    SECTION("poll is correct") {
        for (size_t i = 100; i >= 1; i--) {
            h.add(i);
        }
        for (int i = 0; i < 10; i++) {
            REQUIRE(h.poll().count == 91 + i);
        }
    }
}

TEST_CASE("max_heap", "max_heap spec") {
    heap<uint64_t> h(10);

    SECTION("should never grow beyond size limit") {
        for (size_t i = 1; i <= 10; i++) {
            h.add(i);
            REQUIRE(h._heap.size() == i);
        }

        for (size_t i = 10; i < 100; i++) {
            h.add(i);
            REQUIRE(h._heap.size() == 10);
        }
    }

    SECTION("should give correct result") {
        for (size_t i = 0; i < 100; i++) {
            h.add(i);
        }
        std::sort(h._heap.begin(), h._heap.end());
        for (size_t i = 0; i < h._heap.size(); i++) {
            REQUIRE(h._heap[i] == i);
        }
    }

    SECTION("should give correct result when input is reversed") {
        for (int i = 99; i >= 0; i--) {
            h.add(i);
        }
        std::sort(h._heap.begin(), h._heap.end());
        for (size_t i = 0; i < h._heap.size(); i++) {
            REQUIRE(h._heap[i] == i);
        }

    }


    SECTION("poll is correct") {
        for (size_t i = 0; i < 100; i++) {
            h.add(i);
        }
        for (int i = 0; i < 10; i++) {
            REQUIRE(h.poll() == (9 - i));
        }
    }
}