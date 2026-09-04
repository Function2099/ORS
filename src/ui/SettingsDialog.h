#pragma once

#include "core/Config.h"

#include <QDialog>

class QLineEdit;

namespace ors {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(Config& config, QWidget* parent = nullptr);

private:
    void browseDirectory();
    void applyToConfig();

    Config& config_;
    QLineEdit* directoryEdit_{};
    QLineEdit* templateEdit_{};
};

} // namespace ors
