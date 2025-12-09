#include "login_dialog.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

LoginDialog::LoginDialog(const UiPalette& palette, QWidget* parent)
    : QDialog(parent), palette_(palette) {
    setWindowTitle(tr("MI E2EE Client - 登录"));
    setModal(true);
    setFixedSize(420, 520);
    setSizeGripEnabled(false);
    setupPalette();
    setupUi();
    connectSignals();
}

void LoginDialog::setupPalette() {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        QStringLiteral("QDialog { background:%1; } QLabel { color:%2; }")
            .arg(palette_.background.name(), palette_.textPrimary.name()));
}

void LoginDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(14);

    layout->addStretch(1);

    auto* title = new QLabel(tr("QQ 登录"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("font-size:24px; font-weight:700; color:%1;")
            .arg(palette_.textPrimary.name()));
    layout->addWidget(title, 0, Qt::AlignHCenter);

    auto* avatar = new QLabel(this);
    avatar->setFixedSize(120, 120);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setPixmap(BuildAvatar(QStringLiteral("QQ"), palette_.accent, 110));
    layout->addWidget(avatar, 0, Qt::AlignHCenter);

    userBox_ = new QComboBox(this);
    userBox_->setEditable(true);
    userBox_->setPlaceholderText(tr("用户名"));
    userBox_->setMinimumHeight(38);
    userBox_->setFixedWidth(260);
    layout->addWidget(userBox_, 0, Qt::AlignHCenter);

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(tr("密码"));
    passwordEdit_->setMinimumHeight(38);
    passwordEdit_->setFixedWidth(260);
    layout->addWidget(passwordEdit_, 0, Qt::AlignHCenter);

    loginButton_ = new QPushButton(tr("登录"), this);
    loginButton_->setMinimumHeight(42);
    loginButton_->setFixedWidth(200);
    loginButton_->setCursor(Qt::PointingHandCursor);
    layout->addSpacing(6);
    layout->addWidget(loginButton_, 0, Qt::AlignHCenter);

    auto* linkRow = new QHBoxLayout();
    linkRow->setSpacing(16);
    auto createLink = [&](const QString& text) {
        auto* link = new QLabel(text, this);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(false);
        link->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; text-decoration: underline;")
                .arg(palette_.accent.name()));
        return link;
    };
    linkRow->addStretch(1);
    linkRow->addWidget(createLink(tr("添加账号")));
    linkRow->addWidget(createLink(tr("移除账号")));
    linkRow->addStretch(1);
    layout->addLayout(linkRow);

    layout->addStretch(1);
}

void LoginDialog::connectSignals() {
    connect(loginButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &QDialog::accept);
}

QString LoginDialog::username() const {
    return userBox_ ? userBox_->currentText() : QString();
}

void LoginDialog::setAccounts(const QStringList& users) {
    if (!userBox_) {
        return;
    }
    userBox_->clear();
    userBox_->addItems(users);
    if (!users.isEmpty()) {
        userBox_->setCurrentIndex(0);
    }
}

}  // namespace mi::client::ui::widgets
