#include <catch2/catch.hpp>
#include <setjmp.h>
#include <stdio.h>

#include "memusage_guard.h"

jmp_buf jbuf;

void new_handler() {
    longjmp(jbuf, 1);
}

constexpr rlim_t GB = 1'024 * 1'024 * 1'024;

TEST_CASE("memusage_guard", "[memusage_guad spec]") {
    SECTION("should be able to guard") {
        memusage_guard g(1 * GB, new_handler);
        if (setjmp(jbuf) == 0) {
            char *buf = new char[2 * GB];
            REQUIRE_FALSE(true);
        } else {
            REQUIRE_FALSE(false);
        }
    }
}