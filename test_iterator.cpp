#include <catch2/catch.hpp>
#include "iterator.h"

template <typename Container>
struct container_iterator: Container::iterator {
    Container &cont;

    container_iterator(Container &c, typename Container::iterator iter)
        : cont(c), Container::iterator(iter) {}

    bool valid() {
        return (*static_cast<typename Container::iterator *>(this)) != cont.end();
    }
};

TEST_CASE("container_iterator", "[container_iterator spec]") {
    std::vector<int> v = {1, 2, 3, 4};
    container_iterator<std::vector<int>> begin = {v, v.begin()};
    container_iterator<std::vector<int>> end = {v, v.end()};
    REQUIRE(begin.valid());
    REQUIRE(!end.valid());

    std::vector<int> copied(begin, end);
    REQUIRE(v == copied);
}

TEST_CASE("merge_iterator", "[merge iterator spec]") {

    SECTION("should work with even iterators") {
        using vs_t = std::vector<entry<owned_url_t>>;
        using iter_t = container_iterator<vs_t>;
        vs_t v1 = { {"aba", 1 }, {"bbb", 2 }, {"cac", 3 } };
        vs_t v2 = { {"aaa", 1 }, {"bab", 2 }, {"ccc", 3 } };
        vs_t v3 = { {"aca", 1 }, {"bcb", 2 }, {"cbc", 3 } };

        iter_t iters[3] = { {v1, v1.begin()}, {v2, v2.begin()}, {v3, v3.begin()} };
        merge_iter<iter_t> miter(iters, iters + 3);

        vs_t result, expected = {
            {"aaa", 1}, {"aba", 1}, {"aca", 1}, 
            {"bab", 2}, {"bbb", 2}, {"bcb", 2}, 
            {"cac", 3}, {"cbc", 3}, {"ccc", 3}
        };

        while (miter.valid()) {
            result.push_back(*miter);
            ++miter;
        }

        REQUIRE(result == expected);
    }
}