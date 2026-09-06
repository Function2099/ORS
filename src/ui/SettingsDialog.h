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
class QWidget;

namespace ors {

class HotkeyEdit;

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
    QWidget* createGifPage();
    QWidget* createHotkeysPage();
    QWidget* createSavePage();
    QWidget* createTimeLimitPage();
    QWidget* createWatermarkPage();
    QWidget* createPerformancePage();
    QWidget* createLanguagePage();
    void browseDirectory();
    void browseWatermarkImage();
    void showFilenameHelp();
    void updatePreview();
    void updateWatermarkPreview();
    void resetToDefaults();
    bool applyToConfig();
    bool validateHotkeys();
    bool validateWatermark();
    void loadRecordingFrom(const ConfigData& cfg);
    void loadAudioFrom(const ConfigData& cfg);
    void loadCaptureFrom(const ConfigData& cfg);
    void loadGifFrom(const ConfigData& cfg);
    void loadTimeLimitFrom(const ConfigData& cfg);
    void loadWatermarkFrom(const ConfigData& cfg);
    void loadPerformanceFrom(const ConfigData& cfg);
    void loadHotkeysFrom(const ConfigData& cfg);
    void syncTimeLimitEnabled();
    void syncWatermarkEnabled();
    void syncEncoderThreadsEnabled();
    void syncHotkeyEdits();
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

    QCheckBox* gifCursorCheck_{};
    QSpinBox* gifFrameRateSpin_{};

    QCheckBox* timeLimitEnabledCheck_{};
    QSpinBox* timeLimitMinutesSpin_{};
    QSpinBox* timeLimitSecondsSpin_{};
    QButtonGroup* timeLimitActionGroup_{};
    QWidget* timeLimitDurationCard_{};
    QWidget* timeLimitOptionsCard_{};

    QCheckBox* watermarkEnabledCheck_{};
    QWidget* watermarkCard_{};
    QLineEdit* watermarkPathEdit_{};
    QSpinBox* watermarkOpacitySpin_{};
    QSpinBox* watermarkXSpin_{};
    QSpinBox* watermarkYSpin_{};
    QCheckBox* watermarkCaptureCheck_{};
    QLabel* watermarkPreview_{};
    QLabel* watermarkPreviewNote_{};

    QComboBox* multiCoreCombo_{};
    QComboBox* encoderThreadsCombo_{};
    QComboBox* captureModeCombo_{};
    QComboBox* pipelineLayersCombo_{};
    QSpinBox* storageUpdateSpin_{};

    QCheckBox* recordHotkeyCheck_{};
    QCheckBox* pauseHotkeyCheck_{};
    QCheckBox* captureHotkeyCheck_{};
    QCheckBox* selectHotkeyCheck_{};
    HotkeyEdit* recordHotkeyEdit_{};
    HotkeyEdit* pauseHotkeyEdit_{};
    HotkeyEdit* captureHotkeyEdit_{};
    HotkeyEdit* selectHotkeyEdit_{};
};

} // namespace ors
