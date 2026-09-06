#pragma once

#include "capture/ICapture.h"

#include <QString>
#include <memory>

namespace ors {

class CaptureWin final : public ICapture {
public:
    CaptureWin();
    ~CaptureWin() override;

    bool start(const CaptureSettings& settings) override;
    void stop() override;
    GrabResult grab(VideoFrame& out) override;
    GrabResult grab(VideoFrame& out, bool copyPixels, bool emitCachedOnIdle = false);
    GrabResult grabStill(VideoFrame& out, int timeoutMs);

    int width() const;
    int height() const;
    bool softwareFallback() const;
    QString lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ors
