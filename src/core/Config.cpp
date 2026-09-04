#include "core/Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <utility>

namespace ors {
namespace {

QString readString(const QJsonObject& obj, const char* key, const QString& fallback)
{
    const auto it = obj.constFind(QLatin1String(key));
    if (it == obj.constEnd() || !it->isString()) {
        return fallback;
    }
    return it->toString();
}

int readInt(const QJsonObject& obj, const char* key, int fallback)
{
    const auto it = obj.constFind(QLatin1String(key));
    if (it == obj.constEnd() || !it->isDouble()) {
        return fallback;
    }
    return it->toInt(fallback);
}

bool readBool(const QJsonObject& obj, const char* key, bool fallback)
{
    const auto it = obj.constFind(QLatin1String(key));
    if (it == obj.constEnd() || !it->isBool()) {
        return fallback;
    }
    return it->toBool(fallback);
}

QJsonObject objectOrEmpty(const QJsonObject& parent, const char* key)
{
    const auto it = parent.constFind(QLatin1String(key));
    if (it == parent.constEnd() || !it->isObject()) {
        return {};
    }
    return it->toObject();
}

} // namespace

QString Config::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/ORS");
    return dir + QStringLiteral("/config.json");
}

QString Config::resolvedOutputDirectory(const ConfigData& data)
{
    const QString trimmed = data.directory.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (!movies.isEmpty()) {
        return movies;
    }
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString Config::effectivePath(const QString& path) const
{
    return path.isEmpty() ? defaultFilePath() : path;
}

bool Config::load(const QString& path)
{
    const QString filePath = effectivePath(path);
    QFile file(filePath);
    if (!file.exists()) {
        data_ = ConfigData{};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    ConfigData next;

    next.version = readInt(root, "version", 1);

    const QJsonObject ui = objectOrEmpty(root, "ui");
    next.language = readString(ui, "language", next.language);
    next.lastTab = readString(ui, "lastTab", next.lastTab);

    const QJsonObject output = objectOrEmpty(root, "output");
    next.directory = readString(output, "directory", next.directory);
    next.filenameTemplate = readString(output, "filenameTemplate", next.filenameTemplate);

    const QJsonObject region = objectOrEmpty(root, "region");
    next.regionPreset = readString(region, "preset", next.regionPreset);
    next.customWidth = readInt(region, "customWidth", next.customWidth);
    next.customHeight = readInt(region, "customHeight", next.customHeight);
    const auto savedIt = region.constFind(QLatin1String("saved"));
    if (savedIt != region.constEnd() && savedIt->isArray()) {
        for (const QJsonValue& value : savedIt->toArray()) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject obj = value.toObject();
            SavedRegion saved;
            saved.name = readString(obj, "name", {});
            saved.width = readInt(obj, "width", 0);
            saved.height = readInt(obj, "height", 0);
            next.savedRegions.push_back(std::move(saved));
        }
    }

    const QJsonObject codec = objectOrEmpty(root, "codec");
    next.container = readString(codec, "container", next.container);
    next.video = readString(codec, "video", next.video);
    next.audio = readString(codec, "audio", next.audio);

    const QJsonObject audio = objectOrEmpty(root, "audio");
    next.systemAudio = readBool(audio, "system", next.systemAudio);
    next.microphoneId = readString(audio, "microphoneId", next.microphoneId);

    const QJsonObject hotkeys = objectOrEmpty(root, "hotkeys");
    next.toggleRecord = readString(hotkeys, "toggleRecord", next.toggleRecord);
    next.togglePause = readString(hotkeys, "togglePause", next.togglePause);

    data_ = std::move(next);
    return true;
}

bool Config::save(const QString& path) const
{
    const QString filePath = effectivePath(path);
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QJsonObject ui;
    ui.insert(QStringLiteral("language"), data_.language);
    ui.insert(QStringLiteral("lastTab"), data_.lastTab);

    QJsonObject output;
    output.insert(QStringLiteral("directory"), data_.directory);
    output.insert(QStringLiteral("filenameTemplate"), data_.filenameTemplate);

    QJsonArray saved;
    for (const SavedRegion& region : data_.savedRegions) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), region.name);
        obj.insert(QStringLiteral("width"), region.width);
        obj.insert(QStringLiteral("height"), region.height);
        saved.append(obj);
    }

    QJsonObject region;
    region.insert(QStringLiteral("preset"), data_.regionPreset);
    region.insert(QStringLiteral("customWidth"), data_.customWidth);
    region.insert(QStringLiteral("customHeight"), data_.customHeight);
    region.insert(QStringLiteral("saved"), saved);

    QJsonObject codec;
    codec.insert(QStringLiteral("container"), data_.container);
    codec.insert(QStringLiteral("video"), data_.video);
    codec.insert(QStringLiteral("audio"), data_.audio);

    QJsonObject audio;
    audio.insert(QStringLiteral("system"), data_.systemAudio);
    audio.insert(QStringLiteral("microphoneId"), data_.microphoneId);

    QJsonObject hotkeys;
    hotkeys.insert(QStringLiteral("toggleRecord"), data_.toggleRecord);
    hotkeys.insert(QStringLiteral("togglePause"), data_.togglePause);

    QJsonObject root;
    root.insert(QStringLiteral("version"), data_.version);
    root.insert(QStringLiteral("ui"), ui);
    root.insert(QStringLiteral("output"), output);
    root.insert(QStringLiteral("region"), region);
    root.insert(QStringLiteral("codec"), codec);
    root.insert(QStringLiteral("audio"), audio);
    root.insert(QStringLiteral("hotkeys"), hotkeys);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace ors
