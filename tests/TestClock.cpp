#include <catch2/catch_test_macros.hpp>
#include "../src/Clock.hpp"

#include <fmt/core.h>
#include <memory>

TEST_CASE ( "Clock", "[Clock]" )
{
    Clock c{ std::make_shared<int>(1) };

    c.set_clock(9, 1);

    REQUIRE ( c.pts == 9 );
    REQUIRE ( c.serial == 1 );

    c.set_clock_at(10, 1, 2);

    REQUIRE ( c.pts == 10 );
    REQUIRE ( c.serial == 1 );
    REQUIRE ( c.last_updated == 2 );

    Clock other{ std::make_shared<int>(2) };

    other.set_clock_at(15, 30, 5);

    c.sync_clock_to_slave(other);

    REQUIRE ( c.pts == 10 );
    REQUIRE ( c.serial == 1 );
    REQUIRE ( c.last_updated == 2.0 );

    SUCCEED ( "OK" );
}
