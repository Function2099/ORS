#pragma once

#include <QObject>
#include <QString>

namespace ors {

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

    State state() const { return state_; }

    // Phase 0: does not start capture; emits errorOccurred and stays Idle.
    bool start();
    void pause();
    void resume();
    void stop();

signals:
    void stateChanged(ors::RecordingSession::State state);
    void errorOccurred(const QString& message);
    void recordingFinished(const QString& path);

private:
    void setState(State state);

    State state_{State::Idle};
};

} // namespace ors
