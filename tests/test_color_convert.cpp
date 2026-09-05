#include "encode/ColorConvert.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("convertBgraToNv12 writes BT.601 limited-range luma")
{
    constexpr int kWidth = 2;
    constexpr int kHeight = 2;
    std::vector<std::uint8_t> bgra(static_cast<std::size_t>(kWidth * kHeight * 4), 0);
    // Opaque red.
    for (int i = 0; i < kWidth * kHeight; ++i) {
        bgra[static_cast<std::size_t>(i) * 4 + 2] = 255;
        bgra[static_cast<std::size_t>(i) * 4 + 3] = 255;
    }

    std::vector<std::uint8_t> nv12(static_cast<std::size_t>(kWidth * kHeight * 3 / 2), 0);
    ors::convertBgraToNv12(bgra.data(), kWidth * 4, kWidth, kHeight, nv12.data());

    REQUIRE(nv12[0] == 82);
    REQUIRE(nv12[1] == 82);
    REQUIRE(nv12[2] == 82);
    REQUIRE(nv12[3] == 82);
}
