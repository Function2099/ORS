#include "core/EncodedPacket.h"
#include "mux/GifEncoder.h"
#include "mux/IMuxer.h"

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

ors::EncodedPacket makeBgraFrame(int width, int height, std::uint8_t b, std::uint8_t g, std::uint8_t r)
{
    ors::EncodedPacket packet;
    packet.kind = ors::PacketKind::Video;
    packet.bytes.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    for (int i = 0; i < width * height; ++i) {
        packet.bytes[static_cast<std::size_t>(i) * 4u + 0] = b;
        packet.bytes[static_cast<std::size_t>(i) * 4u + 1] = g;
        packet.bytes[static_cast<std::size_t>(i) * 4u + 2] = r;
        packet.bytes[static_cast<std::size_t>(i) * 4u + 3] = 255;
    }
    return packet;
}

ors::EncodedPacket makeSplitFrame(int width, int height)
{
    ors::EncodedPacket packet;
    packet.kind = ors::PacketKind::Video;
    packet.bytes.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int i = y * width + x;
            const bool left = x < width / 2;
            packet.bytes[static_cast<std::size_t>(i) * 4u + 0] = left ? 0 : 255;
            packet.bytes[static_cast<std::size_t>(i) * 4u + 1] = 0;
            packet.bytes[static_cast<std::size_t>(i) * 4u + 2] = left ? 255 : 0;
            packet.bytes[static_cast<std::size_t>(i) * 4u + 3] = 255;
        }
    }
    return packet;
}

std::uint16_t readU16(const QByteArray& bytes, int offset)
{
    return static_cast<std::uint8_t>(bytes.at(offset))
        | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes.at(offset + 1))) << 8);
}

class BitReader {
public:
    explicit BitReader(QByteArray data)
        : data_(std::move(data))
    {}

    int read(int bits)
    {
        int value = 0;
        int shift = 0;
        while (shift < bits) {
            if (bitCount_ == 0) {
                if (offset_ >= data_.size()) {
                    return -1;
                }
                buffer_ = static_cast<std::uint8_t>(data_.at(offset_++));
                bitCount_ = 8;
            }
            const int take = std::min(bits - shift, bitCount_);
            value |= (buffer_ & ((1 << take) - 1)) << shift;
            buffer_ >>= take;
            bitCount_ -= take;
            shift += take;
        }
        return value;
    }

private:
    QByteArray data_;
    int offset_{};
    int buffer_{};
    int bitCount_{};
};

QByteArray concatSubBlocks(const QByteArray& bytes, int& offset)
{
    QByteArray packed;
    while (offset < bytes.size()) {
        const int chunk = static_cast<std::uint8_t>(bytes.at(offset++));
        if (chunk == 0) {
            break;
        }
        REQUIRE(offset + chunk <= bytes.size());
        packed.append(bytes.mid(offset, chunk));
        offset += chunk;
    }
    return packed;
}

std::vector<std::uint8_t> decodeLzw(const QByteArray& packed, int minCodeSize, int pixelCount)
{
    const int clear = 1 << minCodeSize;
    const int eoi = clear + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = eoi + 1;
    std::vector<std::vector<std::uint8_t>> table(static_cast<std::size_t>(4096));
    auto resetTable = [&] {
        table.assign(4096, {});
        for (int i = 0; i < clear; ++i) {
            table[static_cast<std::size_t>(i)] = {static_cast<std::uint8_t>(i)};
        }
        codeSize = minCodeSize + 1;
        nextCode = eoi + 1;
    };
    resetTable();

    BitReader reader(packed);
    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(pixelCount));
    int previous = -1;
    while (static_cast<int>(out.size()) < pixelCount) {
        const int code = reader.read(codeSize);
        if (code < 0 || code == eoi) {
            break;
        }
        if (code == clear) {
            resetTable();
            previous = -1;
            continue;
        }
        std::vector<std::uint8_t> entry;
        if (code < nextCode && !table[static_cast<std::size_t>(code)].empty()) {
            entry = table[static_cast<std::size_t>(code)];
        } else if (code == nextCode && previous >= 0) {
            entry = table[static_cast<std::size_t>(previous)];
            entry.push_back(entry.front());
        } else {
            FAIL("invalid LZW code");
        }
        out.insert(out.end(), entry.begin(), entry.end());
        if (previous >= 0 && nextCode < 4096) {
            auto added = table[static_cast<std::size_t>(previous)];
            added.push_back(entry.front());
            table[static_cast<std::size_t>(nextCode)] = std::move(added);
            ++nextCode;
            if (nextCode == (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
        }
        previous = code;
    }
    return out;
}

int skipExtensions(const QByteArray& bytes, int offset)
{
    while (offset < bytes.size() && static_cast<std::uint8_t>(bytes.at(offset)) == 0x21) {
        ++offset;
        REQUIRE(offset < bytes.size());
        ++offset; // label
        while (offset < bytes.size()) {
            const int chunk = static_cast<std::uint8_t>(bytes.at(offset++));
            if (chunk == 0) {
                break;
            }
            offset += chunk;
        }
    }
    return offset;
}

} // namespace

TEST_CASE("GifEncoder writes GIF89a header size and trailer")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("solid.gif"));

    ors::GifEncoder encoder;
    ors::MuxerOpenParams params;
    params.filePath = path;
    params.videoWidth = 8;
    params.videoHeight = 6;
    params.videoFrameRate = 10;
    REQUIRE(encoder.open(params));
    REQUIRE(encoder.writeVideo(makeBgraFrame(8, 6, 0, 0, 255)));
    REQUIRE(encoder.writeVideo(makeBgraFrame(8, 6, 0, 255, 0)));
    REQUIRE(encoder.writeAudio({}));
    REQUIRE(encoder.finalize());

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    REQUIRE(bytes.size() >= 14);
    REQUIRE(bytes.startsWith("GIF89a"));
    REQUIRE(readU16(bytes, 6) == 8);
    REQUIRE(readU16(bytes, 8) == 6);
    REQUIRE(static_cast<std::uint8_t>(bytes.at(bytes.size() - 1)) == 0x3B);
    REQUIRE(bytes.contains("NETSCAPE2.0"));
}

TEST_CASE("GifEncoder round-trips a two-color frame")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("split.gif"));

    constexpr int width = 16;
    constexpr int height = 8;
    ors::GifEncoder encoder;
    ors::MuxerOpenParams params;
    params.filePath = path;
    params.videoWidth = width;
    params.videoHeight = height;
    params.videoFrameRate = 5;
    REQUIRE(encoder.open(params));
    REQUIRE(encoder.writeVideo(makeSplitFrame(width, height)));
    REQUIRE(encoder.finalize());

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    int offset = 13;
    offset = skipExtensions(bytes, offset);
    REQUIRE(static_cast<std::uint8_t>(bytes.at(offset)) == 0x2C);
    ++offset;
    REQUIRE(readU16(bytes, offset) == 0);
    offset += 2;
    REQUIRE(readU16(bytes, offset) == 0);
    offset += 2;
    REQUIRE(readU16(bytes, offset) == width);
    offset += 2;
    REQUIRE(readU16(bytes, offset) == height);
    offset += 2;
    const int packed = static_cast<std::uint8_t>(bytes.at(offset++));
    REQUIRE((packed & 0x80) != 0);
    const int tableBits = (packed & 0x07) + 1;
    const int tableSize = 1 << tableBits;
    REQUIRE(offset + tableSize * 3 <= bytes.size());
    offset += tableSize * 3;
    const int minCodeSize = static_cast<std::uint8_t>(bytes.at(offset++));
    const QByteArray packedCodes = concatSubBlocks(bytes, offset);
    const auto indices = decodeLzw(packedCodes, minCodeSize, width * height);
    REQUIRE(static_cast<int>(indices.size()) >= width * height);

    const int left = indices.front();
    const int right = indices[static_cast<std::size_t>(width / 2)];
    REQUIRE(left != right);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int expected = x < width / 2 ? left : right;
            REQUIRE(indices[static_cast<std::size_t>(y * width + x)] == expected);
        }
    }
}

TEST_CASE("GifEncoder rejects invalid open params")
{
    ors::GifEncoder encoder;
    ors::MuxerOpenParams params;
    params.filePath = QStringLiteral("unused.gif");
    params.videoWidth = 0;
    params.videoHeight = 8;
    REQUIRE_FALSE(encoder.open(params));
    REQUIRE_FALSE(encoder.lastError().isEmpty());
}
