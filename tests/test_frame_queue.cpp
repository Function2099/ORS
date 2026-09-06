#include "core/FrameQueue.h"
#include "core/Clock.h"
#include "core/VideoFrame.h"

#include <chrono>
#include <utility>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("FrameQueue push and pop preserve order")
{
    ors::FrameQueue<ors::VideoFrame> queue(3);

    ors::VideoFrame a;
    a.width = 1;
    ors::VideoFrame b;
    b.width = 2;
    queue.push(std::move(a));
    queue.push(std::move(b));

    REQUIRE(queue.size() == 2);
    REQUIRE(queue.capacity() == 3);
    REQUIRE(queue.dropped() == 0);

    ors::VideoFrame out;
    REQUIRE(queue.pop(out));
    REQUIRE(out.width == 1);
    REQUIRE(queue.pop(out));
    REQUIRE(out.width == 2);
    REQUIRE_FALSE(queue.pop(out));
}

TEST_CASE("FrameQueue drops oldest when full")
{
    ors::FrameQueue<ors::VideoFrame> queue(2);

    for (int width = 1; width <= 3; ++width) {
        ors::VideoFrame frame;
        frame.width = width;
        queue.push(std::move(frame));
    }

    REQUIRE(queue.size() == 2);
    REQUIRE(queue.dropped() == 1);

    ors::VideoFrame out;
    REQUIRE(queue.pop(out));
    REQUIRE(out.width == 2);
    REQUIRE(queue.pop(out));
    REQUIRE(out.width == 3);
}

TEST_CASE("FrameQueue waitPop times out on empty queue")
{
    ors::FrameQueue<int> queue(1);
    int value = -1;
    REQUIRE_FALSE(queue.waitPop(value, std::chrono::milliseconds(5)));
    REQUIRE(value == -1);

    queue.push(7);
    REQUIRE(queue.waitPop(value, std::chrono::milliseconds(5)));
    REQUIRE(value == 7);
}

TEST_CASE("FrameQueue clear empties without resetting dropped count")
{
    ors::FrameQueue<int> queue(1);
    queue.push(1);
    queue.push(2);
    REQUIRE(queue.dropped() == 1);
    queue.clear();
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.dropped() == 1);
}

TEST_CASE("sampleDurationNs uses the gap to the next frame")
{
    constexpr std::int64_t fallback = 16'666'667;
    REQUIRE(ors::sampleDurationNs(0, 50'000'000, fallback) == 50'000'000);
    REQUIRE(ors::sampleDurationNs(10'000'000, 10'500'000, fallback) == fallback);
    REQUIRE(ors::sampleDurationNs(20'000'000, 10'000'000, fallback) == fallback);
}
