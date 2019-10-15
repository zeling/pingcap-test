#include <catch2/catch.hpp>

#include "iterator.h"
#include "master.h"

template <typename Container> struct container_iterator : Container::iterator {
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
    vs_t v1 = {{"aba", 1}, {"bbb", 2}, {"cac", 3}};
    vs_t v2 = {{"aaa", 1}, {"bab", 2}, {"ccc", 3}};
    vs_t v3 = {{"aca", 1}, {"bcb", 2}, {"cbc", 3}};

    iter_t iters[3] = {{v1, v1.begin()}, {v2, v2.begin()}, {v3, v3.begin()}};
    merge_iter<iter_t> miter(iters, iters + 3);

    vs_t result,
        expected = {{"aaa", 1}, {"aba", 1}, {"aca", 1}, {"bab", 2}, {"bbb", 2},
                    {"bcb", 2}, {"cac", 3}, {"cbc", 3}, {"ccc", 3}};

    while (miter.valid()) {
      result.push_back(*miter);
      ++miter;
    }

    REQUIRE(result == expected);
  }
}

TEST_CASE("read_line_iter", "[read_line_iter spec]") {
  FILE *input = fopen("../read_line_iter_test.txt", "r");
  REQUIRE(input != NULL);
  read_line_iter iter(input);
  std::vector<std::string> result,
      expected = {"a", "b", "c", "d", "e", "f", "g"};
  while (iter.valid()) {
    result.emplace_back(*iter);
    ++iter;
  }
  REQUIRE(result == expected);
}

void write_sst(master::memtable_type memtable, FILE *output);
TEST_CASE("sst", "[sst spec]") {
  master::memtable_type result, expected = {
                                    {"abc", 1},
                                    {"def", 3},
                                    {"ghi", 2},
                                };
  FILE *sst = fopen("test-sst.sst", "w+b");
  REQUIRE(sst != NULL);
  write_sst(expected, sst);
  REQUIRE(fclose(sst) == 0);
  sst = fopen("test-sst.sst", "rb");
  REQUIRE(sst != NULL);
  sst_read_iter iter(sst);
  while (iter.valid()) {
    result.insert({std::string(iter->url), iter->count});
    ++iter;
  }
  REQUIRE(result == expected);
  REQUIRE(fclose(sst) == 0);
  REQUIRE(unlink("test-sst.sst") == 0);
}

TEST_CASE("merge sst", "[merge sst]") {
  master::memtable_type result,
      memtables[3] = {{
                          {"abc", 1},
                          {"def", 3},
                          {"ghi", 2},
                      },
                      {
                          {"aac", 3},
                          {"bec", 4},
                          {"jkl", 5},
                          {"mno", 6},
                      },
                      // Empty
                      {}},
      expected = {
          {"aac", 3}, {"abc", 1}, {"bec", 4}, {"def", 3},
          {"ghi", 2}, {"jkl", 5}, {"mno", 6},
      };
  std::string files[3];
  for (int i = 0; i < 3; i++) {
    files[i] = "test_merge_sst-" + std::to_string(i) + ".sst";
    FILE *out = fopen(files[i].c_str(), "w+b");
    REQUIRE(out != NULL);
    write_sst(memtables[i], out);
    REQUIRE(fclose(out) == 0);
  }
  std::vector<sst_read_iter> iters;
  for (int i = 0; i < 3; i++) {
    FILE *out = fopen(files[i].c_str(), "rb");
    REQUIRE(out != NULL);
    iters.emplace_back(out);
  }

  merge_iter<sst_read_iter> miter(iters.begin(), iters.end());
  while (miter.valid()) {
    result.insert({owned_url_t(miter->url), miter->count});
    ++miter;
  }

  REQUIRE(result == expected);

  for (int i = 0; i < 3; i++) {
    iters[i].close();
    REQUIRE(unlink(files[i].c_str()) == 0);
  }
}

TEST_CASE("merge sst eqaul", "[merge sst spec]") {
  master::memtable_type result,
      memtables[3] = {{
                          {"abc", 1},
                          {"def", 3},
                          {"ghi", 2},
                      },
                      {
                          {"abc", 3},
                          {"bec", 4},
                          {"def", 5},
                          {"mno", 6},
                      },
                      {{"def", 3}}},
      expected = {
          {"abc", 4}, {"bec", 4}, {"def", 11}, {"ghi", 2}, {"mno", 6},
      };
  std::string files[3];
  for (int i = 0; i < 3; i++) {
    files[i] = "test_merge_sst-" + std::to_string(i) + ".sst";
    FILE *out = fopen(files[i].c_str(), "w+b");
    REQUIRE(out != NULL);
    write_sst(memtables[i], out);
    REQUIRE(fclose(out) == 0);
  }
  std::vector<sst_read_iter> iters;
  for (int i = 0; i < 3; i++) {
    FILE *out = fopen(files[i].c_str(), "rb");
    REQUIRE(out != NULL);
    iters.emplace_back(out);
  }

  merge_iter<sst_read_iter> miter(iters.begin(), iters.end());
  owned_url_t last = "";
  count_t last_count = 0;
  while (miter.valid()) {
    if (miter->url != last) {
      if (last != "") {
        result.insert({owned_url_t(last), last_count});
      }
      last = miter->url;
      last_count = miter->count;
    } else {
      last_count += miter->count;
    }
    ++miter;
  }

  if (last != "") {
    result.insert({owned_url_t(last), last_count});
  }

  REQUIRE(result == expected);

  for (int i = 0; i < 3; i++) {
    iters[i].close();
    REQUIRE(unlink(files[i].c_str()) == 0);
  }
}