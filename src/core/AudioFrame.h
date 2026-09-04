#pragma once

#include <cstdint>
#include <vector>

namespace ors {

struct AudioFrame {
    std::int64_t timestampNs{};
    int sampleRate{48000};
    int channels{2};
    int bitsPerSample{16};
    std::vector<std::uint8_t> bytes;
};

} // namespace ors
