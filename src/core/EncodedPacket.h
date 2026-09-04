#pragma once

#include <cstdint>
#include <vector>

namespace ors {

enum class PacketKind {
    Video,
    Audio,
};

struct EncodedPacket {
    PacketKind kind{PacketKind::Video};
    std::int64_t timestampNs{};
    std::int64_t durationNs{};
    bool keyframe{};
    std::vector<std::uint8_t> bytes;
};

} // namespace ors
