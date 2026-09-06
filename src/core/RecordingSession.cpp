#include "core/RecordingSession.h"

#include "core/AudioFrame.h"
#include "core/Clock.h"
#include "core/Config.h"
#include "core/EncodedPacket.h"
#include "core/FilenameTemplate.h"
#include "core/FrameQueue.h"
#include "core/VideoFrame.h"

#include <QDateTime>
#include <QMetaObject>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "audio/AudioCaptureWin.h"
#include "audio/AudioMix.h"
#include "capture/CaptureWin.h"
#include "encode/IEncoder.h"
#include "encode/MFEncoder.h"
#include "mux/GifEncoder.h"
#include "mux/IMuxer.h"
#include "mux/Mp4Muxer.h"

#include <mfapi.h>
#include <objbase.h>
#endif

namespace ors {
#ifdef Q_OS_WIN
namespace {

struct ComInit {
    HRESULT hr{E_FAIL};
    ComInit()
        : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {}
    ~ComInit()
    {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

QString uniqueOutputPath(const RecordingRequest& request)
{
    return makeUniqueOutputPath(
        request.outputDirectory,
        request.filenameTemplate,
        request.filenamePrefix,
        request.filenameStartNumber,
        Config::containerExtension(request.container),
        QDateTime::currentDateTime());
}

bool isGifContainer(const QString& container)
{
    return container.compare(QLatin1String("gif"), Qt::CaseInsensitive) == 0;
}

bool isMp4Container(const QString& container)
{
    return container.compare(QLatin1String("mp4"), Qt::CaseInsensitive) == 0;
}

std::int64_t pcmDurationNs(const AudioFrame& frame)
{
    const int bytesPerSample = std::max(1, frame.bitsPerSample / 8);
    const int frameBytes = std::max(1, frame.channels * bytesPerSample);
    const int samples = static_cast<int>(frame.bytes.size()) / frameBytes;
    if (frame.sampleRate <= 0 || samples <= 0) {
        return 0;
    }
    return static_cast<std::int64_t>(samples) * 1'000'000'000 / frame.sampleRate;
}

} // namespace

class RecordingSession::Pipeline {
public:
    explicit Pipeline(RecordingSession* session, std::size_t videoCapacity)
        : session_(session)
        , videoQueue_(videoCapacity)
    {}

    ~Pipeline()
    {
        requestStop();
        joinWorkers();
    }

    bool start(const RecordingRequest& request)
    {
        request_ = request;
        outputPath = uniqueOutputPath(request);
        if (outputPath.isEmpty()) {
            error = QStringLiteral("無法建立輸出資料夾");
            return false;
        }

        const bool gif = isGifContainer(request.container);
        if (!gif) {
            const HRESULT mfHr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
            if (FAILED(mfHr)) {
                error = QStringLiteral("無法初始化 Media Foundation");
                return false;
            }
            mfStarted_ = true;
        }

        capture_ = std::make_unique<CaptureWin>();
        encoder_ = std::make_unique<MFEncoder>();
        if (gif) {
            muxer_ = std::make_unique<GifEncoder>();
        } else {
            muxer_ = std::make_unique<Mp4Muxer>();
        }

        wantAudio_ = !gif && (request.audio.systemAudio || !request.audio.microphoneId.isEmpty());
        if (wantAudio_ && request.audio.systemAudio) {
            systemAudio_ = std::make_unique<AudioCaptureWin>();
        }
        if (wantAudio_ && !request.audio.microphoneId.isEmpty()) {
            micAudio_ = std::make_unique<AudioCaptureWin>();
        }

        watermark_.load(request.watermark);

        captureThread_ = std::thread([this] { captureLoop(); });

        {
            std::unique_lock lock(mutex_);
            if (!readyCv_.wait_for(lock, std::chrono::seconds(3), [this] {
                    return captureReady_ || stop_.load();
                })) {
                error = QStringLiteral("畫面擷取啟動逾時");
                requestStop();
                joinWorkers();
                shutdownMf();
                return false;
            }
        }
        if (!captureReady_) {
            requestStop();
            joinWorkers();
            shutdownMf();
            return false;
        }

        if (systemAudio_ || micAudio_) {
            audioThread_ = std::thread([this] { audioLoop(); });
            std::unique_lock lock(mutex_);
            readyCv_.wait_for(lock, std::chrono::milliseconds(800), [this] {
                return audioReady_ || audioFailed_ || stop_.load();
            });
            if (!audioReady_) {
                wantAudio_ = false;
                if (audioWarning.isEmpty()) {
                    audioWarning = QStringLiteral("無法擷取音訊，改為僅錄製畫面");
                }
            }
        }

        encodeThread_ = std::thread([this] { encodeLoop(); });
        {
            std::unique_lock lock(mutex_);
            if (!readyCv_.wait_for(lock, std::chrono::seconds(5), [this] {
                    return muxerReady_ || stop_.load();
                })) {
                error = QStringLiteral("無法開啟編碼器");
                requestStop();
                joinWorkers();
                shutdownMf();
                return false;
            }
        }
        if (!muxerReady_) {
            requestStop();
            joinWorkers();
            shutdownMf();
            return false;
        }
        return true;
    }

    void requestStop()
    {
        stop_.store(true);
        videoQueue_.wake();
        audioQueue_.wake();
        readyCv_.notify_all();
        snapshotCv_.notify_all();
    }

    bool grabSnapshot(VideoFrame& out, int timeoutMs)
    {
        std::unique_lock lock(snapshotMutex_);
        snapshotReady_ = false;
        snapshotRequested_.store(true, std::memory_order_release);
        const bool ok = snapshotCv_.wait_for(
            lock,
            std::chrono::milliseconds(std::max(1, timeoutMs)),
            [this] { return snapshotReady_ || stop_.load(); });
        snapshotRequested_.store(false, std::memory_order_release);
        if (!ok || !snapshotReady_ || snapshotOut_.bytes.empty()) {
            return false;
        }
        out = std::move(snapshotOut_);
        snapshotReady_ = false;
        return true;
    }

    void setPaused(bool paused)
    {
        const bool was = paused_.exchange(paused);
        if (paused == was) {
            return;
        }
        if (paused) {
            pauseStartedNs_.store(nowNs());
            return;
        }
        const std::int64_t started = pauseStartedNs_.exchange(0);
        if (started > 0) {
            const std::int64_t extra = nowNs() - started;
            if (extra > 0) {
                pauseOffsetNs_.fetch_add(extra);
            }
        }
    }

    std::uint64_t dropped() const { return videoQueue_.dropped(); }

    void joinWorkers()
    {
        if (captureThread_.joinable()) {
            captureThread_.join();
        }
        if (audioThread_.joinable()) {
            audioThread_.join();
        }
        if (encodeThread_.joinable()) {
            encodeThread_.join();
        }
        shutdownMf();
    }

    QString outputPath;
    QString error;
    QString audioWarning;
    QString captureWarning;
    QString encoderWarning;

private:
    void shutdownMf()
    {
        if (mfStarted_) {
            MFShutdown();
            mfStarted_ = false;
        }
    }

    void fail(const QString& message)
    {
        if (error.isEmpty()) {
            error = message;
        }
        requestStop();
    }

    void captureLoop()
    {
        ComInit com;
        if (!capture_->start(request_.capture)) {
            error = capture_->lastError().isEmpty()
                ? QStringLiteral("畫面擷取啟動失敗")
                : capture_->lastError();
            requestStop();
            return;
        }

        request_.encoder.width = capture_->width();
        request_.encoder.height = capture_->height();
        if (capture_->softwareFallback()) {
            captureWarning = QStringLiteral("畫面擷取已改用 GDI，效能可能較低");
        }
        {
            std::lock_guard lock(mutex_);
            captureReady_ = true;
        }
        readyCv_.notify_all();

        VideoFrame frame;
        while (!stop_.load()) {
            const bool snapshot = snapshotRequested_.load();
            const bool copyPixels = !paused_.load() || snapshot;
            const GrabResult grabbed = capture_->grab(frame, copyPixels, snapshot);
            if (grabbed == GrabResult::Failed) {
                if (!stop_.load()) {
                    fail(capture_->lastError().isEmpty()
                             ? QStringLiteral("畫面擷取中斷")
                             : capture_->lastError());
                }
                break;
            }
            if (grabbed == GrabResult::Idle) {
                fulfillSnapshot(frame);
                continue;
            }
            fulfillSnapshot(frame);
            if (!paused_.load()) {
                watermark_.apply(frame);
                frame.timestampNs = adjustPausedTimestampNs(
                    frame.timestampNs, pauseOffsetNs_.load(std::memory_order_relaxed));
                videoQueue_.push(std::move(frame));
            }
        }
        capture_->stop();
        snapshotCv_.notify_all();
    }

    void fulfillSnapshot(const VideoFrame& frame)
    {
        if (!snapshotRequested_.load(std::memory_order_acquire)) {
            return;
        }
        if (frame.format != PixelFormat::BGRA8 || frame.width < 2 || frame.height < 2
            || frame.bytes.empty()) {
            return;
        }
        std::lock_guard lock(snapshotMutex_);
        if (!snapshotRequested_.load(std::memory_order_relaxed) || snapshotReady_) {
            return;
        }
        snapshotOut_ = frame;
        snapshotReady_ = true;
        snapshotRequested_.store(false, std::memory_order_release);
        snapshotCv_.notify_all();
    }

    void audioLoop()
    {
        ComInit com;

        if (systemAudio_) {
            AudioCaptureSettings systemSettings;
            systemSettings.systemAudio = true;
            if (!systemAudio_->start(systemSettings)) {
                audioWarning = systemAudio_->lastError().isEmpty()
                    ? QStringLiteral("無法擷取系統音訊")
                    : systemAudio_->lastError();
                systemAudio_.reset();
            }
        }
        if (micAudio_) {
            AudioCaptureSettings micSettings;
            micSettings.systemAudio = false;
            micSettings.microphoneId = request_.audio.microphoneId;
            micSettings.inputSource = request_.audio.inputSource;
            if (!micAudio_->start(micSettings)) {
                const QString micError = micAudio_->lastError().isEmpty()
                    ? QStringLiteral("無法擷取麥克風")
                    : micAudio_->lastError();
                audioWarning = audioWarning.isEmpty() ? micError
                    : audioWarning + QStringLiteral("；") + micError;
                micAudio_.reset();
            }
        }

        if (!systemAudio_ && !micAudio_) {
            audioFailed_ = true;
            readyCv_.notify_all();
            return;
        }

        const int outRate = systemAudio_ ? systemAudio_->sampleRate() : micAudio_->sampleRate();
        request_.audio.sampleRate = outRate;
        request_.audio.channels = 2;
        audioReady_ = true;
        readyCv_.notify_all();

        std::vector<std::int16_t> micFifo;
        AudioFrame frame;
        while (!stop_.load()) {
            if (systemAudio_ && micAudio_) {
                if (!systemAudio_->grab(frame, 16)) {
                    systemAudio_.reset();
                    continue;
                }
                drainMicIntoFifo(micFifo, outRate);
                const std::size_t maxFrames =
                    static_cast<std::size_t>(std::max(1, outRate / 10)); // ~100 ms
                trimStereoFifo(micFifo, maxFrames);
                if (!paused_.load() && !frame.bytes.empty()) {
                    frame.timestampNs = adjustPausedTimestampNs(
                        frame.timestampNs, pauseOffsetNs_.load(std::memory_order_relaxed));
                    const std::size_t frames = frame.bytes.size() / (2 * sizeof(std::int16_t));
                    mixStereoS16FromFifo(
                        reinterpret_cast<std::int16_t*>(frame.bytes.data()),
                        frames,
                        micFifo);
                    audioQueue_.pushWait(std::move(frame));
                }
                continue;
            }

            AudioCaptureWin* source = systemAudio_ ? systemAudio_.get() : micAudio_.get();
            if (!source) {
                break;
            }
            if (!source->grab(frame, 16)) {
                if (source == systemAudio_.get()) {
                    systemAudio_.reset();
                } else {
                    micAudio_.reset();
                }
                continue;
            }
            if (!paused_.load() && !frame.bytes.empty()) {
                if (frame.sampleRate != outRate) {
                    std::vector<std::int16_t> resampled;
                    const auto* src = reinterpret_cast<const std::int16_t*>(frame.bytes.data());
                    const std::size_t srcFrames = frame.bytes.size() / (2 * sizeof(std::int16_t));
                    resampleStereoS16Linear(src, srcFrames, frame.sampleRate, outRate, resampled);
                    frame.bytes.resize(resampled.size() * sizeof(std::int16_t));
                    if (!resampled.empty()) {
                        std::memcpy(frame.bytes.data(), resampled.data(), frame.bytes.size());
                    }
                    frame.sampleRate = outRate;
                }
                frame.timestampNs = adjustPausedTimestampNs(
                    frame.timestampNs, pauseOffsetNs_.load(std::memory_order_relaxed));
                audioQueue_.pushWait(std::move(frame));
            }
        }
        if (systemAudio_) {
            systemAudio_->stop();
        }
        if (micAudio_) {
            micAudio_->stop();
        }
    }

    void drainMicIntoFifo(std::vector<std::int16_t>& fifo, int outRate)
    {
        if (!micAudio_) {
            return;
        }
        for (int i = 0; i < 8; ++i) {
            AudioFrame mic;
            if (!micAudio_->grab(mic, 0)) {
                micAudio_.reset();
                return;
            }
            if (mic.bytes.empty()) {
                return;
            }
            const auto* src = reinterpret_cast<const std::int16_t*>(mic.bytes.data());
            const std::size_t srcFrames = mic.bytes.size() / (2 * sizeof(std::int16_t));
            std::vector<std::int16_t> resampled;
            resampleStereoS16Linear(src, srcFrames, mic.sampleRate, outRate, resampled);
            fifo.insert(fifo.end(), resampled.begin(), resampled.end());
        }
    }

    void encodeLoop()
    {
        ComInit com;
        EncoderSettings encoderSettings = request_.encoder;
        encoderSettings.width = capture_->width();
        encoderSettings.height = capture_->height();
        if (encoderSettings.frameRate <= 0) {
            encoderSettings.frameRate = request_.capture.frameRate > 0 ? request_.capture.frameRate : 30;
        }
        const bool gif = isGifContainer(request_.container);
        encoderSettings.outputFormat = gif ? PixelFormat::BGRA8 : PixelFormat::NV12;
        if (!encoder_->open(encoderSettings)) {
            fail(QStringLiteral("無法開啟編碼器"));
            notifySessionDone();
            return;
        }

        MuxerOpenParams muxParams;
        muxParams.filePath = outputPath;
        muxParams.videoWidth = encoderSettings.width & ~1;
        muxParams.videoHeight = encoderSettings.height & ~1;
        muxParams.videoFrameRate = encoderSettings.frameRate;
        muxParams.videoBitrateKbps = encoderSettings.bitrateKbps > 0 ? encoderSettings.bitrateKbps : 8000;
        muxParams.keyframeGopFrames = encoderSettings.keyframeGopFrames;
        muxParams.encoderThreads = request_.encoderThreads;
        muxParams.hasAudio = wantAudio_ && audioReady_;
        muxParams.audioSampleRate = request_.audio.sampleRate;
        muxParams.audioChannels = 2;
        muxParams.preferNv12 = !gif;
        if (!muxer_->open(muxParams)) {
            fail(muxer_->lastError().isEmpty() ? QStringLiteral("無法建立輸出檔案") : muxer_->lastError());
            encoder_->close();
            notifySessionDone();
            return;
        }
        if (!gif && muxer_->videoInputFormat() != PixelFormat::NV12) {
            encoderSettings.outputFormat = PixelFormat::BGRA8;
            encoder_->open(encoderSettings);
        }

        if (!gif && !muxer_->hardwareVideoEncoder()) {
            encoderWarning = QStringLiteral("硬體視訊編碼不可用，改用軟體編碼，可能較耗 CPU");
        }

        muxerReady_ = true;
        readyCv_.notify_all();

        VideoFrame video;
        AudioFrame audio;
        EncodedPacket packet;
        EncodedPacket pendingVideo;
        bool hasPendingVideo = false;
        const std::int64_t fallbackDurationNs =
            1'000'000'000 / std::max(1, encoderSettings.frameRate);

        const auto holdVideo = [&](EncodedPacket incoming) -> bool {
            if (hasPendingVideo) {
                incoming.timestampNs = monotonicTimestampNs(
                    pendingVideo.timestampNs,
                    incoming.timestampNs,
                    1'000'000);
                pendingVideo.durationNs = sampleDurationNs(
                    pendingVideo.timestampNs,
                    incoming.timestampNs,
                    fallbackDurationNs);
                if (!muxer_->writeVideo(pendingVideo) && !stop_.load()) {
                    fail(muxer_->lastError().isEmpty()
                             ? QStringLiteral("寫入視訊失敗")
                             : muxer_->lastError());
                    return false;
                }
            }
            pendingVideo = std::move(incoming);
            hasPendingVideo = true;
            return true;
        };

        while (!stop_.load()) {
            if (videoQueue_.waitPop(video, std::chrono::milliseconds(15))) {
                if (encoder_->encode(video, packet) && !holdVideo(std::move(packet))) {
                    break;
                }
            }
            bool audioFailed = false;
            while (audioQueue_.pop(audio)) {
                packet = {};
                packet.kind = PacketKind::Audio;
                packet.timestampNs = audio.timestampNs;
                packet.durationNs = pcmDurationNs(audio);
                packet.bytes = std::move(audio.bytes);
                if (!muxer_->writeAudio(packet) && !stop_.load()) {
                    fail(muxer_->lastError().isEmpty()
                             ? QStringLiteral("寫入音訊失敗")
                             : muxer_->lastError());
                    audioFailed = true;
                    break;
                }
            }
            if (audioFailed) {
                break;
            }
        }

        while (videoQueue_.pop(video)) {
            if (encoder_->encode(video, packet)) {
                holdVideo(std::move(packet));
            }
        }
        while (audioQueue_.pop(audio)) {
            packet = {};
            packet.kind = PacketKind::Audio;
            packet.timestampNs = audio.timestampNs;
            packet.durationNs = pcmDurationNs(audio);
            packet.bytes = std::move(audio.bytes);
            muxer_->writeAudio(packet);
        }

        std::vector<EncodedPacket> flushed;
        encoder_->flush(flushed);
        for (EncodedPacket& item : flushed) {
            holdVideo(std::move(item));
        }
        if (hasPendingVideo) {
            pendingVideo.durationNs = fallbackDurationNs;
            muxer_->writeVideo(pendingVideo);
        }
        encoder_->close();
        if (!muxer_->finalize() && error.isEmpty()) {
            error = muxer_->lastError().isEmpty() ? QStringLiteral("無法完成輸出檔案") : muxer_->lastError();
        }
        notifySessionDone();
    }

    void notifySessionDone()
    {
        const QString path = outputPath;
        const QString err = error;
        QMetaObject::invokeMethod(
            session_,
            [session = session_, path, err]() { session->onWorkerDone(path, err); },
            Qt::QueuedConnection);
    }

    RecordingSession* session_{};
    RecordingRequest request_{};
    std::unique_ptr<CaptureWin> capture_;
    std::unique_ptr<AudioCaptureWin> systemAudio_;
    std::unique_ptr<AudioCaptureWin> micAudio_;
    std::unique_ptr<IEncoder> encoder_;
    std::unique_ptr<IMuxer> muxer_;
    WatermarkOverlay watermark_;
    FrameQueue<VideoFrame> videoQueue_{4};
    FrameQueue<AudioFrame> audioQueue_{32};
    std::thread captureThread_;
    std::thread audioThread_;
    std::thread encodeThread_;
    std::mutex mutex_;
    std::condition_variable readyCv_;
    std::mutex snapshotMutex_;
    std::condition_variable snapshotCv_;
    VideoFrame snapshotOut_;
    bool snapshotReady_{false};
    std::atomic<bool> snapshotRequested_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> captureReady_{false};
    std::atomic<bool> audioReady_{false};
    std::atomic<bool> audioFailed_{false};
    std::atomic<bool> muxerReady_{false};
    std::atomic<std::int64_t> pauseOffsetNs_{0};
    std::atomic<std::int64_t> pauseStartedNs_{0};
    bool wantAudio_{false};
    bool mfStarted_{false};
};

#else

class RecordingSession::Pipeline {};

#endif // Q_OS_WIN

RecordingSession::RecordingSession(QObject* parent)
    : QObject(parent)
{}

RecordingSession::~RecordingSession()
{
    waitUntilStopped();
}

bool RecordingSession::start(const RecordingRequest& request)
{
    if (state_ != State::Idle) {
        return false;
    }

#ifndef Q_OS_WIN
    Q_UNUSED(request);
    emit errorOccurred(tr("目前僅支援 Windows 錄製"));
    return false;
#else
    if (!isMp4Container(request.container) && !isGifContainer(request.container)) {
        emit errorOccurred(tr("目前僅支援 MP4 與 GIF 輸出"));
        return false;
    }
    if (request.capture.width < 2 || request.capture.height < 2) {
        emit errorOccurred(tr("擷取區域太小"));
        return false;
    }

    pipeline_ = std::make_unique<Pipeline>(
        this, static_cast<std::size_t>(Config::frameQueueCapacity(request.pipelineLayers)));
    if (!pipeline_->start(request)) {
        const QString message = pipeline_->error.isEmpty()
            ? tr("無法開始錄製")
            : pipeline_->error;
        pipeline_.reset();
        emit errorOccurred(message);
        return false;
    }

    outputPath_ = pipeline_->outputPath;
    setState(State::Recording);
    if (!pipeline_->audioWarning.isEmpty()) {
        emit warningOccurred(pipeline_->audioWarning);
    }
    if (!pipeline_->captureWarning.isEmpty()) {
        emit warningOccurred(pipeline_->captureWarning);
    }
    if (!pipeline_->encoderWarning.isEmpty()) {
        emit warningOccurred(pipeline_->encoderWarning);
    }
    return true;
#endif
}

void RecordingSession::pause()
{
    if (state_ != State::Recording) {
        return;
    }
#ifdef Q_OS_WIN
    if (pipeline_) {
        pipeline_->setPaused(true);
    }
#endif
    setState(State::Paused);
}

bool RecordingSession::snapshotFrame(VideoFrame& out)
{
#ifndef Q_OS_WIN
    Q_UNUSED(out);
    return false;
#else
    if (state_ == State::Idle || !pipeline_) {
        return false;
    }
    return pipeline_->grabSnapshot(out, 1000);
#endif
}

void RecordingSession::resume()
{
    if (state_ != State::Paused) {
        return;
    }
#ifdef Q_OS_WIN
    if (pipeline_) {
        pipeline_->setPaused(false);
    }
#endif
    setState(State::Recording);
}

void RecordingSession::stop()
{
    if (!pipeline_) {
        if (state_ != State::Idle) {
            setState(State::Idle);
        }
        return;
    }
    if (state_ == State::Stopping) {
        return;
    }

#ifdef Q_OS_WIN
    pipeline_->requestStop();
    setState(State::Stopping);
#else
    pipeline_.reset();
    setState(State::Idle);
    emit recordingFinished({});
#endif
}

void RecordingSession::waitUntilStopped()
{
#ifdef Q_OS_WIN
    if (pipeline_) {
        pipeline_->requestStop();
        pipeline_->joinWorkers();
        outputPath_ = pipeline_->outputPath;
        pipeline_.reset();
    }
#else
    pipeline_.reset();
#endif
    if (state_ != State::Idle) {
        setState(State::Idle);
    }
}

std::uint64_t RecordingSession::droppedFrames() const
{
#ifdef Q_OS_WIN
    if (pipeline_) {
        return pipeline_->dropped();
    }
#endif
    return 0;
}

void RecordingSession::onWorkerDone(const QString& path, const QString& error)
{
    if (!pipeline_) {
        return;
    }
#ifdef Q_OS_WIN
    pipeline_->requestStop();
    pipeline_->joinWorkers();
#endif
    pipeline_.reset();
    outputPath_ = path;
    setState(State::Idle);
    if (!error.isEmpty()) {
        emit errorOccurred(error);
    }
    emit recordingFinished(path);
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
