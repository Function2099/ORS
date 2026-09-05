#pragma once

#include "core/Config.h"
#include "core/RecordingSession.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QRect>

class QAction;
class QButtonGroup;
class QEvent;
class QLabel;
class QMenu;
class QPushButton;
class QShowEvent;
class QSystemTrayIcon;
class QTimer;
class QToolButton;

namespace ors {

class RegionOverlay;
class RegionSelector;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void present();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupHeader(QWidget* parent);
    void setupModeRow(QWidget* parent);
    void setupActions(QWidget* parent);
    void setupStatus(QWidget* parent);
    void applyConfigToUi();
    void applyTrayFromConfig();
    void setupTray();
    void restoreFromTray();
    bool trayActive() const;
    void persistUiToConfig();
    bool saveConfig();
    void updateChrome();
    void updateActionsForTab(int index);
    void setupRegionOverlay();
    void updateOverlayVisibility();
    void syncOverlayFromConfig();
    void persistOverlayRect(bool asCustom);
    void promptCustomSize();
    void startRegionSelection();
    QRect currentRegionRect() const;
    QRect presetRegionRect(const QString& preset) const;

    void onRecord();
    void onCapture();
    void onOpenFolder();
    void onSettings();
    void onRegionPreset(const QString& preset);
    void onCodecSelected(const QString& container);
    void onSystemAudioToggled(bool enabled);
    void onMicrophoneSelected(const QString& id);
    void rebuildSoundMenu();
    void onTabChanged(int index);
    void onSessionError(const QString& message);

    QString tabKey(int index) const;
    int tabIndex(const QString& key) const;
    QString stateText() const;
    QString modeHint() const;
    RecordingRequest makeRecordingRequest() const;
    void onSessionFinished(const QString& path);
    void onTimerTick();
    qint64 currentElapsedMs() const;
    static QString formatElapsed(qint64 milliseconds);

    Config config_;
    RecordingSession session_;
    QElapsedTimer recClock_;
    qint64 recordedMs_{0};
    QTimer* uiTimer_{};

    QButtonGroup* tabGroup_{};
    QPushButton* screenTab_{};
    QPushButton* gameTab_{};
    QPushButton* audioTab_{};

    QToolButton* recordButton_{};
    QToolButton* captureButton_{};
    QToolButton* regionButton_{};
    QToolButton* openButton_{};
    QToolButton* codecButton_{};
    QToolButton* soundButton_{};
    QToolButton* settingsButton_{};

    QLabel* statusLabel_{};
    QLabel* metaLabel_{};
    QLabel* timerLabel_{};

    QAction* systemAudioAction_{};
    QMenu* regionMenu_{};
    QMenu* codecMenu_{};
    QMenu* soundMenu_{};

    RegionOverlay* overlay_{};
    QPointer<RegionSelector> selector_;
    QSystemTrayIcon* trayIcon_{};
    bool settingsOpen_{false};
};

} // namespace ors
