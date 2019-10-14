#include <catch2/catch.hpp>
#include <iostream>
#include <stdint.h>
#include "memusage_allocator.h"

size_t get_entry_overhead();

TEST_CASE("memusage_allocator", "[spec]") {
    SECTION("should report correct size") {
        REQUIRE(get_entry_overhead() > 0);
    }
}