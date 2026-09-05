#include "audio/AudioMix.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

TEST_CASE("mono float is duplicated to both stereo channels")
{
    const float src[] = {0.5f, -0.25f};
    std::vector<std::uint8_t> out;
    ors::convertInterleavedToStereoS16(
        reinterpret_cast<const std::uint8_t*>(src),
        2,
        1,
        32,
        true,
        false,
        ors::MicInputSource::Stereo,
        out);
    REQUIRE(out.size() == 8);
    const auto* dst = reinterpret_cast<const std::int16_t*>(out.data());
    REQUIRE(dst[0] == dst[1]);
    REQUIRE(dst[2] == dst[3]);
    REQUIRE(dst[0] > 0);
    REQUIRE(dst[2] < 0);
}

TEST_CASE("left input source copies channel 0 onto both outputs")
{
    const std::int16_t src[] = {1000, 2000, 3000, 4000};
    std::vector<std::uint8_t> out;
    ors::convertInterleavedToStereoS16(
        reinterpret_cast<const std::uint8_t*>(src),
        2,
        2,
        16,
        false,
        false,
        ors::MicInputSource::Left,
        out);
    const auto* dst = reinterpret_cast<const std::int16_t*>(out.data());
    REQUIRE(dst[0] == dst[1]);
    REQUIRE(dst[2] == dst[3]);
}

TEST_CASE("right input source copies channel 1 onto both outputs")
{
    std::int16_t stereo[] = {1000, 2000, 3000, 4000};
    ors::applyMicInputSource(stereo, 2, ors::MicInputSource::Right);
    REQUIRE(stereo[0] == 2000);
    REQUIRE(stereo[1] == 2000);
    REQUIRE(stereo[2] == 4000);
    REQUIRE(stereo[3] == 4000);
}

TEST_CASE("stereo input source leaves channels unchanged")
{
    std::int16_t stereo[] = {1000, 2000};
    ors::applyMicInputSource(stereo, 1, ors::MicInputSource::Stereo);
    REQUIRE(stereo[0] == 1000);
    REQUIRE(stereo[1] == 2000);
}

TEST_CASE("mixStereoS16 saturates instead of wrapping")
{
    std::int16_t dst[] = {30000, -30000};
    const std::int16_t src[] = {30000, -30000};
    ors::mixStereoS16(dst, src, 2);
    REQUIRE(dst[0] == 32767);
    REQUIRE(dst[1] == -32768);
}

TEST_CASE("linear resample doubles stereo frame count at 2x rate")
{
    const std::int16_t src[] = {0, 0, 1000, 1000};
    std::vector<std::int16_t> dst;
    ors::resampleStereoS16Linear(src, 2, 24000, 48000, dst);
    REQUIRE(dst.size() == 8);
    REQUIRE(dst.front() == 0);
    REQUIRE(dst[dst.size() - 2] == 1000);
}

TEST_CASE("micInputSourceFromId parses left right and default stereo")
{
    REQUIRE(ors::micInputSourceFromId("left") == ors::MicInputSource::Left);
    REQUIRE(ors::micInputSourceFromId("right") == ors::MicInputSource::Right);
    REQUIRE(ors::micInputSourceFromId("stereo") == ors::MicInputSource::Stereo);
    REQUIRE(ors::micInputSourceFromId("nope") == ors::MicInputSource::Stereo);
}
