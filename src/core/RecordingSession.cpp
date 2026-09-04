#include "core/RecordingSession.h"

namespace ors {

RecordingSession::RecordingSession(QObject* parent)
    : QObject(parent)
{}

bool RecordingSession::start()
{
    if (state_ != State::Idle) {
        return false;
    }
    emit errorOccurred(tr("擷取管線尚未接上"));
    return false;
}

void RecordingSession::pause()
{
    if (state_ != State::Recording) {
        return;
    }
    setState(State::Paused);
}

void RecordingSession::resume()
{
    if (state_ != State::Paused) {
        return;
    }
    setState(State::Recording);
}

void RecordingSession::stop()
{
    if (state_ == State::Idle) {
        return;
    }
    setState(State::Idle);
    emit recordingFinished({});
}

void RecordingSession::setState(State state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit stateChanged(state_);
}

} // namespace ors
