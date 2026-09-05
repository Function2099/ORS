#pragma once

#include "audio/IAudioCapture.h"
#include "capture/ICapture.h"
#include "encode/IEncoder.h"

#include <QObject>
#include <QString>
#include <memory>

namespace ors {

struct RecordingRequest {
    CaptureSettings capture;
    AudioCaptureSettings audio;
    EncoderSettings encoder;
    QString outputDirectory;
    QString filenameTemplate{QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>")};
    QString filenamePrefix{QStringLiteral("錄製")};
    int filenameStartNumber{1};
    QString container{QStringLiteral("mp4")};
};

class RecordingSession : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Recording,
        Paused,
    };
    Q_ENUM(State)

    explicit RecordingSession(QObject* parent = nullptr);
    ~RecordingSession() override;

    State state() const { return state_; }
    QString outputPath() const { return outputPath_; }

    bool start(const RecordingRequest& request);
    void pause();
    void resume();
    void stop();

    Q_INVOKABLE void onWorkerDone(const QString& path, const QString& error);

signals:
    void stateChanged(ors::RecordingSession::State state);
    void errorOccurred(const QString& message);
    void recordingFinished(const QString& path);

private:
    void setState(State state);

    class Pipeline;
    friend class Pipeline;

    State state_{State::Idle};
    QString outputPath_;
    std::unique_ptr<Pipeline> pipeline_;
};

} // namespace ors
