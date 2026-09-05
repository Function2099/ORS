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

    bool includeCursor{true};
    bool alwaysOnTop{false};
    bool useTrayIcon{true};
    bool hideWhenMinimized{false};
    bool hideOnStartup{false};
    int frameRate{60};
    QString videoQuality{QStringLiteral("very-high")};
    int customBitrateKbps{12000};
    int keyframeInterval{5};
    QString resolutionAlign{QStringLiteral("8x4")};
    QString frameRateMode{QStringLiteral("vfr")};

    bool captureIncludeCursor{true};
    QString captureImageFormat{QStringLiteral("png")};
};

class Config {
public:
    static QString defaultFilePath();
    static QString resolvedOutputDirectory(const ConfigData& data);
    static int videoBitrateKbps(const ConfigData& data, int width, int height);
    static int gopFrameCount(const ConfigData& data);
    static void alignCaptureSize(const ConfigData& data, int& width, int& height);
    static QString normalizedCaptureImageFormat(const QString& format);

    bool load(const QString& path = {});
    bool save(const QString& path = {}) const;

    ConfigData& data() { return data_; }
    const ConfigData& data() const { return data_; }

private:
    QString effectivePath(const QString& path) const;

    ConfigData data_{};
};

} // namespace ors
