#pragma once

#include "core/Config.h"
#include "core/RecordingSession.h"

#include <QMainWindow>
#include <QRect>

class QAction;
class QButtonGroup;
class QLabel;
class QMenu;
class QPushButton;
class QToolButton;

namespace ors {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void setupHeader(QWidget* parent);
    void setupModeRow(QWidget* parent);
    void setupActions(QWidget* parent);
    void setupStatus(QWidget* parent);
    void applyConfigToUi();
    void persistUiToConfig();
    bool saveConfig();
    void updateChrome();
    void updateActionsForTab(int index);
    QRect currentRegionRect() const;

    void onRecord();
    void onCapture();
    void onOpenFolder();
    void onSettings();
    void onRegionPreset(const QString& preset);
    void onCodecSelected(const QString& container);
    void onSystemAudioToggled(bool enabled);
    void onNoMicrophone();
    void onTabChanged(int index);
    void onSessionError(const QString& message);

    QString tabKey(int index) const;
    int tabIndex(const QString& key) const;
    QString stateText() const;
    QString modeHint() const;

    Config config_;
    RecordingSession session_;

    QButtonGroup* tabGroup_{};
    QPushButton* screenTab_{};
    QPushButton* gameTab_{};
    QPushButton* audioTab_{};

    QPushButton* recordButton_{};
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
};

} // namespace ors
