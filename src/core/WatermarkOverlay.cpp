#include "core/WatermarkOverlay.h"

#include <QPainter>

#include <algorithm>

namespace ors {

bool WatermarkOverlay::load(const WatermarkSettings& settings)
{
    clear();
    if (!settings.enabled || settings.imagePath.trimmed().isEmpty()) {
        return false;
    }

    QImage loaded;
    if (!loaded.load(settings.imagePath) || loaded.isNull()) {
        return false;
    }

    configure(
        loaded,
        settings.opacity,
        settings.x,
        settings.y);
    return active_;
}

void WatermarkOverlay::configure(const QImage& image, int opacity, int x, int y)
{
    if (image.isNull()) {
        clear();
        return;
    }
    image_ = image.convertToFormat(QImage::Format_ARGB32);
    opacity_ = std::clamp(opacity, 1, 100);
    x_ = x;
    y_ = y;
    active_ = !image_.isNull();
}

void WatermarkOverlay::clear()
{
    active_ = false;
    image_ = QImage();
    opacity_ = 100;
    x_ = 10;
    y_ = 10;
}

void WatermarkOverlay::apply(VideoFrame& frame) const
{
    if (!active_ || image_.isNull() || frame.format != PixelFormat::BGRA8
        || frame.width < 1 || frame.height < 1 || frame.bytes.empty()) {
        return;
    }

    const int stride = frame.stride > 0 ? frame.stride : frame.width * 4;
    if (stride < frame.width * 4) {
        return;
    }
    if (x_ >= frame.width || y_ >= frame.height) {
        return;
    }
    if (x_ + image_.width() <= 0 || y_ + image_.height() <= 0) {
        return;
    }

    QImage dest(
        frame.bytes.data(),
        frame.width,
        frame.height,
        stride,
        QImage::Format_ARGB32);
    if (dest.isNull()) {
        return;
    }

    QPainter painter(&dest);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(static_cast<qreal>(opacity_) / 100.0);
    painter.drawImage(x_, y_, image_);
}

} // namespace ors
