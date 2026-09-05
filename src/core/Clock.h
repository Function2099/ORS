#pragma once

#include <chrono>
#include <cstdint>

namespace ors {

inline std::int64_t nowNs()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace ors
