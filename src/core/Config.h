#pragma once

#include <QString>
#include <QVector>

namespace ors {

struct SavedRegion {
    QString name;
    int width{};
    int height{};
};

struct ConfigData {
    int version{1};

    QString language{QStringLiteral("zh_TW")};
    QString lastTab{QStringLiteral("screen")};

    QString directory;
    QString filenameTemplate{QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>")};
    QString filenamePrefix{QStringLiteral("錄製")};
    int filenameStartNumber{1};

    QString regionPreset{QStringLiteral("custom")};
    bool hasCustomPosition{false};
    int customX{0};
    int customY{0};
    int customWidth{1280};
    int customHeight{720};
    QVector<SavedRegion> savedRegions;

    QString container{QStringLiteral("mp4")};
    QString video{QStringLiteral("h264")};
    QString audio{QStringLiteral("aac")};

    bool systemAudio{true};
    QString microphoneId;
    QString microphoneInputSource{QStringLiteral("stereo")};

    QString toggleRecord{QStringLiteral("F9")};
    QString togglePause{QStringLiteral("F10")};
    QString captureStill{QStringLiteral("F3")};
    QString selectTarget{QStringLiteral("F4")};
    bool toggleRecordEnabled{true};
    bool togglePauseEnabled{true};
    bool captureStillEnabled{true};
    bool selectTargetEnabled{true};

    bool includeCursor{true};
    bool alwaysOnTop{false};
    bool useTrayIcon{true};
    bool hideWhenMinimized{false};
    bool hideOnStartup{false};
    int frameRate{60};
    QString videoQuality{QStringLiteral("high")};
    int customBitrateKbps{12000};
    int keyframeInterval{5};
    QString resolutionAlign{QStringLiteral("8x4")};
    QString frameRateMode{QStringLiteral("vfr")};
    int storageUpdateSeconds{5};

    bool captureIncludeCursor{true};
    QString captureImageFormat{QStringLiteral("png")};

    bool gifIncludeCursor{true};
    int gifFrameRate{10};

    bool timeLimitEnabled{false};
    int timeLimitMinutes{10};
    int timeLimitSeconds{0};
    QString timeLimitAction{QStringLiteral("none")};

    bool watermarkEnabled{false};
    QString watermarkImagePath;
    int watermarkOpacity{100};
    int watermarkX{10};
    int watermarkY{10};
    bool watermarkApplyToCapture{true};

    bool useMultiCore{true};
    int encoderThreads{0};
    QString captureMode{QStringLiteral("dxgi")};
    int pipelineLayers{3};
};

class Config {
public:
    static QString defaultFilePath();
    static QString resolvedOutputDirectory(const ConfigData& data);
    static int videoBitrateKbps(const ConfigData& data, int width, int height);
    static int gopFrameCount(const ConfigData& data);
    static void alignCaptureSize(const ConfigData& data, int& width, int& height);
    static QString normalizedCaptureImageFormat(const QString& format);
    static QString containerExtension(const QString& container);
    static constexpr int kMinFrameRate = 1;
    static constexpr int kMaxFrameRate = 240;
    static constexpr int kMaxGifFrameRate = 60;
    static int normalizedFrameRate(int frameRate);
    static int normalizedGifFrameRate(int frameRate);
    static int normalizedStorageUpdateSeconds(int seconds);
    static int normalizedTimeLimitMinutes(int minutes);
    static int normalizedTimeLimitSeconds(int seconds);
    static QString normalizedTimeLimitAction(const QString& action);
    static int timeLimitDurationMs(const ConfigData& data);
    static int normalizedWatermarkOpacity(int opacity);
    static int normalizedWatermarkOffset(int value);
    static int normalizedEncoderThreads(int threads);
    static QString normalizedCaptureMode(const QString& mode);
    static int normalizedPipelineLayers(int layers);
    static int frameQueueCapacity(int pipelineLayers);
    static int effectiveEncoderThreads(const ConfigData& data);

    bool load(const QString& path = {});
    bool save(const QString& path = {}) const;

    ConfigData& data() { return data_; }
    const ConfigData& data() const { return data_; }

private:
    QString effectivePath(const QString& path) const;

    ConfigData data_{};
};

} // namespace ors
