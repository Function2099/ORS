#include "core/Config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Config round-trip preserves schema fields")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.json"));

    ors::Config config;
    config.data().language = QStringLiteral("en_US");
    config.data().lastTab = QStringLiteral("game");
    config.data().directory = QStringLiteral("D:/Videos/ORS");
    config.data().filenameTemplate = QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>");
    config.data().filenamePrefix = QStringLiteral("錄製");
    config.data().filenameStartNumber = 3;
    config.data().regionPreset = QStringLiteral("1080p");
    config.data().hasCustomPosition = true;
    config.data().customX = 120;
    config.data().customY = 80;
    config.data().customWidth = 1280;
    config.data().customHeight = 720;
    config.data().savedRegions.push_back({QStringLiteral("Stream"), 1920, 1080});
    config.data().container = QStringLiteral("wmv");
    config.data().video = QStringLiteral("wmv");
    config.data().audio = QStringLiteral("wma");
    config.data().systemAudio = false;
    config.data().microphoneId = QStringLiteral("mic-1");
    config.data().microphoneInputSource = QStringLiteral("left");
    config.data().toggleRecord = QStringLiteral("F8");
    config.data().togglePause = QStringLiteral("F7");
    config.data().captureStill = QStringLiteral("F5");
    config.data().selectTarget = QStringLiteral("Shift+F4");
    config.data().toggleRecordEnabled = false;
    config.data().togglePauseEnabled = true;
    config.data().captureStillEnabled = false;
    config.data().selectTargetEnabled = true;
    config.data().includeCursor = false;
    config.data().alwaysOnTop = true;
    config.data().useTrayIcon = false;
    config.data().frameRate = 60;
    config.data().videoQuality = QStringLiteral("medium");
    config.data().keyframeInterval = 8;
    config.data().resolutionAlign = QStringLiteral("16x16");
    config.data().frameRateMode = QStringLiteral("cfr");
    config.data().storageUpdateSeconds = 12;
    config.data().captureIncludeCursor = false;
    config.data().captureImageFormat = QStringLiteral("jpg");
    config.data().gifIncludeCursor = false;
    config.data().gifFrameRate = 12;
    config.data().timeLimitEnabled = true;
    config.data().timeLimitMinutes = 15;
    config.data().timeLimitSeconds = 30;
    config.data().timeLimitAction = QStringLiteral("restart");
    config.data().watermarkEnabled = true;
    config.data().watermarkImagePath = QStringLiteral("D:/logos/mark.png");
    config.data().watermarkOpacity = 80;
    config.data().watermarkX = 24;
    config.data().watermarkY = 48;
    config.data().watermarkApplyToCapture = false;
    config.data().useMultiCore = false;
    config.data().encoderThreads = 4;
    config.data().captureMode = QStringLiteral("gdi");
    config.data().pipelineLayers = 2;

    REQUIRE(config.save(path));
    REQUIRE(QFile::exists(path));

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE(loaded.data().version == 1);
    REQUIRE(loaded.data().language == QStringLiteral("en_US"));
    REQUIRE(loaded.data().lastTab == QStringLiteral("game"));
    REQUIRE(loaded.data().directory == QStringLiteral("D:/Videos/ORS"));
    REQUIRE(loaded.data().filenameTemplate == QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"));
    REQUIRE(loaded.data().filenamePrefix == QStringLiteral("錄製"));
    REQUIRE(loaded.data().filenameStartNumber == 3);
    REQUIRE(loaded.data().regionPreset == QStringLiteral("1080p"));
    REQUIRE(loaded.data().hasCustomPosition);
    REQUIRE(loaded.data().customX == 120);
    REQUIRE(loaded.data().customY == 80);
    REQUIRE(loaded.data().customWidth == 1280);
    REQUIRE(loaded.data().customHeight == 720);
    REQUIRE(loaded.data().savedRegions.size() == 1);
    REQUIRE(loaded.data().savedRegions.front().name == QStringLiteral("Stream"));
    REQUIRE(loaded.data().savedRegions.front().width == 1920);
    REQUIRE(loaded.data().container == QStringLiteral("wmv"));
    REQUIRE(loaded.data().systemAudio == false);
    REQUIRE(loaded.data().microphoneId == QStringLiteral("mic-1"));
    REQUIRE(loaded.data().microphoneInputSource == QStringLiteral("left"));
    REQUIRE(loaded.data().toggleRecord == QStringLiteral("F8"));
    REQUIRE(loaded.data().togglePause == QStringLiteral("F7"));
    REQUIRE(loaded.data().captureStill == QStringLiteral("F5"));
    REQUIRE(loaded.data().selectTarget == QStringLiteral("Shift+F4"));
    REQUIRE_FALSE(loaded.data().toggleRecordEnabled);
    REQUIRE(loaded.data().togglePauseEnabled);
    REQUIRE_FALSE(loaded.data().captureStillEnabled);
    REQUIRE(loaded.data().selectTargetEnabled);
    REQUIRE_FALSE(loaded.data().includeCursor);
    REQUIRE(loaded.data().alwaysOnTop);
    REQUIRE_FALSE(loaded.data().useTrayIcon);
    REQUIRE(loaded.data().frameRate == 60);
    REQUIRE(loaded.data().videoQuality == QStringLiteral("medium"));
    REQUIRE(loaded.data().keyframeInterval == 8);
    REQUIRE(loaded.data().resolutionAlign == QStringLiteral("16x16"));
    REQUIRE(loaded.data().frameRateMode == QStringLiteral("cfr"));
    REQUIRE(loaded.data().storageUpdateSeconds == 12);
    REQUIRE_FALSE(loaded.data().captureIncludeCursor);
    REQUIRE(loaded.data().captureImageFormat == QStringLiteral("jpg"));
    REQUIRE_FALSE(loaded.data().gifIncludeCursor);
    REQUIRE(loaded.data().gifFrameRate == 12);
    REQUIRE(loaded.data().timeLimitEnabled);
    REQUIRE(loaded.data().timeLimitMinutes == 15);
    REQUIRE(loaded.data().timeLimitSeconds == 30);
    REQUIRE(loaded.data().timeLimitAction == QStringLiteral("restart"));
    REQUIRE(loaded.data().watermarkEnabled);
    REQUIRE(loaded.data().watermarkImagePath == QStringLiteral("D:/logos/mark.png"));
    REQUIRE(loaded.data().watermarkOpacity == 80);
    REQUIRE(loaded.data().watermarkX == 24);
    REQUIRE(loaded.data().watermarkY == 48);
    REQUIRE_FALSE(loaded.data().watermarkApplyToCapture);
    REQUIRE_FALSE(loaded.data().useMultiCore);
    REQUIRE(loaded.data().encoderThreads == 4);
    REQUIRE(loaded.data().captureMode == QStringLiteral("gdi"));
    REQUIRE(loaded.data().pipelineLayers == 2);
}

TEST_CASE("gopFrameCount is keyframe seconds times fps")
{
    ors::ConfigData data;
    data.frameRate = 30;
    data.keyframeInterval = 5;
    REQUIRE(ors::Config::gopFrameCount(data) == 150);
    data.frameRate = 60;
    data.keyframeInterval = 1;
    REQUIRE(ors::Config::gopFrameCount(data) == 60);
}

TEST_CASE("Config missing file loads defaults")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("missing.json"));

    ors::Config config;
    REQUIRE(config.load(path));
    REQUIRE(config.data().version == 1);
    REQUIRE(config.data().language == QStringLiteral("zh_TW"));
    REQUIRE(config.data().lastTab == QStringLiteral("screen"));
    REQUIRE(config.data().filenameTemplate == QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"));
    REQUIRE(config.data().filenamePrefix == QStringLiteral("錄製"));
    REQUIRE(config.data().filenameStartNumber == 1);
    REQUIRE(config.data().container == QStringLiteral("mp4"));
    REQUIRE(config.data().microphoneInputSource == QStringLiteral("stereo"));
    REQUIRE(config.data().captureIncludeCursor);
    REQUIRE(config.data().captureImageFormat == QStringLiteral("png"));
    REQUIRE(config.data().gifIncludeCursor);
    REQUIRE(config.data().gifFrameRate == 10);
    REQUIRE_FALSE(config.data().timeLimitEnabled);
    REQUIRE(config.data().timeLimitMinutes == 10);
    REQUIRE(config.data().timeLimitSeconds == 0);
    REQUIRE(config.data().timeLimitAction == QStringLiteral("none"));
    REQUIRE_FALSE(config.data().watermarkEnabled);
    REQUIRE(config.data().watermarkImagePath.isEmpty());
    REQUIRE(config.data().watermarkOpacity == 100);
    REQUIRE(config.data().watermarkX == 10);
    REQUIRE(config.data().watermarkY == 10);
    REQUIRE(config.data().watermarkApplyToCapture);
    REQUIRE(config.data().useMultiCore);
    REQUIRE(config.data().encoderThreads == 0);
    REQUIRE(config.data().captureMode == QStringLiteral("dxgi"));
    REQUIRE(config.data().pipelineLayers == 3);
    REQUIRE(config.data().storageUpdateSeconds == 5);
    REQUIRE(config.data().toggleRecord == QStringLiteral("F9"));
    REQUIRE(config.data().togglePause == QStringLiteral("F10"));
    REQUIRE(config.data().captureStill == QStringLiteral("F3"));
    REQUIRE(config.data().selectTarget == QStringLiteral("F4"));
    REQUIRE(config.data().toggleRecordEnabled);
    REQUIRE(config.data().togglePauseEnabled);
    REQUIRE(config.data().captureStillEnabled);
    REQUIRE(config.data().selectTargetEnabled);
}

TEST_CASE("Config JSON uses nested schema keys")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("schema.json"));

    ors::Config config;
    REQUIRE(config.save(path));

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    REQUIRE(root.contains(QStringLiteral("version")));
    REQUIRE(root.contains(QStringLiteral("ui")));
    REQUIRE(root.contains(QStringLiteral("output")));
    REQUIRE(root.contains(QStringLiteral("region")));
    REQUIRE(root.contains(QStringLiteral("codec")));
    REQUIRE(root.contains(QStringLiteral("audio")));
    REQUIRE(root.contains(QStringLiteral("hotkeys")));
    const QJsonObject hotkeys = root.value(QStringLiteral("hotkeys")).toObject();
    REQUIRE(hotkeys.value(QStringLiteral("toggleRecord")).toString() == QStringLiteral("F9"));
    REQUIRE(hotkeys.value(QStringLiteral("togglePause")).toString() == QStringLiteral("F10"));
    REQUIRE(hotkeys.value(QStringLiteral("captureStill")).toString() == QStringLiteral("F3"));
    REQUIRE(hotkeys.value(QStringLiteral("selectTarget")).toString() == QStringLiteral("F4"));
    REQUIRE(hotkeys.value(QStringLiteral("toggleRecordEnabled")).toBool());
    REQUIRE(hotkeys.value(QStringLiteral("togglePauseEnabled")).toBool());
    REQUIRE(hotkeys.value(QStringLiteral("captureStillEnabled")).toBool());
    REQUIRE(hotkeys.value(QStringLiteral("selectTargetEnabled")).toBool());
    REQUIRE(root.contains(QStringLiteral("recording")));
    const QJsonObject recording = root.value(QStringLiteral("recording")).toObject();
    REQUIRE(recording.value(QStringLiteral("storageUpdateSeconds")).toInt() == 5);
    REQUIRE(root.contains(QStringLiteral("capture")));
    REQUIRE(root.contains(QStringLiteral("gif")));
    const QJsonObject capture = root.value(QStringLiteral("capture")).toObject();
    REQUIRE(capture.value(QStringLiteral("includeCursor")).toBool());
    REQUIRE(capture.value(QStringLiteral("imageFormat")).toString() == QStringLiteral("png"));
    const QJsonObject gif = root.value(QStringLiteral("gif")).toObject();
    REQUIRE(gif.value(QStringLiteral("includeCursor")).toBool());
    REQUIRE(gif.value(QStringLiteral("frameRate")).toInt() == 10);
    REQUIRE(root.contains(QStringLiteral("timeLimit")));
    const QJsonObject timeLimit = root.value(QStringLiteral("timeLimit")).toObject();
    REQUIRE_FALSE(timeLimit.value(QStringLiteral("enabled")).toBool());
    REQUIRE(timeLimit.value(QStringLiteral("minutes")).toInt() == 10);
    REQUIRE(timeLimit.value(QStringLiteral("seconds")).toInt() == 0);
    REQUIRE(timeLimit.value(QStringLiteral("action")).toString() == QStringLiteral("none"));
    REQUIRE(root.contains(QStringLiteral("watermark")));
    const QJsonObject watermark = root.value(QStringLiteral("watermark")).toObject();
    REQUIRE_FALSE(watermark.value(QStringLiteral("enabled")).toBool());
    REQUIRE(watermark.value(QStringLiteral("opacity")).toInt() == 100);
    REQUIRE(watermark.value(QStringLiteral("x")).toInt() == 10);
    REQUIRE(watermark.value(QStringLiteral("y")).toInt() == 10);
    REQUIRE(watermark.value(QStringLiteral("applyToCapture")).toBool());
    REQUIRE(root.contains(QStringLiteral("performance")));
    const QJsonObject performance = root.value(QStringLiteral("performance")).toObject();
    REQUIRE(performance.value(QStringLiteral("useMultiCore")).toBool());
    REQUIRE(performance.value(QStringLiteral("encoderThreads")).toInt() == 0);
    REQUIRE(performance.value(QStringLiteral("captureMode")).toString() == QStringLiteral("dxgi"));
    REQUIRE(performance.value(QStringLiteral("pipelineLayers")).toInt() == 3);
}

TEST_CASE("capture.imageFormat jpeg aliases to jpg")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("alias.json"));

    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QByteArrayLiteral(
        "{ \"version\": 1, \"capture\": { \"includeCursor\": false, \"imageFormat\": \"jpeg\" } }"));
    file.close();

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE_FALSE(loaded.data().captureIncludeCursor);
    REQUIRE(loaded.data().captureImageFormat == QStringLiteral("jpg"));
}

TEST_CASE("normalizedCaptureImageFormat maps aliases and fallbacks")
{
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("JPEG")) == QStringLiteral("jpg"));
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("bmp")) == QStringLiteral("bmp"));
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("webp")) == QStringLiteral("png"));
}

TEST_CASE("containerExtension maps mp4 wmv and gif")
{
    REQUIRE(ors::Config::containerExtension(QStringLiteral("mp4")) == QStringLiteral(".mp4"));
    REQUIRE(ors::Config::containerExtension(QStringLiteral("WMV")) == QStringLiteral(".wmv"));
    REQUIRE(ors::Config::containerExtension(QStringLiteral("gif")) == QStringLiteral(".gif"));
    REQUIRE(ors::Config::containerExtension(QStringLiteral("avi")) == QStringLiteral(".mp4"));
}

TEST_CASE("normalizedFrameRate clamps recording and GIF to 1-60")
{
    REQUIRE(ors::Config::normalizedFrameRate(0) == 1);
    REQUIRE(ors::Config::normalizedFrameRate(60) == 60);
    REQUIRE(ors::Config::normalizedFrameRate(120) == 60);
    REQUIRE(ors::Config::normalizedGifFrameRate(0) == 1);
    REQUIRE(ors::Config::normalizedGifFrameRate(10) == 10);
    REQUIRE(ors::Config::normalizedGifFrameRate(50) == 50);
    REQUIRE(ors::Config::normalizedGifFrameRate(120) == 60);
}

TEST_CASE("normalizedStorageUpdateSeconds clamps to 1-999")
{
    REQUIRE(ors::Config::normalizedStorageUpdateSeconds(0) == 1);
    REQUIRE(ors::Config::normalizedStorageUpdateSeconds(5) == 5);
    REQUIRE(ors::Config::normalizedStorageUpdateSeconds(999) == 999);
    REQUIRE(ors::Config::normalizedStorageUpdateSeconds(2000) == 999);
}

TEST_CASE("timeLimit helpers clamp duration and action")
{
    REQUIRE(ors::Config::normalizedTimeLimitMinutes(-1) == 0);
    REQUIRE(ors::Config::normalizedTimeLimitMinutes(10) == 10);
    REQUIRE(ors::Config::normalizedTimeLimitMinutes(2000) == 999);
    REQUIRE(ors::Config::normalizedTimeLimitSeconds(-3) == 0);
    REQUIRE(ors::Config::normalizedTimeLimitSeconds(30) == 30);
    REQUIRE(ors::Config::normalizedTimeLimitSeconds(90) == 59);
    REQUIRE(ors::Config::normalizedTimeLimitAction(QStringLiteral("RESTART")) == QStringLiteral("restart"));
    REQUIRE(ors::Config::normalizedTimeLimitAction(QStringLiteral("quit")) == QStringLiteral("quit"));
    REQUIRE(ors::Config::normalizedTimeLimitAction(QStringLiteral("shutdown")) == QStringLiteral("shutdown"));
    REQUIRE(ors::Config::normalizedTimeLimitAction(QStringLiteral("sleep")) == QStringLiteral("sleep"));
    REQUIRE(ors::Config::normalizedTimeLimitAction(QStringLiteral("explode")) == QStringLiteral("none"));

    ors::ConfigData data;
    data.timeLimitMinutes = 10;
    data.timeLimitSeconds = 0;
    REQUIRE(ors::Config::timeLimitDurationMs(data) == 600000);
    data.timeLimitMinutes = 0;
    data.timeLimitSeconds = 0;
    REQUIRE(ors::Config::timeLimitDurationMs(data) == 1000);
}

TEST_CASE("timeLimit missing block keeps defaults and clamps invalid values")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString missingPath = dir.filePath(QStringLiteral("no-limit.json"));
    QFile missing(missingPath);
    REQUIRE(missing.open(QIODevice::WriteOnly | QIODevice::Truncate));
    missing.write(QByteArrayLiteral("{ \"version\": 1 }"));
    missing.close();

    ors::Config withoutBlock;
    REQUIRE(withoutBlock.load(missingPath));
    REQUIRE_FALSE(withoutBlock.data().timeLimitEnabled);
    REQUIRE(withoutBlock.data().timeLimitMinutes == 10);
    REQUIRE(withoutBlock.data().timeLimitSeconds == 0);
    REQUIRE(withoutBlock.data().timeLimitAction == QStringLiteral("none"));

    const QString invalidPath = dir.filePath(QStringLiteral("bad-limit.json"));
    QFile invalid(invalidPath);
    REQUIRE(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalid.write(QByteArrayLiteral(
        "{ \"version\": 1, \"timeLimit\": { \"enabled\": true, \"minutes\": 2000, "
        "\"seconds\": 90, \"action\": \"explode\" } }"));
    invalid.close();

    ors::Config loaded;
    REQUIRE(loaded.load(invalidPath));
    REQUIRE(loaded.data().timeLimitEnabled);
    REQUIRE(loaded.data().timeLimitMinutes == 999);
    REQUIRE(loaded.data().timeLimitSeconds == 59);
    REQUIRE(loaded.data().timeLimitAction == QStringLiteral("none"));
}

TEST_CASE("watermark and performance helpers clamp invalid values")
{
    REQUIRE(ors::Config::normalizedWatermarkOpacity(0) == 1);
    REQUIRE(ors::Config::normalizedWatermarkOpacity(80) == 80);
    REQUIRE(ors::Config::normalizedWatermarkOpacity(200) == 100);
    REQUIRE(ors::Config::normalizedWatermarkOffset(-20000) == -9999);
    REQUIRE(ors::Config::normalizedWatermarkOffset(24) == 24);
    REQUIRE(ors::Config::normalizedEncoderThreads(-3) == 0);
    REQUIRE(ors::Config::normalizedEncoderThreads(0) == 0);
    REQUIRE(ors::Config::normalizedEncoderThreads(4) == 4);
    REQUIRE(ors::Config::normalizedEncoderThreads(99) == 16);
    REQUIRE(ors::Config::normalizedCaptureMode(QStringLiteral("GDI")) == QStringLiteral("gdi"));
    REQUIRE(ors::Config::normalizedCaptureMode(QStringLiteral("wgc")) == QStringLiteral("dxgi"));
    REQUIRE(ors::Config::normalizedPipelineLayers(-1) == 0);
    REQUIRE(ors::Config::normalizedPipelineLayers(2) == 2);
    REQUIRE(ors::Config::normalizedPipelineLayers(9) == 3);
    REQUIRE(ors::Config::frameQueueCapacity(3) == 4);
    REQUIRE(ors::Config::frameQueueCapacity(2) == 2);
    REQUIRE(ors::Config::frameQueueCapacity(0) == 1);

    ors::ConfigData data;
    REQUIRE(ors::Config::effectiveEncoderThreads(data) == 0);
    data.useMultiCore = false;
    data.encoderThreads = 8;
    REQUIRE(ors::Config::effectiveEncoderThreads(data) == 1);
}

TEST_CASE("watermark and performance missing blocks keep defaults")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("no-extra.json"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QByteArrayLiteral("{ \"version\": 1 }"));
    file.close();

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE_FALSE(loaded.data().watermarkEnabled);
    REQUIRE(loaded.data().watermarkOpacity == 100);
    REQUIRE(loaded.data().watermarkApplyToCapture);
    REQUIRE(loaded.data().useMultiCore);
    REQUIRE(loaded.data().encoderThreads == 0);
    REQUIRE(loaded.data().captureMode == QStringLiteral("dxgi"));
    REQUIRE(loaded.data().pipelineLayers == 3);

    const QString invalidPath = dir.filePath(QStringLiteral("bad-extra.json"));
    QFile invalid(invalidPath);
    REQUIRE(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalid.write(QByteArrayLiteral(
        "{ \"version\": 1, \"watermark\": { \"enabled\": true, \"opacity\": 0, \"x\": -20000 }, "
        "\"performance\": { \"encoderThreads\": 99, \"captureMode\": \"wgc\", "
        "\"pipelineLayers\": 1 } }"));
    invalid.close();

    ors::Config clamped;
    REQUIRE(clamped.load(invalidPath));
    REQUIRE(clamped.data().watermarkEnabled);
    REQUIRE(clamped.data().watermarkOpacity == 1);
    REQUIRE(clamped.data().watermarkX == -9999);
    REQUIRE(clamped.data().encoderThreads == 16);
    REQUIRE(clamped.data().captureMode == QStringLiteral("dxgi"));
    REQUIRE(clamped.data().pipelineLayers == 3);
}

TEST_CASE("Empty output directory resolves to movies or home")
{
    ors::ConfigData data;
    const QString resolved = ors::Config::resolvedOutputDirectory(data);
    REQUIRE_FALSE(resolved.isEmpty());
}

TEST_CASE("alignCaptureSize snaps to 8x4")
{
    ors::ConfigData data;
    data.resolutionAlign = QStringLiteral("8x4");
    int width = 1920;
    int height = 1081;
    ors::Config::alignCaptureSize(data, width, height);
    REQUIRE(width == 1920);
    REQUIRE(height == 1080);
}

TEST_CASE("videoBitrateKbps scales with quality")
{
    ors::ConfigData data;
    data.frameRate = 30;
    data.videoQuality = QStringLiteral("low");
    const int low = ors::Config::videoBitrateKbps(data, 1280, 720);
    data.videoQuality = QStringLiteral("very-high");
    const int high = ors::Config::videoBitrateKbps(data, 1280, 720);
    REQUIRE(high > low);
}
