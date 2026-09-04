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
    QString filenameTemplate{QStringLiteral("ORS_yyyyMMdd_HHmmss")};

    QString regionPreset{QStringLiteral("monitor-primary")};
    int customWidth{1920};
    int customHeight{1080};
    QVector<SavedRegion> savedRegions;

    QString container{QStringLiteral("mp4")};
    QString video{QStringLiteral("h264")};
    QString audio{QStringLiteral("aac")};

    bool systemAudio{true};
    QString microphoneId;

    QString toggleRecord{QStringLiteral("F9")};
    QString togglePause{QStringLiteral("F10")};
};

class Config {
public:
    static QString defaultFilePath();
    static QString resolvedOutputDirectory(const ConfigData& data);

    bool load(const QString& path = {});
    bool save(const QString& path = {}) const;

    ConfigData& data() { return data_; }
    const ConfigData& data() const { return data_; }

private:
    QString effectivePath(const QString& path) const;

    ConfigData data_{};
};

} // namespace ors
