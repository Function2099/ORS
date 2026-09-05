#include "ui/MainWindow.h"

#include "audio/AudioDevices.h"
#include "core/FilenameTemplate.h"
#include "ui/RegionOverlay.h"
#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcons.h"

#ifdef Q_OS_WIN
#include "capture/CaptureWin.h"
#include "core/VideoFrame.h"
#endif

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace ors {
namespace {

const QColor kIconColor(0x1f, 0x4e, 0x4c);
const QColor kHeaderIcon(0xec, 0xf4, 0xf3);
const QColor kRecordIcon(0xff, 0xff, 0xff);

QPushButton* makeModeButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(QStringLiteral("modeButton"));
    return button;
}

void applyActionButtonSize(QWidget* button)
{
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumSize(64, 70);
    button->setMaximumHeight(74);
}

QToolButton* makeActionButton(QWidget* parent, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(28, 28));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(objectName);
    applyActionButtonSize(button);
    return button;
}

QRect centeredOn(const QRect& screen, int width, int height)
{
    const int x = screen.x() + qMax(0, (screen.width() - width) / 2);
    const int y = screen.y() + qMax(0, (screen.height() - height) / 2);
    return {x, y, width, height};
}

QString captureExtension(const QString& format)
{
    if (format == QLatin1String("jpg")) {
        return QStringLiteral(".jpg");
    }
    if (format == QLatin1String("bmp")) {
        return QStringLiteral(".bmp");
    }
    return QStringLiteral(".png");
}

#ifdef Q_OS_WIN
QImage imageFromBgra(const VideoFrame& frame)
{
    if (frame.format != PixelFormat::BGRA8 || frame.width < 2 || frame.height < 2 || frame.bytes.empty()) {
        return {};
    }
    QImage image(frame.width, frame.height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }
    const int srcStride = frame.stride > 0 ? frame.stride : frame.width * 4;
    const int rowBytes = frame.width * 4;
    for (int y = 0; y < frame.height; ++y) {
        std::memcpy(image.scanLine(y), frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * srcStride,
            static_cast<std::size_t>(rowBytes));
    }
    return image;
}
#endif

QScreen* secondaryScreen()
{
    QScreen* primary = QGuiApplication::primaryScreen();
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen != primary) {
            return screen;
        }
    }
    return primary;
}

QSize regionPresetSize(const QString& preset)
{
    if (preset == QLatin1String("720p") || preset == QLatin1String("youtube-720")) {
        return {1280, 720};
    }
    if (preset == QLatin1String("1080p") || preset == QLatin1String("youtube-1080")) {
        return {1920, 1080};
    }
    if (preset == QLatin1String("1440p") || preset == QLatin1String("youtube-1440")) {
        return {2560, 1440};
    }
    if (preset == QLatin1String("4k") || preset == QLatin1String("youtube-2160")) {
        return {3840, 2160};
    }
    if (preset == QLatin1String("1366x768")) {
        return {1366, 768};
    }
    if (preset == QLatin1String("1600x900")) {
        return {1600, 900};
    }
    if (preset == QLatin1String("1280x800")) {
        return {1280, 800};
    }
    if (preset == QLatin1String("1024x768")) {
        return {1024, 768};
    }
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    config_.load();
    setupUi();
    applyConfigToUi();

    connect(&session_, &RecordingSession::errorOccurred, this, &MainWindow::onSessionError);
    connect(&session_, &RecordingSession::stateChanged, this, [this](RecordingSession::State) {
        updateChrome();
    });
    connect(&session_, &RecordingSession::recordingFinished, this, &MainWindow::onSessionFinished);

    uiTimer_ = new QTimer(this);
    uiTimer_->setInterval(200);
    connect(uiTimer_, &QTimer::timeout, this, &MainWindow::onTimerTick);
    setupRegionOverlay();
    updateChrome();
}

MainWindow::~MainWindow() = default;

void MainWindow::present()
{
    applyConfigToUi();
    if (config_.data().hideOnStartup && trayActive()) {
        return;
    }
    show();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("ORS"));
    resize(448, 176);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("chromeRoot"));

    auto* header = new QWidget(root);
    header->setObjectName(QStringLiteral("headerBar"));
    setupHeader(header);

    auto* modes = new QWidget(root);
    modes->setObjectName(QStringLiteral("modeRow"));
    setupModeRow(modes);

    auto* actions = new QWidget(root);
    actions->setObjectName(QStringLiteral("actionRow"));
    setupActions(actions);

    auto* status = new QWidget(root);
    status->setObjectName(QStringLiteral("statusRow"));
    setupStatus(status);

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(modes);
    layout->addWidget(actions);
    layout->addWidget(status);

    setCentralWidget(root);
    setMinimumSize(432, 164);

    root->setStyleSheet(QStringLiteral(
        "#chromeRoot { background: #f6f3ee; }"
        "#headerBar { background: #163a38; }"
        "#brandLabel { color: #f4f7f6; font-size: 13px; font-weight: 700; letter-spacing: 1px; }"
        "#headerBar QToolButton#settingsButton {"
        "  background: transparent; border: none; padding: 2px; border-radius: 6px;"
        "}"
        "#headerBar QToolButton#settingsButton:hover { background: rgba(255,255,255,0.12); }"
        "#modeRow { background: #f6f3ee; }"
        "#modeButton {"
        "  background: transparent;"
        "  border: 1px solid #d7d0c6;"
        "  border-radius: 12px;"
        "  padding: 3px 10px;"
        "  color: #3c4a48;"
        "}"
        "#modeButton:hover { background: #efeae3; }"
        "#modeButton:checked {"
        "  background: #163a38;"
        "  border-color: #163a38;"
        "  color: #f4f7f6;"
        "  font-weight: 600;"
        "}"
        "#actionRow { background: #f6f3ee; }"
        "#recordButton {"
        "  background: #e25b4a;"
        "  color: #fff;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 2px 4px 2px;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "}"
        "#recordButton:hover { background: #cc4e3f; }"
        "#recordButton:pressed { background: #b54438; }"
        "#chipButton {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  padding: 6px 2px 4px 2px;"
        "  color: #1f4e4c;"
        "  font-size: 11px;"
        "}"
        "#chipButton:hover { background: #fbf8f4; border-color: #c9c0b4; }"
        "#chipButton::menu-indicator { image: none; width: 0; }"
        "#statusRow { background: #efeae3; }"
        "#statusLabel { color: #1f4e4c; font-weight: 600; }"
        "#timerLabel { color: #1f4e4c; font-family: Consolas, 'Cascadia Mono', monospace; }"
        "#metaLabel { color: #6b7573; }"));
}

void MainWindow::setupHeader(QWidget* parent)
{
    auto* brand = new QLabel(QStringLiteral("ORS"), parent);
    brand->setObjectName(QStringLiteral("brandLabel"));

    settingsButton_ = new QToolButton(parent);
    settingsButton_->setObjectName(QStringLiteral("settingsButton"));
    settingsButton_->setIcon(toolbarIcon(ToolbarGlyph::Settings, 20, kHeaderIcon));
    settingsButton_->setIconSize(QSize(20, 20));
    settingsButton_->setAutoRaise(true);
    settingsButton_->setCursor(Qt::PointingHandCursor);
    settingsButton_->setFocusPolicy(Qt::NoFocus);
    settingsButton_->setToolTip(tr("設定"));
    connect(settingsButton_, &QToolButton::clicked, this, &MainWindow::onSettings);

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(12, 6, 8, 6);
    layout->addWidget(brand);
    layout->addStretch();
    layout->addWidget(settingsButton_);
}

void MainWindow::setupModeRow(QWidget* parent)
{
    screenTab_ = makeModeButton(tr("螢幕"), parent);
    gameTab_ = makeModeButton(tr("遊戲"), parent);
    audioTab_ = makeModeButton(tr("音訊"), parent);
    screenTab_->setToolTip(tr("螢幕錄製"));
    gameTab_->setToolTip(tr("遊戲錄製"));
    audioTab_->setToolTip(tr("音訊錄製"));

    tabGroup_ = new QButtonGroup(this);
    tabGroup_->setExclusive(true);
    tabGroup_->addButton(screenTab_, 0);
    tabGroup_->addButton(gameTab_, 1);
    tabGroup_->addButton(audioTab_, 2);
    connect(tabGroup_, &QButtonGroup::idClicked, this, &MainWindow::onTabChanged);

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(12, 6, 12, 2);
    layout->setSpacing(6);
    layout->addWidget(screenTab_);
    layout->addWidget(gameTab_);
    layout->addWidget(audioTab_);
    layout->addStretch();
}

void MainWindow::setupActions(QWidget* parent)
{
    recordButton_ = makeActionButton(parent, QStringLiteral("recordButton"));
    recordButton_->setIcon(toolbarIcon(ToolbarGlyph::Record, 28, kRecordIcon));
    recordButton_->setText(tr("錄製"));
    recordButton_->setToolTip(tr("錄製"));
    connect(recordButton_, &QToolButton::clicked, this, &MainWindow::onRecord);

    regionMenu_ = new QMenu(tr("範圍"), this);
    regionMenu_->addAction(tr("全螢幕"), this, [this] { onRegionPreset(QStringLiteral("fullscreen")); });
    regionMenu_->addAction(tr("主要顯示器"), this, [this] { onRegionPreset(QStringLiteral("monitor-primary")); });
    regionMenu_->addAction(tr("第二顯示器"), this, [this] { onRegionPreset(QStringLiteral("monitor-secondary")); });
    regionMenu_->addSeparator();
    regionMenu_->addAction(tr("720p (1280×720)"), this, [this] { onRegionPreset(QStringLiteral("720p")); });
    regionMenu_->addAction(tr("1080p (1920×1080)"), this, [this] { onRegionPreset(QStringLiteral("1080p")); });
    regionMenu_->addAction(tr("1440p (2560×1440)"), this, [this] { onRegionPreset(QStringLiteral("1440p")); });
    regionMenu_->addAction(tr("4K (3840×2160)"), this, [this] { onRegionPreset(QStringLiteral("4k")); });
    regionMenu_->addSeparator();
    regionMenu_->addAction(tr("1366×768"), this, [this] { onRegionPreset(QStringLiteral("1366x768")); });
    regionMenu_->addAction(tr("1600×900"), this, [this] { onRegionPreset(QStringLiteral("1600x900")); });
    regionMenu_->addAction(tr("1280×800"), this, [this] { onRegionPreset(QStringLiteral("1280x800")); });
    regionMenu_->addAction(tr("1024×768"), this, [this] { onRegionPreset(QStringLiteral("1024x768")); });
    regionMenu_->addSeparator();
    regionMenu_->addAction(tr("選擇區域…"), this, [this] { onRegionPreset(QStringLiteral("select-region")); });
    regionMenu_->addAction(tr("自訂大小…"), this, [this] { onRegionPreset(QStringLiteral("custom")); });

    codecMenu_ = new QMenu(tr("輸出格式"), this);
    codecMenu_->addAction(tr("H.264 + AAC (.MP4)"), this, [this] { onCodecSelected(QStringLiteral("mp4")); });
    codecMenu_->addAction(tr("WMV"), this, [this] { onCodecSelected(QStringLiteral("wmv")); });
    codecMenu_->addSeparator();
    auto* later = codecMenu_->addAction(tr("其他容器（後續 Phase）"));
    later->setEnabled(false);

    soundMenu_ = new QMenu(tr("麥克風"), this);
    connect(soundMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildSoundMenu);
    rebuildSoundMenu();

    captureButton_ = makeActionButton(parent, QStringLiteral("chipButton"));
    captureButton_->setIcon(toolbarIcon(ToolbarGlyph::Capture, 28, kIconColor));
    captureButton_->setText(tr("截圖"));
    captureButton_->setToolTip(tr("截圖"));
    connect(captureButton_, &QToolButton::clicked, this, &MainWindow::onCapture);

    regionButton_ = makeActionButton(parent, QStringLiteral("chipButton"));
    regionButton_->setIcon(toolbarIcon(ToolbarGlyph::Region, 28, kIconColor));
    regionButton_->setText(tr("範圍"));
    regionButton_->setToolTip(tr("範圍"));
    regionButton_->setMenu(regionMenu_);
    regionButton_->setPopupMode(QToolButton::InstantPopup);

    openButton_ = makeActionButton(parent, QStringLiteral("chipButton"));
    openButton_->setIcon(toolbarIcon(ToolbarGlyph::Open, 28, kIconColor));
    openButton_->setText(tr("資料夾"));
    openButton_->setToolTip(tr("資料夾"));
    connect(openButton_, &QToolButton::clicked, this, &MainWindow::onOpenFolder);

    codecButton_ = makeActionButton(parent, QStringLiteral("chipButton"));
    codecButton_->setIcon(toolbarIcon(ToolbarGlyph::Codec, 28, kIconColor));
    codecButton_->setText(tr("輸出格式"));
    codecButton_->setToolTip(tr("輸出格式"));
    codecButton_->setMenu(codecMenu_);
    codecButton_->setPopupMode(QToolButton::InstantPopup);

    soundButton_ = makeActionButton(parent, QStringLiteral("chipButton"));
    soundButton_->setIcon(toolbarIcon(ToolbarGlyph::Sound, 28, kIconColor));
    soundButton_->setText(tr("麥克風"));
    soundButton_->setToolTip(tr("麥克風"));
    soundButton_->setMenu(soundMenu_);
    soundButton_->setPopupMode(QToolButton::InstantPopup);

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);
    layout->addWidget(recordButton_);
    layout->addWidget(captureButton_);
    layout->addWidget(regionButton_);
    layout->addWidget(openButton_);
    layout->addWidget(codecButton_);
    layout->addWidget(soundButton_);
}

void MainWindow::setupStatus(QWidget* parent)
{
    statusLabel_ = new QLabel(parent);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));

    timerLabel_ = new QLabel(QStringLiteral("00:00:00"), parent);
    timerLabel_->setObjectName(QStringLiteral("timerLabel"));

    metaLabel_ = new QLabel(parent);
    metaLabel_->setObjectName(QStringLiteral("metaLabel"));
    metaLabel_->setWordWrap(false);

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(12, 4, 12, 6);
    layout->setSpacing(10);
    layout->addWidget(statusLabel_);
    layout->addWidget(timerLabel_);
    layout->addWidget(metaLabel_, 1);
}

void MainWindow::applyConfigToUi()
{
    const int index = tabIndex(config_.data().lastTab);
    if (auto* button = tabGroup_->button(index)) {
        button->setChecked(true);
    }
    rebuildSoundMenu();
    updateActionsForTab(index);

    const bool wantTop = config_.data().alwaysOnTop;
    const bool haveTop = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    if (wantTop != haveTop) {
        const bool visible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, wantTop);
        if (visible) {
            show();
        }
    }

    applyTrayFromConfig();
}

void MainWindow::setupTray()
{
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(toolbarIcon(ToolbarGlyph::Record, 22, kIconColor));
    trayIcon_->setToolTip(tr("ORS"));

    auto* menu = new QMenu(this);
    menu->addAction(tr("顯示"), this, &MainWindow::restoreFromTray);
    menu->addAction(tr("結束"), this, &QWidget::close);
    trayIcon_->setContextMenu(menu);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            restoreFromTray();
        }
    });
}

void MainWindow::applyTrayFromConfig()
{
    const bool want = config_.data().useTrayIcon && QSystemTrayIcon::isSystemTrayAvailable();
    const bool wasActive = trayActive();
    if (want) {
        if (!trayIcon_) {
            setupTray();
        }
        trayIcon_->show();
        return;
    }
    if (trayIcon_) {
        trayIcon_->hide();
    }
    if (wasActive && !isVisible()) {
        showNormal();
    }
}

bool MainWindow::trayActive() const
{
    return trayIcon_ && trayIcon_->isVisible();
}

void MainWindow::restoreFromTray()
{
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    raise();
    activateWindow();
}

void MainWindow::persistUiToConfig()
{
    config_.data().lastTab = tabKey(tabGroup_->checkedId());
}

bool MainWindow::saveConfig()
{
    persistUiToConfig();
    if (!config_.save()) {
        QMessageBox::warning(this, tr("ORS"), tr("無法寫入設定檔。"));
        return false;
    }
    return true;
}

void MainWindow::updateChrome()
{
    const bool idle = session_.state() == RecordingSession::State::Idle;
    const bool recording = session_.state() == RecordingSession::State::Recording;
    const QRect region = currentRegionRect();
    setWindowTitle(tr("ORS"));
    statusLabel_->setText(stateText());
    timerLabel_->setText(formatElapsed(currentElapsedMs()));
    metaLabel_->setText(tr("%1 · %2×%3 · %4")
                            .arg(modeHint())
                            .arg(region.width())
                            .arg(region.height())
                            .arg(config_.data().container.toUpper()));

    recordButton_->setText(idle ? tr("錄製") : tr("停止"));
    recordButton_->setIcon(toolbarIcon(idle ? ToolbarGlyph::Record : ToolbarGlyph::Stop, 28, kRecordIcon));
    recordButton_->setToolTip(idle ? tr("錄製") : tr("停止"));
    screenTab_->setEnabled(idle);
    gameTab_->setEnabled(idle);
    audioTab_->setEnabled(idle);
    captureButton_->setEnabled(idle);
    regionButton_->setEnabled(idle);
    codecButton_->setEnabled(idle);
    soundButton_->setEnabled(idle);
    settingsButton_->setEnabled(idle);
    updateOverlayVisibility();

    if (recording) {
        if (!uiTimer_->isActive()) {
            recClock_.restart();
            uiTimer_->start();
        }
    } else {
        uiTimer_->stop();
        if (session_.state() == RecordingSession::State::Paused && recClock_.isValid()) {
            recordedMs_ += recClock_.elapsed();
            recClock_.invalidate();
        }
        if (idle) {
            recordedMs_ = 0;
            recClock_.invalidate();
            timerLabel_->setText(QStringLiteral("00:00:00"));
        } else {
            timerLabel_->setText(formatElapsed(currentElapsedMs()));
        }
    }
}

void MainWindow::updateActionsForTab(int index)
{
    const bool screen = index <= 0;
    const bool audio = index == 2;
    captureButton_->setVisible(!audio);
    regionButton_->setVisible(screen);
    updateOverlayVisibility();
}

void MainWindow::setupRegionOverlay()
{
    overlay_ = new RegionOverlay(this);
    connect(overlay_, &RegionOverlay::regionChanged, this, [this](const QRect& region) {
        metaLabel_->setText(tr("%1 · %2×%3 · %4")
                                .arg(modeHint())
                                .arg(region.width())
                                .arg(region.height())
                                .arg(config_.data().container.toUpper()));
    });
    connect(overlay_, &RegionOverlay::regionCommitted, this, [this](const QRect&) {
        persistOverlayRect(true);
        saveConfig();
        updateChrome();
    });
    syncOverlayFromConfig();
    updateOverlayVisibility();
}

void MainWindow::updateOverlayVisibility()
{
    if (!overlay_) {
        return;
    }
    const bool screenTab = tabGroup_ && tabGroup_->checkedId() <= 0;
    const bool selecting = selector_ && selector_->isVisible();
    overlay_->setVisible(screenTab && !selecting && !settingsOpen_);
    overlay_->setInteractive(session_.state() == RecordingSession::State::Idle);
    overlay_->setRecording(session_.state() == RecordingSession::State::Recording);
    if (overlay_->isVisible()) {
        overlay_->raise();
    }
}

void MainWindow::syncOverlayFromConfig()
{
    if (!overlay_) {
        return;
    }
    overlay_->setCaptureRect(presetRegionRect(config_.data().regionPreset));
}

void MainWindow::persistOverlayRect(bool asCustom)
{
    if (!overlay_) {
        return;
    }
    const QRect rect = overlay_->captureRectNative();
    config_.data().hasCustomPosition = true;
    config_.data().customX = rect.x();
    config_.data().customY = rect.y();
    config_.data().customWidth = rect.width();
    config_.data().customHeight = rect.height();
    if (asCustom) {
        config_.data().regionPreset = QStringLiteral("custom");
    }
}

void MainWindow::promptCustomSize()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("自訂大小"));
    auto* widthSpin = new QSpinBox(&dialog);
    auto* heightSpin = new QSpinBox(&dialog);
    widthSpin->setRange(RegionOverlay::kMinWidth, 7680);
    heightSpin->setRange(RegionOverlay::kMinHeight, 4320);
    widthSpin->setSingleStep(2);
    heightSpin->setSingleStep(2);
    const QRect current = currentRegionRect();
    widthSpin->setValue(current.width());
    heightSpin->setValue(current.height());

    auto* form = new QFormLayout;
    form->addRow(tr("寬度"), widthSpin);
    form->addRow(tr("高度"), heightSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QRect rect = current;
    rect.setSize(QSize(widthSpin->value() & ~1, heightSpin->value() & ~1));
    rect.moveCenter(current.center());
    overlay_->setCaptureRect(rect);
    persistOverlayRect(true);
    saveConfig();
    updateOverlayVisibility();
    updateChrome();
}

void MainWindow::startRegionSelection()
{
    if (selector_) {
        selector_->raise();
        selector_->activateWindow();
        return;
    }

    if (overlay_) {
        overlay_->hide();
    }
    selector_ = new RegionSelector;
    connect(selector_, &RegionSelector::selected, this, [this](const QRect& rect) {
        selector_.clear();
        overlay_->setCaptureRect(rect);
        persistOverlayRect(true);
        saveConfig();
        updateOverlayVisibility();
        updateChrome();
    });
    connect(selector_, &RegionSelector::cancelled, this, [this] {
        selector_.clear();
        updateOverlayVisibility();
    });
    connect(selector_, &QObject::destroyed, this, [this] {
        selector_.clear();
        updateOverlayVisibility();
    });
    selector_->show();
    selector_->raise();
    selector_->activateWindow();
}

QRect MainWindow::currentRegionRect() const
{
    if (overlay_ && overlay_->isVisible()) {
        return overlay_->captureRectNative();
    }
    return presetRegionRect(config_.data().regionPreset);
}

QRect MainWindow::presetRegionRect(const QString& preset) const
{
    QScreen* primary = QGuiApplication::primaryScreen();
    const QRect desktop = nativeVirtualDesktop();
    const QRect main = nativeScreenGeometry(primary);

    if (preset == QLatin1String("fullscreen")) {
        return desktop;
    }
    if (preset == QLatin1String("monitor-secondary")) {
        if (QScreen* second = secondaryScreen()) {
            return nativeScreenGeometry(second);
        }
    }
    if (const QSize size = regionPresetSize(preset); size.isValid()) {
        return centeredOn(main, size.width(), size.height());
    }
    if (preset == QLatin1String("select-region") || preset == QLatin1String("custom")) {
        QRect custom(0, 0, config_.data().customWidth, config_.data().customHeight);
        if (config_.data().hasCustomPosition) {
            custom.moveTo(config_.data().customX, config_.data().customY);
        } else {
            custom = centeredOn(main, custom.width(), custom.height());
        }
        return custom;
    }
    return main;
}

void MainWindow::onRecord()
{
    if (session_.state() != RecordingSession::State::Idle) {
        session_.stop();
        return;
    }

    const int tab = tabGroup_ ? tabGroup_->checkedId() : 0;
    if (tab == 1) {
        QMessageBox::information(this, tr("ORS"), tr("遊戲擷取將在後續 Phase 實作。"));
        return;
    }
    if (tab == 2) {
        QMessageBox::information(this, tr("ORS"), tr("僅音訊錄製將在後續 Phase 實作。"));
        return;
    }
    if (config_.data().container.compare(QLatin1String("mp4"), Qt::CaseInsensitive) != 0) {
        QMessageBox::information(this, tr("ORS"), tr("目前僅支援 MP4 輸出。"));
        return;
    }

    recordedMs_ = 0;
    recClock_.invalidate();
    persistOverlayRect(config_.data().regionPreset == QLatin1String("custom"));
    session_.start(makeRecordingRequest());
}

void MainWindow::onCapture()
{
#ifndef Q_OS_WIN
    QMessageBox::information(this, tr("ORS"), tr("目前僅支援 Windows 截圖。"));
#else
    if (session_.state() != RecordingSession::State::Idle) {
        return;
    }

    persistOverlayRect(config_.data().regionPreset == QLatin1String("custom"));
    const QRect region = overlay_ ? overlay_->captureRectNative() : currentRegionRect();
    if (region.width() < 2 || region.height() < 2) {
        QMessageBox::warning(this, tr("ORS"), tr("擷取區域太小"));
        return;
    }

    const bool overlayWasVisible = overlay_ && overlay_->isVisible();
    if (overlay_) {
        overlay_->hide();
    }
    QApplication::processEvents();
    QThread::msleep(50);

    CaptureSettings settings;
    settings.source = CaptureSource::Screen;
    settings.x = region.x();
    settings.y = region.y();
    settings.width = region.width();
    settings.height = region.height();
    settings.includeCursor = config_.data().captureIncludeCursor;
    settings.variableFrameRate = false;
    settings.frameRate = 30;

    CaptureWin capture;
    VideoFrame frame;
    QString error;
    bool ok = false;
    if (!capture.start(settings)) {
        error = capture.lastError().isEmpty() ? tr("畫面擷取啟動失敗") : capture.lastError();
    } else {
        const GrabResult result = capture.grabStill(frame, 2000);
        if (result != GrabResult::Ok) {
            error = capture.lastError().isEmpty() ? tr("截圖失敗") : capture.lastError();
        } else {
            ok = true;
        }
        capture.stop();
    }

    if (overlayWasVisible) {
        updateOverlayVisibility();
    }

    if (!ok) {
        QMessageBox::warning(this, tr("ORS"), error);
        return;
    }

    const QImage image = imageFromBgra(frame);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("ORS"), tr("截圖失敗"));
        return;
    }

    const QString format = Config::normalizedCaptureImageFormat(config_.data().captureImageFormat);
    const char* qtFormat = "PNG";
    int quality = -1;
    if (format == QLatin1String("jpg")) {
        qtFormat = "JPEG";
        quality = 92;
    } else if (format == QLatin1String("bmp")) {
        qtFormat = "BMP";
    }

    const QString dirPath = Config::resolvedOutputDirectory(config_.data());
    const QString path = makeUniqueOutputPath(
        dirPath,
        config_.data().filenameTemplate,
        config_.data().filenamePrefix,
        config_.data().filenameStartNumber,
        captureExtension(format),
        QDateTime::currentDateTime());
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("ORS"), tr("無法建立輸出資料夾。"));
        return;
    }
    if (!image.save(path, qtFormat, quality)) {
        QMessageBox::warning(this, tr("ORS"), tr("無法寫入截圖檔案。"));
        return;
    }
    metaLabel_->setText(tr("已儲存 %1").arg(path));
#endif
}

void MainWindow::onOpenFolder()
{
    const QString dirPath = Config::resolvedOutputDirectory(config_.data());
    if (!QDir().mkpath(dirPath)) {
        QMessageBox::warning(this, tr("ORS"), tr("無法建立輸出資料夾。"));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath))) {
        QMessageBox::warning(this, tr("ORS"), tr("無法開啟輸出資料夾。"));
    }
}

void MainWindow::onSettings()
{
    settingsOpen_ = true;
    updateOverlayVisibility();
    SettingsDialog dialog(config_, this);
    const auto result = dialog.exec();
    settingsOpen_ = false;
    if (result == QDialog::Accepted) {
        saveConfig();
        applyConfigToUi();
        updateChrome();
    }
    updateOverlayVisibility();
}

void MainWindow::onRegionPreset(const QString& preset)
{
    if (preset == QLatin1String("select-region")) {
        startRegionSelection();
        return;
    }
    if (preset == QLatin1String("custom")) {
        promptCustomSize();
        return;
    }

    config_.data().regionPreset = preset;
    syncOverlayFromConfig();
    persistOverlayRect(false);
    saveConfig();
    updateOverlayVisibility();
    updateChrome();
}

void MainWindow::onCodecSelected(const QString& container)
{
    config_.data().container = container;
    if (container == QLatin1String("wmv")) {
        config_.data().video = QStringLiteral("wmv");
        config_.data().audio = QStringLiteral("wma");
    } else {
        config_.data().video = QStringLiteral("h264");
        config_.data().audio = QStringLiteral("aac");
    }
    saveConfig();
    updateChrome();
}

void MainWindow::onSystemAudioToggled(bool enabled)
{
    config_.data().systemAudio = enabled;
    saveConfig();
}

void MainWindow::onMicrophoneSelected(const QString& id)
{
    config_.data().microphoneId = id;
    saveConfig();
}

void MainWindow::rebuildSoundMenu()
{
    soundMenu_->clear();
    systemAudioAction_ = soundMenu_->addAction(tr("錄製系統聲音"));
    systemAudioAction_->setCheckable(true);
    systemAudioAction_->setChecked(config_.data().systemAudio);
    connect(systemAudioAction_, &QAction::toggled, this, &MainWindow::onSystemAudioToggled);

    soundMenu_->addSeparator();
    auto* micGroup = new QActionGroup(soundMenu_);
    micGroup->setExclusive(true);

    auto addMic = [this, micGroup](const QString& title, const QString& id) {
        auto* action = soundMenu_->addAction(title);
        action->setCheckable(true);
        action->setData(id);
        micGroup->addAction(action);
        if (config_.data().microphoneId == id) {
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, id](bool checked) {
            if (checked) {
                onMicrophoneSelected(id);
            }
        });
        return action;
    };

    auto* noneAction = addMic(tr("不錄製麥克風"), {});
    addMic(tr("預設麥克風"), QStringLiteral("default"));
    bool matched = config_.data().microphoneId.isEmpty()
        || config_.data().microphoneId == QLatin1String("default");
    for (const AudioDeviceInfo& device : listCaptureDevices()) {
        if (device.id.isEmpty() || device.id == QLatin1String("default")) {
            continue;
        }
        addMic(device.name, device.id);
        if (device.id == config_.data().microphoneId) {
            matched = true;
        }
    }
    if (!matched && !config_.data().microphoneId.isEmpty()) {
        addMic(config_.data().microphoneId, config_.data().microphoneId);
        matched = true;
    }
    if (!matched) {
        noneAction->blockSignals(true);
        noneAction->setChecked(true);
        noneAction->blockSignals(false);
    }
}

void MainWindow::onTabChanged(int index)
{
    config_.data().lastTab = tabKey(index);
    updateActionsForTab(index);
    updateChrome();
}

void MainWindow::onSessionError(const QString& message)
{
    QMessageBox::warning(this, tr("ORS"), message);
}

void MainWindow::onSessionFinished(const QString& path)
{
    recordedMs_ = 0;
    recClock_.invalidate();
    uiTimer_->stop();
    updateChrome();
    if (!path.isEmpty()) {
        metaLabel_->setText(tr("已儲存 %1").arg(path));
    }
}

void MainWindow::onTimerTick()
{
    timerLabel_->setText(formatElapsed(currentElapsedMs()));
}

RecordingRequest MainWindow::makeRecordingRequest() const
{
    const QRect region = overlay_ ? overlay_->captureRectNative() : currentRegionRect();
    int width = region.width();
    int height = region.height();
    Config::alignCaptureSize(config_.data(), width, height);

    RecordingRequest request;
    request.capture.source = CaptureSource::Screen;
    request.capture.x = region.x();
    request.capture.y = region.y();
    request.capture.width = width;
    request.capture.height = height;
    request.capture.frameRate = std::clamp(config_.data().frameRate, 1, 120);
    request.capture.includeCursor = config_.data().includeCursor;
    request.capture.variableFrameRate =
        config_.data().frameRateMode.compare(QLatin1String("vfr"), Qt::CaseInsensitive) == 0;
    request.audio.systemAudio = config_.data().systemAudio;
    request.audio.microphoneId = config_.data().microphoneId;
    request.audio.inputSource = config_.data().microphoneInputSource;
    request.encoder.width = request.capture.width;
    request.encoder.height = request.capture.height;
    request.encoder.frameRate = request.capture.frameRate;
    request.encoder.bitrateKbps = Config::videoBitrateKbps(
        config_.data(), request.capture.width, request.capture.height);
    request.encoder.keyframeGopFrames = Config::gopFrameCount(config_.data());
    request.outputDirectory = Config::resolvedOutputDirectory(config_.data());
    request.filenameTemplate = config_.data().filenameTemplate;
    request.filenamePrefix = config_.data().filenamePrefix;
    request.filenameStartNumber = config_.data().filenameStartNumber;
    request.container = QStringLiteral("mp4");
    return request;
}

qint64 MainWindow::currentElapsedMs() const
{
    if (session_.state() == RecordingSession::State::Recording && recClock_.isValid()) {
        return recordedMs_ + recClock_.elapsed();
    }
    return recordedMs_;
}

QString MainWindow::formatElapsed(qint64 milliseconds)
{
    const qint64 total = std::max<qint64>(0, milliseconds / 1000);
    const int hours = static_cast<int>(total / 3600);
    const int minutes = static_cast<int>((total % 3600) / 60);
    const int seconds = static_cast<int>(total % 60);
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString MainWindow::tabKey(int index) const
{
    switch (index) {
    case 1:
        return QStringLiteral("game");
    case 2:
        return QStringLiteral("audio");
    default:
        return QStringLiteral("screen");
    }
}

int MainWindow::tabIndex(const QString& key) const
{
    if (key == QLatin1String("game")) {
        return 1;
    }
    if (key == QLatin1String("audio")) {
        return 2;
    }
    return 0;
}

QString MainWindow::stateText() const
{
    switch (session_.state()) {
    case RecordingSession::State::Recording:
        return tr("錄製中");
    case RecordingSession::State::Paused:
        return tr("已暫停");
    case RecordingSession::State::Idle:
    default:
        return tr("就緒");
    }
}

QString MainWindow::modeHint() const
{
    switch (tabGroup_ ? tabGroup_->checkedId() : 0) {
    case 1:
        return tr("遊戲視窗");
    case 2:
        return tr("僅音訊");
    default:
        return tr("螢幕畫面");
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    updateOverlayVisibility();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange) {
        return;
    }
    if (isMinimized() && config_.data().hideWhenMinimized && trayActive()) {
        hide();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (selector_) {
        selector_->close();
    }
    if (session_.state() != RecordingSession::State::Idle) {
        session_.stop();
    }
    persistOverlayRect(config_.data().regionPreset == QLatin1String("custom"));
    saveConfig();
    if (trayIcon_) {
        trayIcon_->hide();
    }
    QMainWindow::closeEvent(event);
}

} // namespace ors
