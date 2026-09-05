#pragma once

#include "core/Config.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace ors {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(Config& config, QWidget* parent = nullptr);

private:
    enum class Page {
        Record = 0,
        Audio,
        Capture,
        Gif,
        Hotkeys,
        Save,
        TimeLimit,
        Watermark,
        Performance,
        Language,
    };

    QWidget* createRecordPage();
    QWidget* createAudioPage();
    QWidget* createCapturePage();
    QWidget* createSavePage();
    QWidget* createLanguagePage();
    QWidget* createComingSoonPage();
    void browseDirectory();
    void showFilenameHelp();
    void updatePreview();
    void resetToDefaults();
    void applyToConfig();
    void loadRecordingFrom(const ConfigData& cfg);
    void loadAudioFrom(const ConfigData& cfg);
    void loadCaptureFrom(const ConfigData& cfg);
    void populateMicrophoneCombo();
    void syncMicSourceEnabled();
    void syncTrayChildren();
    void syncCustomQualityButton();
    void updateKeyframeLabel();
    void editCustomBitrate();
    QString currentExtension() const;

    Config& config_;
    QButtonGroup* pageGroup_{};
    QStackedWidget* pages_{};

    QLineEdit* prefixEdit_{};
    QLineEdit* directoryEdit_{};
    QLineEdit* templateEdit_{};
    QSpinBox* startNumberSpin_{};
    QLabel* previewLabel_{};
    QComboBox* languageCombo_{};

    QCheckBox* includeCursorCheck_{};
    QCheckBox* alwaysOnTopCheck_{};
    QCheckBox* trayIconCheck_{};
    QCheckBox* hideMinimizedCheck_{};
    QCheckBox* hideStartupCheck_{};
    QSpinBox* frameRateSpin_{};
    QComboBox* qualityCombo_{};
    QPushButton* customQualityButton_{};
    QLabel* keyframeLabel_{};
    QSlider* keyframeSlider_{};
    QComboBox* resolutionCombo_{};
    QComboBox* frameRateModeCombo_{};
    int customBitrateKbps_{12000};

    QCheckBox* systemAudioCheck_{};
    QComboBox* microphoneCombo_{};
    QComboBox* micSourceCombo_{};
    QWidget* micSourceRow_{};

    QCheckBox* captureCursorCheck_{};
    QComboBox* captureFormatCombo_{};
};

} // namespace ors
