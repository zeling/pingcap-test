#include <catch2/catch.hpp>
#include <setjmp.h>

#include "memusage_guard.h"

jmp_buf jbuf;

void new_handler() { longjmp(jbuf, 1); }

constexpr rlim_t GB = 1'024 * 1'024 * 1'024;

/* The following test will fail on WSL ... */
TEST_CASE("memusage_guard", "[memusage_guad spec][!mayfail]") {
  SECTION("should be able to guard") {
    {
      memusage_guard g(1 * GB, new_handler);
      if (setjmp(jbuf) == 0) {
        char *buf = new char[1 * GB];
        // Absurd.
        REQUIRE(1 == 2);
      }
    }
    char *buf;
    REQUIRE_NOTHROW(buf = new char[1 * GB]);
    REQUIRE(buf != nullptr);
    delete[] buf;
  }
}