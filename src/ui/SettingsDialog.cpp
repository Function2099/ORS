#include "ui/SettingsDialog.h"

#include "audio/AudioDevices.h"
#include "core/FilenameTemplate.h"

#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVector>
#include <QVBoxLayout>

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace ors {
namespace {

QIcon chevronIcon(bool expanded)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    if (expanded) {
        path.moveTo(8, 11);
        path.lineTo(16, 21);
        path.lineTo(24, 11);
    } else {
        path.moveTo(11, 8);
        path.lineTo(21, 16);
        path.lineTo(11, 24);
    }
    path.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x1f, 0x4e, 0x4c));
    painter.drawPath(path);
    painter.end();
    return QIcon(pixmap);
}

QPushButton* makeNavButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAutoDefault(false);
    button->setObjectName(QStringLiteral("navButton"));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(30);
    return button;
}

QToolButton* makeGroupButton(const QString& text, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setCheckable(true);
    button->setAutoExclusive(false);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIcon(chevronIcon(false));
    button->setIconSize(QSize(10, 10));
    button->setObjectName(QStringLiteral("navGroupButton"));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(34);
    return button;
}

struct NavGroup {
    QToolButton* header = nullptr;
    QWidget* items = nullptr;
};

int navItemsHeight(QWidget* items)
{
    if (auto* scroll = qobject_cast<QScrollArea*>(items)) {
        if (QWidget* inner = scroll->widget()) {
            return inner->sizeHint().height();
        }
    }
    return items->sizeHint().height();
}

void stopNavAnimation(QWidget* items)
{
    const auto animations = items->findChildren<QVariantAnimation*>(
        QStringLiteral("navExpandAnim"), Qt::FindDirectChildrenOnly);
    for (auto* animation : animations) {
        animation->stop();
        animation->deleteLater();
    }
}

void setNavItemsOpen(QWidget* items, bool open)
{
    stopNavAnimation(items);
    if (open) {
        items->show();
        items->setFixedHeight(navItemsHeight(items));
        items->setMinimumHeight(0);
        items->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    items->setFixedHeight(0);
    items->hide();
}

void animateNavItems(QWidget* items, bool expand)
{
    stopNavAnimation(items);

    const int fullHeight = navItemsHeight(items);
    const int start = items->isVisible() ? items->height() : 0;
    const int end = expand ? fullHeight : 0;
    if (start == end) {
        if (expand) {
            items->show();
            items->setMinimumHeight(0);
            items->setMaximumHeight(QWIDGETSIZE_MAX);
        } else {
            items->setFixedHeight(0);
            items->hide();
        }
        return;
    }

    items->show();
    items->setFixedHeight(start);

    auto* animation = new QVariantAnimation(items);
    animation->setObjectName(QStringLiteral("navExpandAnim"));
    animation->setDuration(180);
    animation->setEasingCurve(expand ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    animation->setStartValue(start);
    animation->setEndValue(end);
    QObject::connect(animation, &QVariantAnimation::valueChanged, items, [items](const QVariant& value) {
        items->setFixedHeight(value.toInt());
    });
    QObject::connect(animation, &QVariantAnimation::finished, items, [items, expand] {
        if (expand) {
            items->setMinimumHeight(0);
            items->setMaximumHeight(QWIDGETSIZE_MAX);
        } else {
            items->setFixedHeight(0);
            items->hide();
        }
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

QPushButton* makeChipButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(QStringLiteral("chipButton"));
    return button;
}

QWidget* withStepButtons(QSpinBox* spin, QWidget* parent)
{
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    auto* minus = makeChipButton(QStringLiteral("−"), parent);
    auto* plus = makeChipButton(QStringLiteral("+"), parent);
    minus->setObjectName(QStringLiteral("stepButton"));
    plus->setObjectName(QStringLiteral("stepButton"));
    minus->setFixedSize(36, 32);
    plus->setFixedSize(36, 32);
    minus->setAutoRepeat(true);
    plus->setAutoRepeat(true);
    QObject::connect(minus, &QPushButton::clicked, spin, &QSpinBox::stepDown);
    QObject::connect(plus, &QPushButton::clicked, spin, &QSpinBox::stepUp);

    auto* row = new QWidget(parent);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(spin, 1);
    layout->addWidget(minus);
    layout->addWidget(plus);
    return row;
}

QIcon chevronDownIcon()
{
    return chevronIcon(true);
}

void compactCombo(QComboBox* combo)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(1);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QWidget* withDropButton(QComboBox* combo, QWidget* parent)
{
    combo->setObjectName(QStringLiteral("plainCombo"));
    compactCombo(combo);

    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("dropButton"));
    button->setIcon(chevronDownIcon());
    button->setIconSize(QSize(14, 14));
    button->setFixedSize(36, 32);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QObject::connect(button, &QToolButton::clicked, combo, &QComboBox::showPopup);

    auto* row = new QWidget(parent);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(combo, 1);
    layout->addWidget(button);
    return row;
}

} // namespace

SettingsDialog::SettingsDialog(Config& config, QWidget* parent)
    : QDialog(parent)
    , config_(config)
{
    setObjectName(QStringLiteral("settingsRoot"));
    setWindowTitle(tr("設定"));
    setAutoFillBackground(true);
    resize(680, 560);

    setStyleSheet(QStringLiteral(
        "#settingsRoot { background: #f6f3ee; }"
        "#headerBar { background: #163a38; }"
        "#brandLabel { color: #f4f7f6; font-size: 13px; font-weight: 700; letter-spacing: 1px; }"
        "#navPane { background: #efeae3; border-right: 1px solid #ddd6cc; }"
        "#navGroupItems { background: transparent; }"
        "#navGroupButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  color: #1f4e4c;"
        "  font-weight: 700;"
        "  text-align: left;"
        "}"
        "#navGroupButton:hover { background: #e4ddd4; }"
        "#navGroupButton:checked { background: #e4ddd4; }"
        "#navButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  color: #3c4a48;"
        "  text-align: left;"
        "}"
        "#navButton:hover { background: #e4ddd4; }"
        "#navButton:checked {"
        "  background: #163a38;"
        "  color: #f4f7f6;"
        "  font-weight: 600;"
        "}"
        "#contentPane { background: #f6f3ee; }"
        "#pageTitle { color: #1f4e4c; font-size: 15px; font-weight: 700; }"
        "#footerRow { background: #efeae3; }"
        "#chipButton {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  color: #1f4e4c;"
        "}"
        "#chipButton:hover { background: #fbf8f4; border-color: #c9c0b4; }"
        "#chipButton::menu-indicator { image: none; width: 0; }"
        "#stepButton {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  color: #1f4e4c;"
        "  font-size: 16px;"
        "  font-weight: 700;"
        "  padding: 0;"
        "}"
        "#stepButton:hover { background: #efeae3; border-color: #c9c0b4; }"
        "#stepButton:pressed { background: #e4ddd4; }"
        "#dropButton {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  padding: 0;"
        "}"
        "#dropButton:hover { background: #efeae3; border-color: #c9c0b4; }"
        "#dropButton:pressed { background: #e4ddd4; }"
        "#primaryButton {"
        "  background: #163a38;"
        "  color: #f4f7f6;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 5px 16px;"
        "  font-weight: 600;"
        "}"
        "#primaryButton:hover { background: #1f4e4c; }"
        "#primaryButton:pressed { background: #122e2c; }"
        "QLabel { color: #3c4a48; }"
        "#previewLabel { color: #1f4e4c; font-weight: 600; }"
        "#metaLabel { color: #6b7573; }"
        "QLineEdit {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  min-height: 22px;"
        "  color: #1f4e4c;"
        "  selection-background-color: #163a38;"
        "}"
        "QSpinBox, QComboBox {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  min-height: 28px;"
        "  color: #1f4e4c;"
        "  selection-background-color: #163a38;"
        "}"
        "QComboBox#plainCombo::drop-down { border: none; width: 0px; }"
        "QComboBox#plainCombo::down-arrow { image: none; width: 0px; height: 0px; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #163a38; }"
        "QComboBox QAbstractItemView {"
        "  background: #fff;"
        "  border: 1px solid #ddd6cc;"
        "  selection-background-color: #163a38;"
        "  selection-color: #f4f7f6;"
        "  padding: 4px;"
        "}"
        "QCheckBox { color: #3c4a48; spacing: 8px; }"
        "#sectionLabel { color: #1f4e4c; font-weight: 700; }"
        "#videoCard { background: #fff; border: 1px solid #ddd6cc; border-radius: 8px; }"
        "QSlider::groove:horizontal { height: 4px; background: #ddd6cc; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  background: #163a38; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;"
        "}"
        "QScrollArea { background: transparent; border: none; }"));

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("headerBar"));
    auto* title = new QLabel(tr("設定"), header);
    title->setObjectName(QStringLiteral("brandLabel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    auto* nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("navPane"));
    nav->setFixedWidth(176);
    pageGroup_ = new QButtonGroup(this);
    pageGroup_->setExclusive(true);

    auto* navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(8, 10, 8, 10);
    navLayout->setSpacing(4);

    QVector<NavGroup> groups;
    const auto addGroup = [&](const QString& title, std::initializer_list<std::pair<const char*, Page>> items) {
        auto* header = makeGroupButton(title, nav);
        auto* inner = new QWidget;
        auto* itemLayout = new QVBoxLayout(inner);
        itemLayout->setContentsMargins(10, 0, 0, 2);
        itemLayout->setSpacing(2);
        for (const auto& item : items) {
            auto* button = makeNavButton(tr(item.first), inner);
            pageGroup_->addButton(button, static_cast<int>(item.second));
            itemLayout->addWidget(button);
        }

        auto* wrap = new QScrollArea(nav);
        wrap->setObjectName(QStringLiteral("navGroupItems"));
        wrap->setWidget(inner);
        wrap->setWidgetResizable(true);
        wrap->setFrameShape(QFrame::NoFrame);
        wrap->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        wrap->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        wrap->setFocusPolicy(Qt::NoFocus);
        wrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        wrap->setFixedHeight(0);
        wrap->hide();
        navLayout->addWidget(header);
        navLayout->addWidget(wrap);
        groups.push_back({header, wrap});
    };

    addGroup(tr("錄製與擷取"), {
        {QT_TR_NOOP("錄製"), Page::Record},
        {QT_TR_NOOP("聲音"), Page::Audio},
        {QT_TR_NOOP("擷取"), Page::Capture},
        {QT_TR_NOOP("GIF動畫"), Page::Gif},
    });
    addGroup(tr("輸出與效果"), {
        {QT_TR_NOOP("儲存"), Page::Save},
        {QT_TR_NOOP("浮水印"), Page::Watermark},
        {QT_TR_NOOP("時間限制"), Page::TimeLimit},
    });
    addGroup(tr("操作與偏好"), {
        {QT_TR_NOOP("快捷鍵"), Page::Hotkeys},
        {QT_TR_NOOP("語言"), Page::Language},
    });
    addGroup(tr("系統與效能"), {
        {QT_TR_NOOP("效能"), Page::Performance},
    });
    navLayout->addStretch();

    for (const auto& group : groups) {
        connect(group.header, &QToolButton::clicked, this, [groups, header = group.header] {
            const bool open = header->isChecked();
            for (const auto& item : groups) {
                const bool isThis = item.header == header && open;
                item.header->setChecked(isThis);
                item.header->setIcon(chevronIcon(isThis));
                animateNavItems(item.items, isThis);
            }
        });
    }

    auto* pageTitle = new QLabel(this);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));

    pages_ = new QStackedWidget(this);
    pages_->setObjectName(QStringLiteral("contentPane"));
    pages_->addWidget(createRecordPage());
    pages_->addWidget(createAudioPage());
    pages_->addWidget(createCapturePage());
    pages_->addWidget(createComingSoonPage());
    pages_->addWidget(createComingSoonPage());
    pages_->addWidget(createSavePage());
    pages_->addWidget(createComingSoonPage());
    pages_->addWidget(createComingSoonPage());
    pages_->addWidget(createComingSoonPage());
    pages_->addWidget(createLanguagePage());

    auto* pageWrap = new QWidget(this);
    auto* pageWrapLayout = new QVBoxLayout(pageWrap);
    pageWrapLayout->setContentsMargins(0, 0, 0, 0);
    pageWrapLayout->setSpacing(0);
    auto* titleWrap = new QWidget(pageWrap);
    auto* titleLayout = new QHBoxLayout(titleWrap);
    titleLayout->setContentsMargins(16, 12, 16, 0);
    titleLayout->addWidget(pageTitle);
    pageWrapLayout->addWidget(titleWrap);
    pageWrapLayout->addWidget(pages_, 1);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(nav);
    bodyLayout->addWidget(pageWrap, 1);

    const auto showPage = [this, pageTitle](int id) {
        pages_->setCurrentIndex(id);
        if (auto* button = pageGroup_->button(id)) {
            button->setChecked(true);
            pageTitle->setText(button->text());
        }
    };
    connect(pageGroup_, &QButtonGroup::idClicked, this, showPage);

    if (!groups.isEmpty()) {
        auto* first = groups.front().header;
        first->setChecked(true);
        first->setIcon(chevronIcon(true));
        setNavItemsOpen(groups.front().items, true);
    }
    showPage(static_cast<int>(Page::Record));

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("footerRow"));
    auto* reset = makeChipButton(tr("重設"), footer);
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    auto* cancel = makeChipButton(tr("取消"), footer);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* ok = new QPushButton(tr("確定"), footer);
    ok->setObjectName(QStringLiteral("primaryButton"));
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFocusPolicy(Qt::NoFocus);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, [this] {
        applyToConfig();
        accept();
    });
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 8, 12, 8);
    footerLayout->setSpacing(6);
    footerLayout->addWidget(reset);
    footerLayout->addStretch();
    footerLayout->addWidget(cancel);
    footerLayout->addWidget(ok);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(body, 1);
    layout->addWidget(footer);

    connect(prefixEdit_, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    connect(templateEdit_, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    connect(startNumberSpin_, &QSpinBox::valueChanged, this, [this](int) { updatePreview(); });

    auto* previewTimer = new QTimer(this);
    previewTimer->setInterval(3000);
    connect(previewTimer, &QTimer::timeout, this, &SettingsDialog::updatePreview);
    previewTimer->start();
    loadRecordingFrom(config_.data());
    loadAudioFrom(config_.data());
    loadCaptureFrom(config_.data());
    updatePreview();
}

QWidget* SettingsDialog::createSavePage()
{
    auto* savePage = new QWidget;
    directoryEdit_ = new QLineEdit(savePage);
    directoryEdit_->setPlaceholderText(tr("空白 = 系統「影片」資料夾"));
    directoryEdit_->setText(config_.data().directory);
    auto* browse = makeChipButton(tr("瀏覽"), savePage);
    connect(browse, &QPushButton::clicked, this, &SettingsDialog::browseDirectory);
    auto* dirRow = new QWidget(savePage);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(6);
    dirLayout->addWidget(directoryEdit_);
    dirLayout->addWidget(browse);

    templateEdit_ = new QLineEdit(savePage);
    templateEdit_->setText(config_.data().filenameTemplate);
    auto* presetButton = new QToolButton(savePage);
    presetButton->setObjectName(QStringLiteral("chipButton"));
    presetButton->setText(tr("範本"));
    presetButton->setCursor(Qt::PointingHandCursor);
    presetButton->setFocusPolicy(Qt::NoFocus);
    presetButton->setPopupMode(QToolButton::InstantPopup);
    presetButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    const QString stamp = QStringLiteral("<YYYY_MM_DD_HH_NN_SS_Z>");
    auto* filenameMenu = new QMenu(tr("範本"), presetButton);
    auto addPreset = [this, filenameMenu](const QString& title, const QString& tmpl) {
        filenameMenu->addAction(title, this, [this, tmpl] {
            templateEdit_->setText(tmpl);
            templateEdit_->setFocus();
            templateEdit_->end(false);
            updatePreview();
        });
    };
    addPreset(tr("前綴 + 日期 + 時間"), QStringLiteral("<Prefix>_%1").arg(stamp));
    addPreset(tr("程式名稱 + 日期 + 時間"), QStringLiteral("<Name>_%1").arg(stamp));
    addPreset(tr("使用者識別碼 + 日期 + 時間"), QStringLiteral("<User>_%1").arg(stamp));
    addPreset(tr("顯示使用者名稱 + 日期 + 時間"), QStringLiteral("<DisplayName>_%1").arg(stamp));
    filenameMenu->addSeparator();
    addPreset(tr("前綴 + 當地語系化日期 + 編號(#)"), QStringLiteral("<Prefix>_<Date>_#"));
    addPreset(tr("程式名稱 + 當地語系化日期 + 編號(#)"), QStringLiteral("<Name>_<Date>_#"));
    addPreset(tr("使用者識別碼 + 當地語系化日期 + 編號(#)"), QStringLiteral("<User>_<Date>_#"));
    addPreset(tr("顯示使用者名稱 + 當地語系化日期 + 編號(#)"), QStringLiteral("<DisplayName>_<Date>_#"));
    filenameMenu->addSeparator();
    addPreset(tr("前綴 + 編號1(#)"), QStringLiteral("<Prefix>_#"));
    addPreset(tr("前綴 + 編號2(##)"), QStringLiteral("<Prefix>_##"));
    addPreset(tr("前綴 + 編號3(###)"), QStringLiteral("<Prefix>_###"));
    filenameMenu->addSeparator();
    addPreset(tr("程式名稱 + 編號1(#)"), QStringLiteral("<Name>_#"));
    addPreset(tr("程式名稱 + 編號2(##)"), QStringLiteral("<Name>_##"));
    addPreset(tr("程式名稱 + 編號3(###)"), QStringLiteral("<Name>_###"));
    filenameMenu->addSeparator();
    addPreset(tr("編號1(#)"), QStringLiteral("#"));
    addPreset(tr("編號2(##)"), QStringLiteral("##"));
    addPreset(tr("編號3(###)"), QStringLiteral("###"));
    filenameMenu->addSeparator();
    filenameMenu->addAction(tr("說明"), this, &SettingsDialog::showFilenameHelp);
    presetButton->setMenu(filenameMenu);

    auto* nameRow = new QWidget(savePage);
    auto* nameLayout = new QHBoxLayout(nameRow);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(6);
    nameLayout->addWidget(templateEdit_);
    nameLayout->addWidget(presetButton);

    startNumberSpin_ = new QSpinBox(savePage);
    startNumberSpin_->setRange(1, 999999);
    startNumberSpin_->setValue(std::max(1, config_.data().filenameStartNumber));
    startNumberSpin_->setMinimumWidth(96);

    previewLabel_ = new QLabel(savePage);
    previewLabel_->setObjectName(QStringLiteral("previewLabel"));
    previewLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLabel_->setWordWrap(true);

    auto* saveForm = new QFormLayout(savePage);
    saveForm->setContentsMargins(16, 10, 16, 12);
    saveForm->setHorizontalSpacing(12);
    saveForm->setVerticalSpacing(10);
    saveForm->addRow(tr("輸出資料夾"), dirRow);
    saveForm->addRow(tr("檔案名稱"), nameRow);
    saveForm->addRow(tr("起始號碼"), withStepButtons(startNumberSpin_, savePage));
    saveForm->addRow(tr("範例"), previewLabel_);
    return savePage;
}

QWidget* SettingsDialog::createLanguagePage()
{
    auto* languagePage = new QWidget;
    languageCombo_ = new QComboBox(languagePage);
    languageCombo_->addItem(tr("繁體中文"), QStringLiteral("zh_TW"));
    languageCombo_->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    const int langIndex = languageCombo_->findData(config_.data().language);
    languageCombo_->setCurrentIndex(langIndex >= 0 ? langIndex : 0);
    auto* languageNote = new QLabel(tr("語言將於下次啟動時套用。"), languagePage);
    languageNote->setObjectName(QStringLiteral("metaLabel"));
    languageNote->setWordWrap(true);
    auto* languageForm = new QFormLayout(languagePage);
    languageForm->setContentsMargins(16, 10, 16, 12);
    languageForm->addRow(tr("語言"), withDropButton(languageCombo_, languagePage));
    languageForm->addRow(QString(), languageNote);
    return languagePage;
}

QWidget* SettingsDialog::createComingSoonPage()
{
    auto* page = new QWidget;
    auto* note = new QLabel(tr("此頁面將於後續實作。"), page);
    note->setObjectName(QStringLiteral("metaLabel"));
    note->setWordWrap(true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createCapturePage()
{
    auto* page = new QWidget;
    captureCursorCheck_ = new QCheckBox(tr("包含游標"), page);
    captureFormatCombo_ = new QComboBox(page);
    captureFormatCombo_->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    captureFormatCombo_->addItem(QStringLiteral("JPG"), QStringLiteral("jpg"));
    captureFormatCombo_->addItem(QStringLiteral("BMP"), QStringLiteral("bmp"));

    auto* form = new QFormLayout(page);
    form->setContentsMargins(16, 10, 16, 12);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    form->addRow(QString(), captureCursorCheck_);
    form->addRow(tr("圖檔格式"), withDropButton(captureFormatCombo_, page));
    return page;
}

QWidget* SettingsDialog::createAudioPage()
{
    auto* inner = new QWidget;

    auto* systemLabel = new QLabel(tr("系統聲音"), inner);
    systemLabel->setObjectName(QStringLiteral("sectionLabel"));
    auto* systemCard = new QWidget(inner);
    systemCard->setObjectName(QStringLiteral("videoCard"));
    systemAudioCheck_ = new QCheckBox(tr("錄製系統聲音"), systemCard);
    auto* systemLayout = new QVBoxLayout(systemCard);
    systemLayout->setContentsMargins(12, 12, 12, 12);
    systemLayout->addWidget(systemAudioCheck_);

    auto* micLabel = new QLabel(tr("麥克風設定"), inner);
    micLabel->setObjectName(QStringLiteral("sectionLabel"));
    auto* micCard = new QWidget(inner);
    micCard->setObjectName(QStringLiteral("videoCard"));

    microphoneCombo_ = new QComboBox(micCard);
    populateMicrophoneCombo();
    connect(microphoneCombo_, &QComboBox::currentIndexChanged, this, &SettingsDialog::syncMicSourceEnabled);

    micSourceCombo_ = new QComboBox(micCard);
    micSourceCombo_->addItem(tr("輸入 1 (左聲道)"), QStringLiteral("left"));
    micSourceCombo_->addItem(tr("輸入 2 (右聲道)"), QStringLiteral("right"));
    micSourceCombo_->addItem(tr("輸入 1 + 2 (立體聲)"), QStringLiteral("stereo"));
    micSourceRow_ = withDropButton(micSourceCombo_, micCard);

    auto addRow = [](QGridLayout* grid, int row, QWidget* label, QWidget* field) {
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        grid->addWidget(label, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
    };
    auto* deviceLabel = new QLabel(tr("裝置"), micCard);
    auto* sourceLabel = new QLabel(tr("輸入來源"), micCard);
    auto* micGrid = new QGridLayout(micCard);
    micGrid->setContentsMargins(12, 12, 12, 12);
    micGrid->setHorizontalSpacing(16);
    micGrid->setVerticalSpacing(12);
    micGrid->setColumnStretch(1, 1);
    addRow(micGrid, 0, deviceLabel, withDropButton(microphoneCombo_, micCard));
    addRow(micGrid, 1, sourceLabel, micSourceRow_);

    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(16, 10, 16, 12);
    innerLayout->setSpacing(8);
    innerLayout->addWidget(systemLabel);
    innerLayout->addWidget(systemCard);
    innerLayout->addSpacing(6);
    innerLayout->addWidget(micLabel);
    innerLayout->addWidget(micCard);
    innerLayout->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inner);
    return scroll;
}

QWidget* SettingsDialog::createRecordPage()
{
    auto* inner = new QWidget;
    includeCursorCheck_ = new QCheckBox(tr("包含游標"), inner);
    alwaysOnTopCheck_ = new QCheckBox(tr("總是最上層"), inner);
    trayIconCheck_ = new QCheckBox(tr("使用工作列圖示"), inner);
    hideMinimizedCheck_ = new QCheckBox(tr("最小化時隱藏應用程式"), inner);
    hideStartupCheck_ = new QCheckBox(tr("應用程式啟動時隱藏"), inner);
    connect(trayIconCheck_, &QCheckBox::toggled, this, &SettingsDialog::syncTrayChildren);

    auto* trayChildren = new QWidget(inner);
    auto* trayChildLayout = new QVBoxLayout(trayChildren);
    trayChildLayout->setContentsMargins(24, 0, 0, 4);
    trayChildLayout->setSpacing(4);
    trayChildLayout->addWidget(hideMinimizedCheck_);
    trayChildLayout->addWidget(hideStartupCheck_);

    auto* videoLabel = new QLabel(tr("視訊"), inner);
    videoLabel->setObjectName(QStringLiteral("sectionLabel"));

    auto* videoCard = new QWidget(inner);
    videoCard->setObjectName(QStringLiteral("videoCard"));

    frameRateSpin_ = new QSpinBox(videoCard);
    frameRateSpin_->setRange(1, 120);
    frameRateSpin_->setMinimumWidth(72);

    qualityCombo_ = new QComboBox(videoCard);
    qualityCombo_->addItem(tr("非常高"), QStringLiteral("very-high"));
    qualityCombo_->addItem(tr("高"), QStringLiteral("high"));
    qualityCombo_->addItem(tr("中"), QStringLiteral("medium"));
    qualityCombo_->addItem(tr("低"), QStringLiteral("low"));
    qualityCombo_->addItem(tr("自訂"), QStringLiteral("custom"));
    connect(qualityCombo_, &QComboBox::currentIndexChanged, this, &SettingsDialog::syncCustomQualityButton);

    customQualityButton_ = makeChipButton(tr("自訂…"), videoCard);
    connect(customQualityButton_, &QPushButton::clicked, this, &SettingsDialog::editCustomBitrate);
    auto* qualityRow = new QWidget(videoCard);
    qualityRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* qualityLayout = new QHBoxLayout(qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    qualityLayout->setSpacing(6);
    qualityLayout->addWidget(withDropButton(qualityCombo_, videoCard), 1);
    qualityLayout->addWidget(customQualityButton_);

    keyframeLabel_ = new QLabel(videoCard);
    keyframeSlider_ = new QSlider(Qt::Horizontal, videoCard);
    keyframeSlider_->setRange(1, 30);
    keyframeSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    keyframeSlider_->setMinimumHeight(22);
    connect(keyframeSlider_, &QSlider::valueChanged, this, &SettingsDialog::updateKeyframeLabel);

    resolutionCombo_ = new QComboBox(videoCard);
    resolutionCombo_->addItem(tr("寬度為 8 的倍數，高度為 4 的倍數"), QStringLiteral("8x4"));
    resolutionCombo_->addItem(tr("寬度與高度皆為 2 的倍數"), QStringLiteral("2x2"));
    resolutionCombo_->addItem(tr("寬度與高度皆為 16 的倍數"), QStringLiteral("16x16"));

    frameRateModeCombo_ = new QComboBox(videoCard);
    frameRateModeCombo_->addItem(tr("可變幀速率（快）"), QStringLiteral("vfr"));
    frameRateModeCombo_->addItem(tr("固定幀速率"), QStringLiteral("cfr"));

    auto addVideoRow = [](QGridLayout* grid, int row, QWidget* label, QWidget* field) {
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        grid->addWidget(label, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
    };

    auto* fpsLabel = new QLabel(tr("每秒幀數") + QStringLiteral(" FPS"), videoCard);
    auto* qualityLabel = new QLabel(tr("品質"), videoCard);
    auto* resolutionLabel = new QLabel(tr("解碼器解析度相容性"), videoCard);
    auto* frameRateModeLabel = new QLabel(tr("幀速率模式"), videoCard);

    auto* videoGrid = new QGridLayout(videoCard);
    videoGrid->setContentsMargins(12, 12, 12, 12);
    videoGrid->setHorizontalSpacing(16);
    videoGrid->setVerticalSpacing(12);
    videoGrid->setColumnStretch(1, 1);
    addVideoRow(videoGrid, 0, fpsLabel, withStepButtons(frameRateSpin_, videoCard));
    addVideoRow(videoGrid, 1, qualityLabel, qualityRow);
    addVideoRow(videoGrid, 2, keyframeLabel_, keyframeSlider_);
    addVideoRow(videoGrid, 3, resolutionLabel, withDropButton(resolutionCombo_, videoCard));
    addVideoRow(videoGrid, 4, frameRateModeLabel, withDropButton(frameRateModeCombo_, videoCard));

    prefixEdit_ = new QLineEdit(inner);

    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(16, 10, 16, 12);
    innerLayout->setSpacing(8);
    innerLayout->addWidget(includeCursorCheck_);
    innerLayout->addWidget(alwaysOnTopCheck_);
    innerLayout->addWidget(trayIconCheck_);
    innerLayout->addWidget(trayChildren);
    innerLayout->addSpacing(6);
    innerLayout->addWidget(videoLabel);
    innerLayout->addWidget(videoCard);
    innerLayout->addSpacing(8);
    auto* prefixForm = new QFormLayout;
    prefixForm->addRow(tr("檔案名稱前綴"), prefixEdit_);
    innerLayout->addLayout(prefixForm);
    innerLayout->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inner);
    return scroll;
}

QString SettingsDialog::currentExtension() const
{
    return config_.data().container.compare(QLatin1String("wmv"), Qt::CaseInsensitive) == 0
        ? QStringLiteral(".wmv")
        : QStringLiteral(".mp4");
}

void SettingsDialog::loadRecordingFrom(const ConfigData& cfg)
{
    includeCursorCheck_->setChecked(cfg.includeCursor);
    alwaysOnTopCheck_->setChecked(cfg.alwaysOnTop);
    trayIconCheck_->setChecked(cfg.useTrayIcon);
    hideMinimizedCheck_->setChecked(cfg.hideWhenMinimized);
    hideStartupCheck_->setChecked(cfg.hideOnStartup);
    frameRateSpin_->setValue(cfg.frameRate);
    const int qualityIndex = qualityCombo_->findData(cfg.videoQuality);
    qualityCombo_->setCurrentIndex(qualityIndex >= 0 ? qualityIndex : 0);
    customBitrateKbps_ = cfg.customBitrateKbps;
    keyframeSlider_->setValue(cfg.keyframeInterval);
    const int alignIndex = resolutionCombo_->findData(cfg.resolutionAlign);
    resolutionCombo_->setCurrentIndex(alignIndex >= 0 ? alignIndex : 0);
    const int modeIndex = frameRateModeCombo_->findData(cfg.frameRateMode);
    frameRateModeCombo_->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
    prefixEdit_->setText(cfg.filenamePrefix);
    syncTrayChildren();
    syncCustomQualityButton();
    updateKeyframeLabel();
}

void SettingsDialog::populateMicrophoneCombo()
{
    microphoneCombo_->clear();
    microphoneCombo_->addItem(tr("不錄製麥克風"), QString());
    microphoneCombo_->addItem(tr("預設麥克風"), QStringLiteral("default"));
    for (const AudioDeviceInfo& device : listCaptureDevices()) {
        if (device.id.isEmpty() || device.id == QLatin1String("default")) {
            continue;
        }
        microphoneCombo_->addItem(device.name, device.id);
    }
}

void SettingsDialog::loadCaptureFrom(const ConfigData& cfg)
{
    captureCursorCheck_->setChecked(cfg.captureIncludeCursor);
    const int formatIndex = captureFormatCombo_->findData(
        Config::normalizedCaptureImageFormat(cfg.captureImageFormat));
    captureFormatCombo_->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);
}

void SettingsDialog::loadAudioFrom(const ConfigData& cfg)
{
    systemAudioCheck_->setChecked(cfg.systemAudio);
    int micIndex = microphoneCombo_->findData(cfg.microphoneId);
    if (micIndex < 0 && !cfg.microphoneId.isEmpty()) {
        microphoneCombo_->addItem(cfg.microphoneId, cfg.microphoneId);
        micIndex = microphoneCombo_->count() - 1;
    }
    microphoneCombo_->setCurrentIndex(micIndex >= 0 ? micIndex : 0);
    const int sourceIndex = micSourceCombo_->findData(cfg.microphoneInputSource);
    micSourceCombo_->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : 2);
    syncMicSourceEnabled();
}

void SettingsDialog::syncMicSourceEnabled()
{
    const bool hasMic = microphoneCombo_ && !microphoneCombo_->currentData().toString().isEmpty();
    if (micSourceCombo_) {
        micSourceCombo_->setEnabled(hasMic);
    }
    if (micSourceRow_) {
        micSourceRow_->setEnabled(hasMic);
    }
}

void SettingsDialog::syncTrayChildren()
{
    const bool enabled = trayIconCheck_->isChecked();
    hideMinimizedCheck_->setEnabled(enabled);
    hideStartupCheck_->setEnabled(enabled);
}

void SettingsDialog::syncCustomQualityButton()
{
    customQualityButton_->setEnabled(
        qualityCombo_->currentData().toString() == QLatin1String("custom"));
}

void SettingsDialog::updateKeyframeLabel()
{
    keyframeLabel_->setText(tr("關鍵影格速率: %1").arg(keyframeSlider_->value()));
}

void SettingsDialog::editCustomBitrate()
{
    bool ok = false;
    const int value = QInputDialog::getInt(
        this,
        tr("自訂品質"),
        tr("位元率 (kbps)"),
        customBitrateKbps_,
        500,
        100000,
        100,
        &ok);
    if (ok) {
        customBitrateKbps_ = value;
    }
}

void SettingsDialog::browseDirectory()
{
    const QString start = directoryEdit_->text().isEmpty()
        ? Config::resolvedOutputDirectory(config_.data())
        : directoryEdit_->text();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇輸出資料夾"), start);
    if (!dir.isEmpty()) {
        directoryEdit_->setText(QDir::toNativeSeparators(dir));
    }
}

void SettingsDialog::showFilenameHelp()
{
    QMessageBox::information(
        this,
        tr("檔案名稱"),
        tr("可插入的欄位：\n"
           "<Prefix> 前綴（在「錄製」頁設定）\n"
           "<Name> 程式名稱\n"
           "<User> 使用者識別碼\n"
           "<DisplayName> 顯示使用者名稱\n"
           "<YYYY_MM_DD_HH_NN_SS_Z> 日期與時間\n"
           "<Date> 當地語系化日期\n"
           "# / ## / ### 編號（位數會補零）"));
}

void SettingsDialog::updatePreview()
{
    const QString name = expandFilenameTemplate(
        templateEdit_->text(),
        prefixEdit_->text(),
        QDateTime::currentDateTime(),
        startNumberSpin_->value());
    previewLabel_->setText(name + currentExtension());
}

void SettingsDialog::resetToDefaults()
{
    if (QMessageBox::question(this, tr("設定"), tr("要將設定恢復為預設值嗎？")) != QMessageBox::Yes) {
        return;
    }
    const ConfigData defaults;
    prefixEdit_->setText(defaults.filenamePrefix);
    directoryEdit_->setText(defaults.directory);
    templateEdit_->setText(defaults.filenameTemplate);
    startNumberSpin_->setValue(defaults.filenameStartNumber);
    loadRecordingFrom(defaults);
    loadAudioFrom(defaults);
    loadCaptureFrom(defaults);
    const int langIndex = languageCombo_->findData(defaults.language);
    languageCombo_->setCurrentIndex(langIndex >= 0 ? langIndex : 0);
    updatePreview();
}

void SettingsDialog::applyToConfig()
{
    config_.data().directory = directoryEdit_->text().trimmed();
    const QString nameTemplate = templateEdit_->text().trimmed();
    if (!nameTemplate.isEmpty()) {
        config_.data().filenameTemplate = nameTemplate;
    }
    const QString prefix = prefixEdit_->text().trimmed();
    if (!prefix.isEmpty()) {
        config_.data().filenamePrefix = prefix;
    }
    config_.data().filenameStartNumber = startNumberSpin_->value();
    config_.data().language = languageCombo_->currentData().toString();
    config_.data().includeCursor = includeCursorCheck_->isChecked();
    config_.data().alwaysOnTop = alwaysOnTopCheck_->isChecked();
    config_.data().useTrayIcon = trayIconCheck_->isChecked();
    config_.data().hideWhenMinimized = hideMinimizedCheck_->isChecked();
    config_.data().hideOnStartup = hideStartupCheck_->isChecked();
    config_.data().frameRate = frameRateSpin_->value();
    config_.data().videoQuality = qualityCombo_->currentData().toString();
    config_.data().customBitrateKbps = customBitrateKbps_;
    config_.data().keyframeInterval = keyframeSlider_->value();
    config_.data().resolutionAlign = resolutionCombo_->currentData().toString();
    config_.data().frameRateMode = frameRateModeCombo_->currentData().toString();
    config_.data().systemAudio = systemAudioCheck_->isChecked();
    config_.data().microphoneId = microphoneCombo_->currentData().toString();
    config_.data().microphoneInputSource = micSourceCombo_->currentData().toString();
    if (config_.data().microphoneInputSource.isEmpty()) {
        config_.data().microphoneInputSource = QStringLiteral("stereo");
    }
    config_.data().captureIncludeCursor = captureCursorCheck_->isChecked();
    config_.data().captureImageFormat = Config::normalizedCaptureImageFormat(
        captureFormatCombo_->currentData().toString());
}

} // namespace ors
