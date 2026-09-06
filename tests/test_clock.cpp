#include "core/Clock.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sampleDurationNs uses the gap when it is at least 1 ms")
{
    REQUIRE(ors::sampleDurationNs(1'000'000, 17'000'000, 16'666'666) == 16'000'000);
}

TEST_CASE("sampleDurationNs falls back when the gap is backwards or tiny")
{
    REQUIRE(ors::sampleDurationNs(10'000'000, 10'500'000, 16'666'666) == 16'666'666);
    REQUIRE(ors::sampleDurationNs(10'000'000, 8'000'000, 16'666'666) == 16'666'666);
}

TEST_CASE("monotonicTimestampNs keeps later timestamps")
{
    REQUIRE(ors::monotonicTimestampNs(10, 25, 1'000'000) == 25);
}

TEST_CASE("monotonicTimestampNs clamps backwards or duplicate timestamps")
{
    REQUIRE(ors::monotonicTimestampNs(10'000'000, 8'000'000, 1'000'000) == 11'000'000);
    REQUIRE(ors::monotonicTimestampNs(10'000'000, 10'000'000, 1'000'000) == 11'000'000);
}
