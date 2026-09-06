#pragma once

#include "core/VideoFrame.h"

#include <QImage>
#include <QString>

namespace ors {

struct WatermarkSettings {
    bool enabled{false};
    QString imagePath;
    int opacity{100};
    int x{10};
    int y{10};
};

class WatermarkOverlay {
public:
    bool load(const WatermarkSettings& settings);
    void configure(const QImage& image, int opacity, int x, int y);
    void clear();
    void apply(VideoFrame& frame) const;
    bool isActive() const { return active_; }

private:
    bool active_{false};
    QImage image_;
    int opacity_{100};
    int x_{10};
    int y_{10};
};

} // namespace ors
