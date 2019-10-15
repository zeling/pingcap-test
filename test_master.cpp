#include <catch2/catch.hpp>
#include <stdio.h>

#include "master.h"

TEST_CASE("master", "[master spec]") {
  master::memtable_type result,
      source =
          {
              {"abc", 4}, {"bec", 4}, {"def", 11}, {"ghi", 2}, {"mno", 6},
          },
      expected = {
          {"abc", 4},
          {"bec", 4},
          {"def", 11},
          {"mno", 6},
      };

  std::vector<owned_url_t> urls;
  for (const auto &e : source) {
    for (int i = 0; i < e.second; i++) {
      urls.push_back(e.first);
    }
  }

  std::random_shuffle(urls.begin(), urls.end());

  char buf[] = "test-master-XXXXXX";
  int fd = mkstemp(buf);
  REQUIRE(fd != -1);
  FILE *output = fdopen(fd, "w+");
  for (auto &url : urls) {
    fprintf(output, "%s\n", url.c_str());
  }
  fflush(output);
  master m(buf, 4, 72, 4);
  m.start();
  m.wait_for_all_workers();
  REQUIRE(unlink(buf) == 0);
  for (auto it = m.result_begin(); it != m.result_end(); ++it) {
    result.insert({it->url, it->count});
  }
  REQUIRE(result == expected);
}