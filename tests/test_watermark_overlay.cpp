#include "core/VideoFrame.h"
#include "core/WatermarkOverlay.h"

#include <QColor>
#include <QImage>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace {

ors::VideoFrame makeSolidFrame(int width, int height, std::uint8_t b, std::uint8_t g, std::uint8_t r)
{
    ors::VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.stride = width * 4;
    frame.format = ors::PixelFormat::BGRA8;
    frame.bytes.assign(static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(height), 0);
    for (int i = 0; i < width * height; ++i) {
        const std::size_t offset = static_cast<std::size_t>(i) * 4u;
        frame.bytes[offset + 0] = b;
        frame.bytes[offset + 1] = g;
        frame.bytes[offset + 2] = r;
        frame.bytes[offset + 3] = 255;
    }
    return frame;
}

} // namespace

TEST_CASE("WatermarkOverlay composites opaque image at origin")
{
    QImage mark(2, 2, QImage::Format_ARGB32);
    mark.fill(QColor(255, 0, 0, 255));

    ors::WatermarkOverlay overlay;
    overlay.configure(mark, 100, 0, 0);
    REQUIRE(overlay.isActive());

    ors::VideoFrame frame = makeSolidFrame(8, 8, 255, 0, 0);
    overlay.apply(frame);

    REQUIRE(frame.bytes[0] == 0);
    REQUIRE(frame.bytes[1] == 0);
    REQUIRE(frame.bytes[2] == 255);
    REQUIRE(frame.bytes[3] == 255);
    REQUIRE(frame.bytes[4 * 4] == 255);
    REQUIRE(frame.bytes[4 * 4 + 2] == 0);
}

TEST_CASE("WatermarkOverlay skips frames when image is fully off-screen")
{
    QImage mark(4, 4, QImage::Format_ARGB32);
    mark.fill(QColor(0, 255, 0, 255));

    ors::WatermarkOverlay overlay;
    overlay.configure(mark, 100, 64, 0);
    REQUIRE(overlay.isActive());

    ors::VideoFrame frame = makeSolidFrame(8, 8, 10, 20, 30);
    overlay.apply(frame);
    REQUIRE(frame.bytes[0] == 10);
    REQUIRE(frame.bytes[1] == 20);
    REQUIRE(frame.bytes[2] == 30);
}

TEST_CASE("WatermarkOverlay load ignores disabled or empty path")
{
    ors::WatermarkOverlay overlay;
    ors::WatermarkSettings settings;
    settings.enabled = true;
    REQUIRE_FALSE(overlay.load(settings));
    REQUIRE_FALSE(overlay.isActive());

    settings.imagePath = QStringLiteral("missing-file.png");
    REQUIRE_FALSE(overlay.load(settings));
    REQUIRE_FALSE(overlay.isActive());

    settings.enabled = false;
    settings.imagePath = QStringLiteral("missing-file.png");
    REQUIRE_FALSE(overlay.load(settings));
}
