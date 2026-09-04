#include "ui/SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ors {

SettingsDialog::SettingsDialog(Config& config, QWidget* parent)
    : QDialog(parent)
    , config_(config)
{
    setWindowTitle(tr("設定"));
    resize(480, 160);

    directoryEdit_ = new QLineEdit(this);
    directoryEdit_->setPlaceholderText(tr("空白 = 系統「影片」資料夾"));
    directoryEdit_->setText(config_.data().directory);

    auto* browse = new QPushButton(tr("瀏覽…"), this);
    connect(browse, &QPushButton::clicked, this, &SettingsDialog::browseDirectory);

    auto* dirRow = new QWidget(this);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->addWidget(directoryEdit_);
    dirLayout->addWidget(browse);

    templateEdit_ = new QLineEdit(this);
    templateEdit_->setText(config_.data().filenameTemplate);

    auto* form = new QFormLayout;
    form->addRow(tr("輸出資料夾"), dirRow);
    form->addRow(tr("檔名模板"), templateEdit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        applyToConfig();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SettingsDialog::browseDirectory()
{
    const QString start = directoryEdit_->text().isEmpty()
        ? Config::resolvedOutputDirectory(config_.data())
        : directoryEdit_->text();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇輸出資料夾"), start);
    if (!dir.isEmpty()) {
        directoryEdit_->setText(dir);
    }
}

void SettingsDialog::applyToConfig()
{
    config_.data().directory = directoryEdit_->text().trimmed();
    const QString nameTemplate = templateEdit_->text().trimmed();
    if (!nameTemplate.isEmpty()) {
        config_.data().filenameTemplate = nameTemplate;
    }
}

} // namespace ors
