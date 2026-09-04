#include "ui/MainWindow.h"

#include "ui/SettingsDialog.h"
#include "ui/ToolbarIcons.h"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace ors {
namespace {

const QColor kIconColor(0x1f, 0x4e, 0x4c);
const QColor kHeaderIcon(0xec, 0xf4, 0xf3);

QPushButton* makeModeButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(QStringLiteral("modeButton"));
    return button;
}

QToolButton* makeChipButton(QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(18, 18));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(QStringLiteral("chipButton"));
    return button;
}

QRect centeredOn(const QRect& screen, int width, int height)
{
    const int x = screen.x() + qMax(0, (screen.width() - width) / 2);
    const int y = screen.y() + qMax(0, (screen.height() - height) / 2);
    return {x, y, width, height};
}

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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    config_.load();
    setupUi();
    applyConfigToUi();
    updateChrome();

    connect(&session_, &RecordingSession::errorOccurred, this, &MainWindow::onSessionError);
    connect(&session_, &RecordingSession::stateChanged, this, [this](RecordingSession::State) {
        updateChrome();
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("ORS"));
    resize(560, 240);

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
    layout->addWidget(actions, 1);
    layout->addWidget(status);

    setCentralWidget(root);
    setMinimumSize(520, 220);

    root->setStyleSheet(QStringLiteral(
        "#chromeRoot { background: #f6f3ee; }"
        "#headerBar { background: #163a38; }"
        "#brandLabel { color: #f4f7f6; font-size: 15px; font-weight: 700; letter-spacing: 1px; }"
        "#headerBar QToolButton#settingsButton {"
        "  background: transparent; border: none; padding: 4px; border-radius: 6px;"
        "}"
        "#headerBar QToolButton#settingsButton:hover { background: rgba(255,255,255,0.12); }"
        "#modeRow { background: #f6f3ee; }"
        "#modeButton {"
        "  background: transparent;"
        "  border: 1px solid #d7d0c6;"
        "  border-radius: 14px;"
        "  padding: 5px 14px;"
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
        "  border-radius: 12px;"
        "  padding: 12px 22px;"
        "  font-size: 15px;"
        "  font-weight: 700;"
        "  min-width: 108px;"
        "  min-height: 52px;"
        "}"
        "#recordButton:hover { background: #cc4e3f; }"
        "#recordButton:pressed { background: #b54438; }"
        "#chipButton {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 9px;"
        "  padding: 7px 10px;"
        "  color: #1f4e4c;"
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
    layout->setContentsMargins(16, 10, 10, 10);
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
    layout->setContentsMargins(16, 12, 16, 4);
    layout->setSpacing(8);
    layout->addWidget(screenTab_);
    layout->addWidget(gameTab_);
    layout->addWidget(audioTab_);
    layout->addStretch();
}

void MainWindow::setupActions(QWidget* parent)
{
    recordButton_ = new QPushButton(tr("錄製"), parent);
    recordButton_->setObjectName(QStringLiteral("recordButton"));
    recordButton_->setCursor(Qt::PointingHandCursor);
    recordButton_->setFocusPolicy(Qt::NoFocus);
    connect(recordButton_, &QPushButton::clicked, this, &MainWindow::onRecord);

    regionMenu_ = new QMenu(tr("範圍"), this);
    regionMenu_->addAction(tr("全螢幕"), this, [this] { onRegionPreset(QStringLiteral("fullscreen")); });
    regionMenu_->addAction(tr("主要顯示器"), this, [this] { onRegionPreset(QStringLiteral("monitor-primary")); });
    regionMenu_->addAction(tr("第二顯示器"), this, [this] { onRegionPreset(QStringLiteral("monitor-secondary")); });
    regionMenu_->addSeparator();
    regionMenu_->addAction(tr("YouTube 720p"), this, [this] { onRegionPreset(QStringLiteral("youtube-720")); });
    regionMenu_->addAction(tr("YouTube 1080p"), this, [this] { onRegionPreset(QStringLiteral("youtube-1080")); });
    regionMenu_->addAction(tr("YouTube 1440p"), this, [this] { onRegionPreset(QStringLiteral("youtube-1440")); });
    regionMenu_->addAction(tr("YouTube 4K"), this, [this] { onRegionPreset(QStringLiteral("youtube-2160")); });
    regionMenu_->addSeparator();
    regionMenu_->addAction(tr("選擇區域…"), this, [this] { onRegionPreset(QStringLiteral("select-region")); });
    regionMenu_->addAction(tr("自訂大小…"), this, [this] { onRegionPreset(QStringLiteral("custom")); });

    codecMenu_ = new QMenu(tr("格式"), this);
    codecMenu_->addAction(tr("H.264 + AAC (.MP4)"), this, [this] { onCodecSelected(QStringLiteral("mp4")); });
    codecMenu_->addAction(tr("WMV"), this, [this] { onCodecSelected(QStringLiteral("wmv")); });
    codecMenu_->addSeparator();
    auto* later = codecMenu_->addAction(tr("其他容器（後續 Phase）"));
    later->setEnabled(false);

    soundMenu_ = new QMenu(tr("麥克風"), this);
    systemAudioAction_ = soundMenu_->addAction(tr("錄製系統聲音"));
    systemAudioAction_->setCheckable(true);
    connect(systemAudioAction_, &QAction::toggled, this, &MainWindow::onSystemAudioToggled);
    soundMenu_->addAction(tr("不錄製麥克風"), this, &MainWindow::onNoMicrophone);

    captureButton_ = makeChipButton(parent);
    captureButton_->setIcon(toolbarIcon(ToolbarGlyph::Capture, 18, kIconColor));
    captureButton_->setText(tr("截圖"));
    connect(captureButton_, &QToolButton::clicked, this, &MainWindow::onCapture);

    regionButton_ = makeChipButton(parent);
    regionButton_->setIcon(toolbarIcon(ToolbarGlyph::Region, 18, kIconColor));
    regionButton_->setText(tr("範圍"));
    regionButton_->setMenu(regionMenu_);
    regionButton_->setPopupMode(QToolButton::InstantPopup);

    openButton_ = makeChipButton(parent);
    openButton_->setIcon(toolbarIcon(ToolbarGlyph::Open, 18, kIconColor));
    openButton_->setText(tr("資料夾"));
    connect(openButton_, &QToolButton::clicked, this, &MainWindow::onOpenFolder);

    codecButton_ = makeChipButton(parent);
    codecButton_->setIcon(toolbarIcon(ToolbarGlyph::Codec, 18, kIconColor));
    codecButton_->setText(tr("格式"));
    codecButton_->setMenu(codecMenu_);
    codecButton_->setPopupMode(QToolButton::InstantPopup);

    soundButton_ = makeChipButton(parent);
    soundButton_->setIcon(toolbarIcon(ToolbarGlyph::Sound, 18, kIconColor));
    soundButton_->setText(tr("麥克風"));
    soundButton_->setMenu(soundMenu_);
    soundButton_->setPopupMode(QToolButton::InstantPopup);

    auto* chips = new QWidget(parent);
    auto* chipLayout = new QHBoxLayout(chips);
    chipLayout->setContentsMargins(0, 0, 0, 0);
    chipLayout->setSpacing(8);
    chipLayout->addWidget(captureButton_);
    chipLayout->addWidget(regionButton_);
    chipLayout->addWidget(openButton_);
    chipLayout->addWidget(codecButton_);
    chipLayout->addWidget(soundButton_);
    chipLayout->addStretch();

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->setSpacing(14);
    layout->addWidget(recordButton_, 0, Qt::AlignVCenter);
    layout->addWidget(chips, 1);
}

void MainWindow::setupStatus(QWidget* parent)
{
    statusLabel_ = new QLabel(parent);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));

    timerLabel_ = new QLabel(QStringLiteral("00:00:00"), parent);
    timerLabel_->setObjectName(QStringLiteral("timerLabel"));

    metaLabel_ = new QLabel(parent);
    metaLabel_->setObjectName(QStringLiteral("metaLabel"));
    metaLabel_->setWordWrap(true);

    auto* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(16, 8, 16, 10);
    layout->setSpacing(12);
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
    systemAudioAction_->setChecked(config_.data().systemAudio);
    updateActionsForTab(index);
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
    const QRect region = currentRegionRect();
    setWindowTitle(tr("ORS"));
    statusLabel_->setText(stateText());
    timerLabel_->setText(QStringLiteral("00:00:00"));
    metaLabel_->setText(tr("%1 · %2×%3 · %4")
                            .arg(modeHint())
                            .arg(region.width())
                            .arg(region.height())
                            .arg(config_.data().container.toUpper()));
}

void MainWindow::updateActionsForTab(int index)
{
    const bool screen = index <= 0;
    const bool audio = index == 2;
    captureButton_->setVisible(!audio);
    regionButton_->setVisible(screen);
}

QRect MainWindow::currentRegionRect() const
{
    QScreen* primary = QGuiApplication::primaryScreen();
    const QRect desktop = primary ? primary->virtualGeometry() : QRect(0, 0, 1920, 1080);
    const QRect main = primary ? primary->geometry() : desktop;
    const QString preset = config_.data().regionPreset;

    if (preset == QLatin1String("fullscreen")) {
        return desktop;
    }
    if (preset == QLatin1String("monitor-secondary")) {
        if (QScreen* second = secondaryScreen()) {
            return second->geometry();
        }
    }
    if (preset == QLatin1String("youtube-720")) {
        return centeredOn(main, 1280, 720);
    }
    if (preset == QLatin1String("youtube-1080")) {
        return centeredOn(main, 1920, 1080);
    }
    if (preset == QLatin1String("youtube-1440")) {
        return centeredOn(main, 2560, 1440);
    }
    if (preset == QLatin1String("youtube-2160")) {
        return centeredOn(main, 3840, 2160);
    }
    if (preset == QLatin1String("select-region") || preset == QLatin1String("custom")) {
        return centeredOn(main, config_.data().customWidth, config_.data().customHeight);
    }
    return main;
}

void MainWindow::onRecord()
{
    session_.start();
}

void MainWindow::onCapture()
{
    QMessageBox::information(this, tr("ORS"), tr("截圖將在 Phase 2 實作。"));
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
    SettingsDialog dialog(config_, this);
    if (dialog.exec() == QDialog::Accepted) {
        saveConfig();
        updateChrome();
    }
}

void MainWindow::onRegionPreset(const QString& preset)
{
    config_.data().regionPreset = preset;
    saveConfig();
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

void MainWindow::onNoMicrophone()
{
    config_.data().microphoneId.clear();
    saveConfig();
}

void MainWindow::onTabChanged(int index)
{
    config_.data().lastTab = tabKey(index);
    updateActionsForTab(index);
    updateChrome();
}

void MainWindow::onSessionError(const QString& message)
{
    QMessageBox::information(this, tr("ORS"), message);
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveConfig();
    QMainWindow::closeEvent(event);
}

} // namespace ors
