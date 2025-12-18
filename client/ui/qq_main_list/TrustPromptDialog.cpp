#include "TrustPromptDialog.h"

#include <QDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "../common/Theme.h"
#include "../common/UiSettings.h"

namespace {

QString NormalizeSas(QString in) {
    in = in.trimmed().toLower();
    QString out;
    out.reserve(in.size());
    for (const QChar ch : in) {
        if (ch.isSpace() || ch == QLatin1Char('-')) {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

QLineEdit *readonlyField(const QString &value, QWidget *parent) {
    auto *edit = new QLineEdit(parent);
    edit->setReadOnly(true);
    edit->setText(value);
    edit->setCursorPosition(0);
    edit->setStyleSheet(
        QStringLiteral(
            "QLineEdit { background: %1; border: 1px solid %2; border-radius: 8px; "
            "color: %3; padding: 7px 10px; font-size: 13px; }")
            .arg(Theme::uiInputBg().name(),
                 Theme::uiInputBorder().name(),
                 Theme::uiTextMain().name()));
    return edit;
}

QLineEdit *inputField(const QString &value, QWidget *parent) {
    auto *edit = new QLineEdit(parent);
    edit->setText(value);
    edit->setClearButtonEnabled(true);
    edit->setStyleSheet(
        QStringLiteral(
            "QLineEdit { background: %1; border: 1px solid %2; border-radius: 8px; "
            "color: %3; padding: 7px 10px; font-size: 13px; }"
            "QLineEdit:focus { border-color: %4; }"
            "QLineEdit { selection-background-color: %4; selection-color: white; }")
            .arg(Theme::uiInputBg().name(),
                 Theme::uiInputBorder().name(),
                 Theme::uiTextMain().name(),
                 Theme::uiAccentBlue().name()));
    return edit;
}

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(34);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %5; }")
            .arg(Theme::uiTextMain().name(),
                 Theme::uiPanelBg().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name(),
                 Theme::uiSelectedBg().name()));
    return btn;
}

QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(34);
    const QColor base = Theme::uiAccentBlue();
    QColor disabledBg = Theme::uiBorder();
    disabledBg.setAlpha(180);
    QColor disabledText = Theme::uiTextMuted();
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: white; background: %1; border: none; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %2; }"
            "QPushButton:pressed { background: %3; }"
            "QPushButton:disabled { background: %4; color: %5; }")
            .arg(base.name(),
                 base.lighter(112).name(),
                 base.darker(110).name(),
                 disabledBg.name(QColor::HexArgb),
                 disabledText.name(QColor::HexArgb)));
    return btn;
}

void maybeApplyMonoFont(QLineEdit *edit) {
    if (!edit) {
        return;
    }
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(Theme::defaultFont(10).pointSize());
    edit->setFont(mono);
}

}  // namespace

bool PromptTrustWithSas(QWidget *parent,
                        const QString &title,
                        const QString &description,
                        const QString &fingerprintHex,
                        const QString &sasShown,
                        QString &outSasInput,
                        const QString &entityLabel,
                        const QString &entityValue) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setModal(true);
    dlg.setMinimumWidth(520);
    dlg.setStyleSheet(QStringLiteral("QDialog { background: %1; }").arg(Theme::uiWindowBg().name()));

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    auto *icon = new QLabel(&dlg);
    icon->setFixedSize(28, 28);
    icon->setPixmap(dlg.style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(28, 28));
    header->addWidget(icon, 0, Qt::AlignTop);

    auto *titleLabel = new QLabel(title, &dlg);
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 14px; font-weight: 600;")
            .arg(Theme::uiTextMain().name()));
    header->addWidget(titleLabel, 1);
    root->addLayout(header);

    auto *desc = new QLabel(description, &dlg);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::PlainText);
    desc->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                            .arg(Theme::uiTextSub().name()));
    root->addWidget(desc);

    auto addField = [&](const QString &labelText, QLineEdit *field) {
        auto *label = new QLabel(labelText, &dlg);
        label->setTextFormat(Qt::PlainText);
        label->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
                                 .arg(Theme::uiTextMain().name()));
        root->addWidget(label);
        root->addWidget(field);
    };

    if (!entityValue.trimmed().isEmpty()) {
        addField(entityLabel.isEmpty()
                     ? UiSettings::Tr(QStringLiteral("对端"), QStringLiteral("Peer"))
                     : entityLabel,
                 readonlyField(entityValue, &dlg));
    }

    auto *fingerprintEdit = readonlyField(fingerprintHex, &dlg);
    maybeApplyMonoFont(fingerprintEdit);
    addField(UiSettings::Tr(QStringLiteral("指纹"), QStringLiteral("Fingerprint")), fingerprintEdit);

    auto *sasEdit = readonlyField(sasShown, &dlg);
    maybeApplyMonoFont(sasEdit);
    addField(UiSettings::Tr(QStringLiteral("安全码（SAS）"), QStringLiteral("SAS")), sasEdit);

    auto *inputLabel = new QLabel(
        UiSettings::Tr(QStringLiteral("请输入上面显示的安全码（可包含 '-'，忽略大小写）："),
                       QStringLiteral("Enter the SAS shown above (ignore '-' and case):")),
        &dlg);
    inputLabel->setWordWrap(true);
    inputLabel->setTextFormat(Qt::PlainText);
    inputLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                  .arg(Theme::uiTextSub().name()));
    root->addWidget(inputLabel);

    const QString expected = NormalizeSas(sasShown);
    auto *inputEdit = inputField(QString(), &dlg);
    inputEdit->setPlaceholderText(UiSettings::Tr(QStringLiteral("输入安全码"),
                                                 QStringLiteral("Enter SAS")));
    maybeApplyMonoFont(inputEdit);
    root->addWidget(inputEdit);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    buttons->addStretch();
    auto *cancelBtn = outlineButton(UiSettings::Tr(QStringLiteral("稍后"), QStringLiteral("Later")), &dlg);
    auto *trustBtn = primaryButton(UiSettings::Tr(QStringLiteral("我已核对，信任"),
                                                  QStringLiteral("I verified it, trust")),
                                   &dlg);
    trustBtn->setDefault(true);
    trustBtn->setEnabled(false);
    QObject::connect(inputEdit, &QLineEdit::textChanged, &dlg, [trustBtn, expected](const QString &t) {
        trustBtn->setEnabled(!expected.isEmpty() && NormalizeSas(t) == expected);
    });
    QObject::connect(cancelBtn, &QAbstractButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(trustBtn, &QAbstractButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(inputEdit, &QLineEdit::returnPressed, &dlg, [trustBtn, &dlg]() {
        if (trustBtn->isEnabled()) {
            dlg.accept();
        }
    });

    buttons->addWidget(cancelBtn);
    buttons->addWidget(trustBtn);
    root->addLayout(buttons);

    inputEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    outSasInput = inputEdit->text();
    return true;
}
