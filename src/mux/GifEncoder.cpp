#include "mux/GifEncoder.h"

#include <QFile>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ors {
namespace {

constexpr int kMaxPalette = 256;
constexpr int kHistBits = 5;
constexpr int kHistSize = 1 << (kHistBits * 3);
constexpr int kMaxLzwCodes = 4096;

int histIndex(int r, int g, int b)
{
    return ((r >> (8 - kHistBits)) << (kHistBits * 2))
        | ((g >> (8 - kHistBits)) << kHistBits)
        | (b >> (8 - kHistBits));
}

int palettePower(int count)
{
    int bits = 1;
    while ((1 << bits) < std::max(2, count)) {
        ++bits;
    }
    return std::clamp(bits, 1, 8);
}

int delayCentiseconds(int frameRate)
{
    const int fps = std::max(1, frameRate);
    return std::max(2, (100 + fps / 2) / fps);
}

struct HistBin {
    std::uint32_t count{};
    std::uint64_t sumR{};
    std::uint64_t sumG{};
    std::uint64_t sumB{};
};

struct ColorPoint {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint32_t count{};
    int hist{};
};

int channelValue(const ColorPoint& point, int channel)
{
    if (channel == 0) {
        return point.r;
    }
    if (channel == 1) {
        return point.g;
    }
    return point.b;
}

int longestChannel(const ColorPoint* points, int begin, int end)
{
    int minV[3] = {255, 255, 255};
    int maxV[3] = {0, 0, 0};
    for (int i = begin; i < end; ++i) {
        minV[0] = std::min(minV[0], static_cast<int>(points[i].r));
        maxV[0] = std::max(maxV[0], static_cast<int>(points[i].r));
        minV[1] = std::min(minV[1], static_cast<int>(points[i].g));
        maxV[1] = std::max(maxV[1], static_cast<int>(points[i].g));
        minV[2] = std::min(minV[2], static_cast<int>(points[i].b));
        maxV[2] = std::max(maxV[2], static_cast<int>(points[i].b));
    }
    const int ranges[3] = {maxV[0] - minV[0], maxV[1] - minV[1], maxV[2] - minV[2]};
    int channel = 0;
    if (ranges[1] > ranges[channel]) {
        channel = 1;
    }
    if (ranges[2] > ranges[channel]) {
        channel = 2;
    }
    return channel;
}

int channelRange(const ColorPoint* points, int begin, int end, int channel)
{
    int mn = 255;
    int mx = 0;
    for (int i = begin; i < end; ++i) {
        const int value = channelValue(points[i], channel);
        mn = std::min(mn, value);
        mx = std::max(mx, value);
    }
    return mx - mn;
}

struct Palette {
    std::array<std::uint8_t, kMaxPalette * 3> rgb{};
    int count{0};
    int tableBits{1};
    std::array<std::uint8_t, kHistSize> histToIndex{};
};

bool quantizeMedianCut(
    const std::uint8_t* bgra,
    int width,
    int height,
    Palette& palette,
    std::vector<std::uint8_t>& indices)
{
    std::vector<HistBin> hist(static_cast<std::size_t>(kHistSize));
    const int pixelCount = width * height;
    for (int i = 0; i < pixelCount; ++i) {
        const std::uint8_t b = bgra[i * 4 + 0];
        const std::uint8_t g = bgra[i * 4 + 1];
        const std::uint8_t r = bgra[i * 4 + 2];
        const int index = histIndex(r, g, b);
        hist[static_cast<std::size_t>(index)].count += 1;
        hist[static_cast<std::size_t>(index)].sumR += r;
        hist[static_cast<std::size_t>(index)].sumG += g;
        hist[static_cast<std::size_t>(index)].sumB += b;
    }

    std::vector<ColorPoint> points;
    points.reserve(256);
    for (int i = 0; i < kHistSize; ++i) {
        if (hist[static_cast<std::size_t>(i)].count == 0) {
            continue;
        }
        ColorPoint point;
        point.count = hist[static_cast<std::size_t>(i)].count;
        point.r = static_cast<std::uint8_t>(hist[static_cast<std::size_t>(i)].sumR / point.count);
        point.g = static_cast<std::uint8_t>(hist[static_cast<std::size_t>(i)].sumG / point.count);
        point.b = static_cast<std::uint8_t>(hist[static_cast<std::size_t>(i)].sumB / point.count);
        point.hist = i;
        points.push_back(point);
    }
    if (points.empty()) {
        return false;
    }

    struct Box {
        int begin{};
        int end{};
    };
    std::vector<Box> boxes;
    boxes.push_back({0, static_cast<int>(points.size())});

    while (static_cast<int>(boxes.size()) < kMaxPalette) {
        int best = -1;
        int bestRange = 0;
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            if (boxes[static_cast<std::size_t>(i)].end - boxes[static_cast<std::size_t>(i)].begin <= 1) {
                continue;
            }
            const int channel = longestChannel(
                points.data(),
                boxes[static_cast<std::size_t>(i)].begin,
                boxes[static_cast<std::size_t>(i)].end);
            const int range = channelRange(
                points.data(),
                boxes[static_cast<std::size_t>(i)].begin,
                boxes[static_cast<std::size_t>(i)].end,
                channel);
            if (range > bestRange) {
                bestRange = range;
                best = i;
            }
        }
        if (best < 0 || bestRange <= 0) {
            break;
        }

        const Box box = boxes[static_cast<std::size_t>(best)];
        const int channel = longestChannel(points.data(), box.begin, box.end);
        std::sort(
            points.begin() + box.begin,
            points.begin() + box.end,
            [channel](const ColorPoint& a, const ColorPoint& b) {
                return channelValue(a, channel) < channelValue(b, channel);
            });

        std::uint64_t total = 0;
        for (int i = box.begin; i < box.end; ++i) {
            total += points[static_cast<std::size_t>(i)].count;
        }
        const std::uint64_t half = total / 2;
        std::uint64_t acc = 0;
        int mid = box.begin + 1;
        for (int i = box.begin; i < box.end - 1; ++i) {
            acc += points[static_cast<std::size_t>(i)].count;
            mid = i + 1;
            if (acc >= half) {
                break;
            }
        }
        if (mid <= box.begin) {
            mid = box.begin + 1;
        }
        if (mid >= box.end) {
            mid = box.end - 1;
        }

        boxes[static_cast<std::size_t>(best)] = {box.begin, mid};
        boxes.push_back({mid, box.end});
    }

    palette = {};
    palette.count = static_cast<int>(boxes.size());
    palette.tableBits = palettePower(palette.count);
    palette.histToIndex.fill(0);
    for (int boxIndex = 0; boxIndex < palette.count; ++boxIndex) {
        const Box& box = boxes[static_cast<std::size_t>(boxIndex)];
        std::uint64_t sumR = 0;
        std::uint64_t sumG = 0;
        std::uint64_t sumB = 0;
        std::uint64_t count = 0;
        for (int i = box.begin; i < box.end; ++i) {
            sumR += static_cast<std::uint64_t>(points[static_cast<std::size_t>(i)].r)
                * points[static_cast<std::size_t>(i)].count;
            sumG += static_cast<std::uint64_t>(points[static_cast<std::size_t>(i)].g)
                * points[static_cast<std::size_t>(i)].count;
            sumB += static_cast<std::uint64_t>(points[static_cast<std::size_t>(i)].b)
                * points[static_cast<std::size_t>(i)].count;
            count += points[static_cast<std::size_t>(i)].count;
            palette.histToIndex[static_cast<std::size_t>(points[static_cast<std::size_t>(i)].hist)] =
                static_cast<std::uint8_t>(boxIndex);
        }
        if (count == 0) {
            count = 1;
        }
        palette.rgb[static_cast<std::size_t>(boxIndex) * 3 + 0] = static_cast<std::uint8_t>(sumR / count);
        palette.rgb[static_cast<std::size_t>(boxIndex) * 3 + 1] = static_cast<std::uint8_t>(sumG / count);
        palette.rgb[static_cast<std::size_t>(boxIndex) * 3 + 2] = static_cast<std::uint8_t>(sumB / count);
    }

    indices.resize(static_cast<std::size_t>(pixelCount));
    for (int i = 0; i < pixelCount; ++i) {
        const std::uint8_t b = bgra[i * 4 + 0];
        const std::uint8_t g = bgra[i * 4 + 1];
        const std::uint8_t r = bgra[i * 4 + 2];
        indices[static_cast<std::size_t>(i)] = palette.histToIndex[static_cast<std::size_t>(histIndex(r, g, b))];
    }
    return true;
}

class BitPacker {
public:
    void write(int code, int bits)
    {
        bitBuffer_ |= static_cast<std::uint32_t>(code) << bitCount_;
        bitCount_ += bits;
        while (bitCount_ >= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(bitBuffer_ & 0xFFu));
            bitBuffer_ >>= 8;
            bitCount_ -= 8;
        }
    }

    void flush()
    {
        if (bitCount_ > 0) {
            bytes_.push_back(static_cast<std::uint8_t>(bitBuffer_ & 0xFFu));
            bitBuffer_ = 0;
            bitCount_ = 0;
        }
    }

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::uint32_t bitBuffer_{};
    int bitCount_{};
};

bool compressLzw(const std::vector<std::uint8_t>& indices, int minCodeSize, std::vector<std::uint8_t>& packed)
{
    if (indices.empty() || minCodeSize < 2 || minCodeSize > 8) {
        return false;
    }

    const int clear = 1 << minCodeSize;
    const int eoi = clear + 1;
    BitPacker packer;
    int codeSize = minCodeSize + 1;
    int nextCode = eoi + 1;
    std::unordered_map<std::uint64_t, int> dict;
    dict.reserve(512);

    auto makeKey = [](int prefix, int suffix) -> std::uint64_t {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(prefix)) << 16)
            | static_cast<std::uint32_t>(suffix);
    };

    auto resetDict = [&] {
        dict.clear();
        codeSize = minCodeSize + 1;
        nextCode = eoi + 1;
    };

    packer.write(clear, codeSize);
    int prefix = indices.front();
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const int suffix = indices[i];
        const std::uint64_t key = makeKey(prefix, suffix);
        const auto it = dict.find(key);
        if (it != dict.end()) {
            prefix = it->second;
            continue;
        }

        packer.write(prefix, codeSize);
        if (nextCode < kMaxLzwCodes) {
            dict.emplace(key, nextCode);
            if (nextCode >= (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
            ++nextCode;
        } else {
            packer.write(clear, codeSize);
            resetDict();
        }
        prefix = suffix;
    }
    packer.write(prefix, codeSize);
    packer.write(eoi, codeSize);
    packer.flush();
    packed = packer.bytes();
    return true;
}

} // namespace

struct GifEncoder::Impl {
    QFile file;
    QString error;
    int width{};
    int height{};
    int frameRate{10};
    bool open{false};

    void writeU8(std::uint8_t value)
    {
        const char ch = static_cast<char>(value);
        file.write(&ch, 1);
    }

    void writeU16(std::uint16_t value)
    {
        writeU8(static_cast<std::uint8_t>(value & 0xFFu));
        writeU8(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    }

    void writeRaw(const void* data, int size)
    {
        file.write(static_cast<const char*>(data), size);
    }

    bool writeSubBlocks(const std::vector<std::uint8_t>& bytes)
    {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const int chunk = static_cast<int>(std::min<std::size_t>(255, bytes.size() - offset));
            writeU8(static_cast<std::uint8_t>(chunk));
            writeRaw(bytes.data() + offset, chunk);
            offset += static_cast<std::size_t>(chunk);
        }
        writeU8(0);
        return file.error() == QFile::NoError;
    }

    void closeFile()
    {
        if (file.isOpen()) {
            file.close();
        }
        open = false;
    }
};

GifEncoder::GifEncoder()
    : impl_(std::make_unique<Impl>())
{}

GifEncoder::~GifEncoder()
{
    finalize();
}

QString GifEncoder::lastError() const
{
    return impl_->error;
}

bool GifEncoder::open(const MuxerOpenParams& params)
{
    finalize();
    impl_->error.clear();
    if (params.filePath.isEmpty() || params.videoWidth < 1 || params.videoHeight < 1
        || params.videoWidth > 65535 || params.videoHeight > 65535) {
        impl_->error = QStringLiteral("GIF 參數無效");
        return false;
    }

    impl_->file.setFileName(params.filePath);
    if (!impl_->file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        impl_->error = QStringLiteral("無法建立 GIF 檔案");
        return false;
    }

    impl_->width = params.videoWidth;
    impl_->height = params.videoHeight;
    impl_->frameRate = std::max(1, params.videoFrameRate);

    impl_->writeRaw("GIF89a", 6);
    impl_->writeU16(static_cast<std::uint16_t>(impl_->width));
    impl_->writeU16(static_cast<std::uint16_t>(impl_->height));
    impl_->writeU8(0x70);
    impl_->writeU8(0);
    impl_->writeU8(0);

    // Netscape 2.0 loop-forever extension.
    impl_->writeU8(0x21);
    impl_->writeU8(0xFF);
    impl_->writeU8(0x0B);
    impl_->writeRaw("NETSCAPE2.0", 11);
    impl_->writeU8(0x03);
    impl_->writeU8(0x01);
    impl_->writeU16(0);
    impl_->writeU8(0x00);

    if (impl_->file.error() != QFile::NoError) {
        impl_->error = QStringLiteral("無法寫入 GIF 檔頭");
        impl_->closeFile();
        return false;
    }
    impl_->open = true;
    return true;
}

bool GifEncoder::writeVideo(const EncodedPacket& packet)
{
    if (!impl_->open) {
        impl_->error = QStringLiteral("GIF 尚未開啟");
        return false;
    }

    const std::size_t expected =
        static_cast<std::size_t>(impl_->width) * static_cast<std::size_t>(impl_->height) * 4u;
    if (packet.bytes.size() < expected) {
        impl_->error = QStringLiteral("GIF 影格尺寸不符");
        return false;
    }

    Palette palette;
    std::vector<std::uint8_t> indices;
    if (!quantizeMedianCut(packet.bytes.data(), impl_->width, impl_->height, palette, indices)) {
        impl_->error = QStringLiteral("GIF 調色盤量化失敗");
        return false;
    }

    const int tableSize = 1 << palette.tableBits;
    const int minCodeSize = std::max(2, palette.tableBits);
    std::vector<std::uint8_t> packed;
    if (!compressLzw(indices, minCodeSize, packed)) {
        impl_->error = QStringLiteral("GIF 壓縮失敗");
        return false;
    }

    const int delay = delayCentiseconds(impl_->frameRate);
    impl_->writeU8(0x21);
    impl_->writeU8(0xF9);
    impl_->writeU8(0x04);
    impl_->writeU8(0x04);
    impl_->writeU16(static_cast<std::uint16_t>(delay));
    impl_->writeU8(0);
    impl_->writeU8(0);

    impl_->writeU8(0x2C);
    impl_->writeU16(0);
    impl_->writeU16(0);
    impl_->writeU16(static_cast<std::uint16_t>(impl_->width));
    impl_->writeU16(static_cast<std::uint16_t>(impl_->height));
    impl_->writeU8(static_cast<std::uint8_t>(0x80 | (palette.tableBits - 1)));

    std::array<std::uint8_t, kMaxPalette * 3> table{};
    std::memcpy(table.data(), palette.rgb.data(), static_cast<std::size_t>(palette.count) * 3u);
    impl_->writeRaw(table.data(), tableSize * 3);
    impl_->writeU8(static_cast<std::uint8_t>(minCodeSize));
    if (!impl_->writeSubBlocks(packed) || impl_->file.error() != QFile::NoError) {
        impl_->error = QStringLiteral("寫入 GIF 影格失敗");
        return false;
    }
    return true;
}

bool GifEncoder::writeAudio(const EncodedPacket&)
{
    return true;
}

bool GifEncoder::finalize()
{
    if (!impl_->open) {
        impl_->closeFile();
        return true;
    }

    impl_->writeU8(0x3B);
    const bool ok = impl_->file.error() == QFile::NoError;
    if (!ok && impl_->error.isEmpty()) {
        impl_->error = QStringLiteral("無法完成 GIF 檔案");
    }
    impl_->closeFile();
    return ok;
}

} // namespace ors
