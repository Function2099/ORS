#pragma once

#include "mux/IMuxer.h"

#include <memory>

namespace ors {

class Mp4Muxer final : public IMuxer {
public:
    Mp4Muxer();
    ~Mp4Muxer() override;

    bool open(const MuxerOpenParams& params) override;
    bool writeVideo(const EncodedPacket& packet) override;
    bool writeAudio(const EncodedPacket& packet) override;
    bool finalize() override;

    QString lastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ors
