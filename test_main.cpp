#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE( "1 == 1", "[basic arithmetic]" ) {
    REQUIRE( 1 == 1 );
}