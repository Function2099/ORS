#include "core/Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
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

int Config::videoBitrateKbps(const ConfigData& data, int width, int height)
{
    const int fps = normalizedFrameRate(data.frameRate);
    const int base = std::max(4000, std::max(1, width) * std::max(1, height) * fps / 8000);
    if (data.videoQuality == QLatin1String("custom")) {
        return std::clamp(data.customBitrateKbps, 500, 100000);
    }
    if (data.videoQuality == QLatin1String("very-high")) {
        return std::max(6000, base * 3 / 2);
    }
    if (data.videoQuality == QLatin1String("medium")) {
        return std::max(2000, base * 3 / 5);
    }
    if (data.videoQuality == QLatin1String("low")) {
        return std::max(1000, base / 3);
    }
    return base;
}

int Config::gopFrameCount(const ConfigData& data)
{
    const int fps = normalizedFrameRate(data.frameRate);
    const int seconds = std::clamp(data.keyframeInterval, 1, 30);
    return std::clamp(seconds * fps, 1, 3600);
}

QString Config::normalizedCaptureImageFormat(const QString& format)
{
    const QString lower = format.trimmed().toLower();
    if (lower == QLatin1String("jpg") || lower == QLatin1String("jpeg")) {
        return QStringLiteral("jpg");
    }
    if (lower == QLatin1String("bmp")) {
        return QStringLiteral("bmp");
    }
    return QStringLiteral("png");
}

QString Config::containerExtension(const QString& container)
{
    if (container.compare(QLatin1String("wmv"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral(".wmv");
    }
    if (container.compare(QLatin1String("gif"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral(".gif");
    }
    return QStringLiteral(".mp4");
}

int Config::normalizedFrameRate(int frameRate)
{
    return std::clamp(frameRate, kMinFrameRate, kMaxFrameRate);
}

int Config::normalizedGifFrameRate(int frameRate)
{
    return normalizedFrameRate(frameRate);
}

int Config::normalizedStorageUpdateSeconds(int seconds)
{
    return std::clamp(seconds, 1, 999);
}

int Config::normalizedTimeLimitMinutes(int minutes)
{
    return std::clamp(minutes, 0, 999);
}

int Config::normalizedTimeLimitSeconds(int seconds)
{
    return std::clamp(seconds, 0, 59);
}

QString Config::normalizedTimeLimitAction(const QString& action)
{
    const QString lower = action.trimmed().toLower();
    if (lower == QLatin1String("restart") || lower == QLatin1String("quit")
        || lower == QLatin1String("shutdown") || lower == QLatin1String("sleep")) {
        return lower;
    }
    return QStringLiteral("none");
}

int Config::timeLimitDurationMs(const ConfigData& data)
{
    const int minutes = normalizedTimeLimitMinutes(data.timeLimitMinutes);
    const int seconds = normalizedTimeLimitSeconds(data.timeLimitSeconds);
    return std::max(1, minutes * 60 + seconds) * 1000;
}

int Config::normalizedWatermarkOpacity(int opacity)
{
    return std::clamp(opacity, 1, 100);
}

int Config::normalizedWatermarkOffset(int value)
{
    return std::clamp(value, -9999, 9999);
}

int Config::normalizedEncoderThreads(int threads)
{
    if (threads <= 0) {
        return 0;
    }
    return std::clamp(threads, 1, 16);
}

QString Config::normalizedCaptureMode(const QString& mode)
{
    if (mode.trimmed().compare(QLatin1String("gdi"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("gdi");
    }
    return QStringLiteral("dxgi");
}

int Config::normalizedPipelineLayers(int layers)
{
    if (layers <= 0) {
        return 0;
    }
    if (layers == 2) {
        return 2;
    }
    return 3;
}

int Config::frameQueueCapacity(int pipelineLayers)
{
    const int layers = normalizedPipelineLayers(pipelineLayers);
    if (layers <= 0) {
        return 1;
    }
    if (layers == 2) {
        return 2;
    }
    return 4;
}

int Config::effectiveEncoderThreads(const ConfigData& data)
{
    if (!data.useMultiCore) {
        return 1;
    }
    return normalizedEncoderThreads(data.encoderThreads);
}

void Config::alignCaptureSize(const ConfigData& data, int& width, int& height)
{
    int alignW = 2;
    int alignH = 2;
    if (data.resolutionAlign == QLatin1String("8x4")) {
        alignW = 8;
        alignH = 4;
    } else if (data.resolutionAlign == QLatin1String("16x16")) {
        alignW = 16;
        alignH = 16;
    }
    width = std::max(alignW, width / alignW * alignW);
    height = std::max(alignH, height / alignH * alignH);
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
    if (next.filenameTemplate == QLatin1String("ORS_yyyyMMdd_HHmmss")) {
        next.filenameTemplate = QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>");
    }
    next.filenamePrefix = readString(output, "filenamePrefix", next.filenamePrefix);
    next.filenameStartNumber = std::max(1, readInt(output, "filenameStartNumber", next.filenameStartNumber));

    const QJsonObject region = objectOrEmpty(root, "region");
    next.regionPreset = readString(region, "preset", next.regionPreset);
    next.customWidth = readInt(region, "customWidth", next.customWidth);
    next.customHeight = readInt(region, "customHeight", next.customHeight);
    const auto xIt = region.constFind(QLatin1String("customX"));
    const auto yIt = region.constFind(QLatin1String("customY"));
    if (xIt != region.constEnd() && yIt != region.constEnd()) {
        next.hasCustomPosition = true;
        next.customX = readInt(region, "customX", next.customX);
        next.customY = readInt(region, "customY", next.customY);
    }
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
    next.microphoneInputSource = readString(
        audio, "microphoneInputSource", next.microphoneInputSource);
    if (next.microphoneInputSource.compare(QLatin1String("left"), Qt::CaseInsensitive) != 0
        && next.microphoneInputSource.compare(QLatin1String("right"), Qt::CaseInsensitive) != 0) {
        next.microphoneInputSource = QStringLiteral("stereo");
    } else {
        next.microphoneInputSource = next.microphoneInputSource.toLower();
    }

    const QJsonObject hotkeys = objectOrEmpty(root, "hotkeys");
    next.toggleRecord = readString(hotkeys, "toggleRecord", next.toggleRecord);
    next.togglePause = readString(hotkeys, "togglePause", next.togglePause);
    next.captureStill = readString(hotkeys, "captureStill", next.captureStill);
    next.selectTarget = readString(hotkeys, "selectTarget", next.selectTarget);
    next.toggleRecordEnabled = readBool(hotkeys, "toggleRecordEnabled", next.toggleRecordEnabled);
    next.togglePauseEnabled = readBool(hotkeys, "togglePauseEnabled", next.togglePauseEnabled);
    next.captureStillEnabled = readBool(hotkeys, "captureStillEnabled", next.captureStillEnabled);
    next.selectTargetEnabled = readBool(hotkeys, "selectTargetEnabled", next.selectTargetEnabled);

    const QJsonObject recording = objectOrEmpty(root, "recording");
    next.includeCursor = readBool(recording, "includeCursor", next.includeCursor);
    next.alwaysOnTop = readBool(recording, "alwaysOnTop", next.alwaysOnTop);
    next.useTrayIcon = readBool(recording, "useTrayIcon", next.useTrayIcon);
    next.hideWhenMinimized = readBool(recording, "hideWhenMinimized", next.hideWhenMinimized);
    next.hideOnStartup = readBool(recording, "hideOnStartup", next.hideOnStartup);
    next.frameRate = normalizedFrameRate(readInt(recording, "frameRate", next.frameRate));
    next.videoQuality = readString(recording, "quality", next.videoQuality);
    next.customBitrateKbps = std::clamp(
        readInt(recording, "customBitrateKbps", next.customBitrateKbps), 500, 100000);
    next.keyframeInterval = std::clamp(
        readInt(recording, "keyframeInterval", next.keyframeInterval), 1, 30);
    next.resolutionAlign = readString(recording, "resolutionAlign", next.resolutionAlign);
    next.frameRateMode = readString(recording, "frameRateMode", next.frameRateMode);
    next.storageUpdateSeconds = normalizedStorageUpdateSeconds(
        readInt(recording, "storageUpdateSeconds", next.storageUpdateSeconds));

    const QJsonObject capture = objectOrEmpty(root, "capture");
    next.captureIncludeCursor = readBool(capture, "includeCursor", next.captureIncludeCursor);
    next.captureImageFormat = normalizedCaptureImageFormat(
        readString(capture, "imageFormat", next.captureImageFormat));

    const QJsonObject gif = objectOrEmpty(root, "gif");
    next.gifIncludeCursor = readBool(gif, "includeCursor", next.gifIncludeCursor);
    next.gifFrameRate = normalizedGifFrameRate(readInt(gif, "frameRate", next.gifFrameRate));

    const QJsonObject timeLimit = objectOrEmpty(root, "timeLimit");
    next.timeLimitEnabled = readBool(timeLimit, "enabled", next.timeLimitEnabled);
    next.timeLimitMinutes = normalizedTimeLimitMinutes(
        readInt(timeLimit, "minutes", next.timeLimitMinutes));
    next.timeLimitSeconds = normalizedTimeLimitSeconds(
        readInt(timeLimit, "seconds", next.timeLimitSeconds));
    next.timeLimitAction = normalizedTimeLimitAction(
        readString(timeLimit, "action", next.timeLimitAction));

    const QJsonObject watermark = objectOrEmpty(root, "watermark");
    next.watermarkEnabled = readBool(watermark, "enabled", next.watermarkEnabled);
    next.watermarkImagePath = readString(watermark, "imagePath", next.watermarkImagePath);
    next.watermarkOpacity = normalizedWatermarkOpacity(
        readInt(watermark, "opacity", next.watermarkOpacity));
    next.watermarkX = normalizedWatermarkOffset(readInt(watermark, "x", next.watermarkX));
    next.watermarkY = normalizedWatermarkOffset(readInt(watermark, "y", next.watermarkY));
    next.watermarkApplyToCapture = readBool(
        watermark, "applyToCapture", next.watermarkApplyToCapture);

    const QJsonObject performance = objectOrEmpty(root, "performance");
    next.useMultiCore = readBool(performance, "useMultiCore", next.useMultiCore);
    next.encoderThreads = normalizedEncoderThreads(
        readInt(performance, "encoderThreads", next.encoderThreads));
    next.captureMode = normalizedCaptureMode(
        readString(performance, "captureMode", next.captureMode));
    next.pipelineLayers = normalizedPipelineLayers(
        readInt(performance, "pipelineLayers", next.pipelineLayers));

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
    output.insert(QStringLiteral("filenamePrefix"), data_.filenamePrefix);
    output.insert(QStringLiteral("filenameStartNumber"), data_.filenameStartNumber);

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
    region.insert(QStringLiteral("customX"), data_.customX);
    region.insert(QStringLiteral("customY"), data_.customY);
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
    audio.insert(QStringLiteral("microphoneInputSource"), data_.microphoneInputSource);

    QJsonObject hotkeys;
    hotkeys.insert(QStringLiteral("toggleRecord"), data_.toggleRecord);
    hotkeys.insert(QStringLiteral("togglePause"), data_.togglePause);
    hotkeys.insert(QStringLiteral("captureStill"), data_.captureStill);
    hotkeys.insert(QStringLiteral("selectTarget"), data_.selectTarget);
    hotkeys.insert(QStringLiteral("toggleRecordEnabled"), data_.toggleRecordEnabled);
    hotkeys.insert(QStringLiteral("togglePauseEnabled"), data_.togglePauseEnabled);
    hotkeys.insert(QStringLiteral("captureStillEnabled"), data_.captureStillEnabled);
    hotkeys.insert(QStringLiteral("selectTargetEnabled"), data_.selectTargetEnabled);

    QJsonObject recording;
    recording.insert(QStringLiteral("includeCursor"), data_.includeCursor);
    recording.insert(QStringLiteral("alwaysOnTop"), data_.alwaysOnTop);
    recording.insert(QStringLiteral("useTrayIcon"), data_.useTrayIcon);
    recording.insert(QStringLiteral("hideWhenMinimized"), data_.hideWhenMinimized);
    recording.insert(QStringLiteral("hideOnStartup"), data_.hideOnStartup);
    recording.insert(QStringLiteral("frameRate"), normalizedFrameRate(data_.frameRate));
    recording.insert(QStringLiteral("quality"), data_.videoQuality);
    recording.insert(QStringLiteral("customBitrateKbps"), data_.customBitrateKbps);
    recording.insert(QStringLiteral("keyframeInterval"), data_.keyframeInterval);
    recording.insert(QStringLiteral("resolutionAlign"), data_.resolutionAlign);
    recording.insert(QStringLiteral("frameRateMode"), data_.frameRateMode);
    recording.insert(
        QStringLiteral("storageUpdateSeconds"),
        normalizedStorageUpdateSeconds(data_.storageUpdateSeconds));

    QJsonObject capture;
    capture.insert(QStringLiteral("includeCursor"), data_.captureIncludeCursor);
    capture.insert(QStringLiteral("imageFormat"), normalizedCaptureImageFormat(data_.captureImageFormat));

    QJsonObject gif;
    gif.insert(QStringLiteral("includeCursor"), data_.gifIncludeCursor);
    gif.insert(QStringLiteral("frameRate"), normalizedGifFrameRate(data_.gifFrameRate));

    QJsonObject timeLimit;
    timeLimit.insert(QStringLiteral("enabled"), data_.timeLimitEnabled);
    timeLimit.insert(QStringLiteral("minutes"), normalizedTimeLimitMinutes(data_.timeLimitMinutes));
    timeLimit.insert(QStringLiteral("seconds"), normalizedTimeLimitSeconds(data_.timeLimitSeconds));
    timeLimit.insert(QStringLiteral("action"), normalizedTimeLimitAction(data_.timeLimitAction));

    QJsonObject watermark;
    watermark.insert(QStringLiteral("enabled"), data_.watermarkEnabled);
    watermark.insert(QStringLiteral("imagePath"), data_.watermarkImagePath);
    watermark.insert(QStringLiteral("opacity"), normalizedWatermarkOpacity(data_.watermarkOpacity));
    watermark.insert(QStringLiteral("x"), normalizedWatermarkOffset(data_.watermarkX));
    watermark.insert(QStringLiteral("y"), normalizedWatermarkOffset(data_.watermarkY));
    watermark.insert(QStringLiteral("applyToCapture"), data_.watermarkApplyToCapture);

    QJsonObject performance;
    performance.insert(QStringLiteral("useMultiCore"), data_.useMultiCore);
    performance.insert(QStringLiteral("encoderThreads"), normalizedEncoderThreads(data_.encoderThreads));
    performance.insert(QStringLiteral("captureMode"), normalizedCaptureMode(data_.captureMode));
    performance.insert(QStringLiteral("pipelineLayers"), normalizedPipelineLayers(data_.pipelineLayers));

    QJsonObject root;
    root.insert(QStringLiteral("version"), data_.version);
    root.insert(QStringLiteral("ui"), ui);
    root.insert(QStringLiteral("output"), output);
    root.insert(QStringLiteral("region"), region);
    root.insert(QStringLiteral("codec"), codec);
    root.insert(QStringLiteral("audio"), audio);
    root.insert(QStringLiteral("hotkeys"), hotkeys);
    root.insert(QStringLiteral("recording"), recording);
    root.insert(QStringLiteral("capture"), capture);
    root.insert(QStringLiteral("gif"), gif);
    root.insert(QStringLiteral("timeLimit"), timeLimit);
    root.insert(QStringLiteral("watermark"), watermark);
    root.insert(QStringLiteral("performance"), performance);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace ors
