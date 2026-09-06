#pragma once

#include <chrono>
#include <cstdint>

namespace ors {

inline std::int64_t nowNs()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// Duration of a held VFR/CFR sample: time until the next unique frame.
// Deltas under 1 ms are treated as duplicates and fall back to 1/fps.
inline std::int64_t sampleDurationNs(
    std::int64_t previousNs,
    std::int64_t nextNs,
    std::int64_t fallbackNs)
{
    const std::int64_t delta = nextNs - previousNs;
    const std::int64_t fallback = fallbackNs > 0 ? fallbackNs : 1;
    return delta >= 1'000'000 ? delta : fallback;
}

// Keep mux timestamps moving forward when capture clocks jump backwards.
inline std::int64_t monotonicTimestampNs(
    std::int64_t previousNs,
    std::int64_t nextNs,
    std::int64_t minDeltaNs)
{
    const std::int64_t minDelta = minDeltaNs > 0 ? minDeltaNs : 1;
    return nextNs > previousNs ? nextNs : previousNs + minDelta;
}

// Subtract accumulated pause time so the mux timeline skips pauses.
inline std::int64_t adjustPausedTimestampNs(std::int64_t timestampNs, std::int64_t pauseOffsetNs)
{
    if (pauseOffsetNs <= 0) {
        return timestampNs;
    }
    return timestampNs > pauseOffsetNs ? timestampNs - pauseOffsetNs : 0;
}

} // namespace ors
