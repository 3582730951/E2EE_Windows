#include "login_dialog.h"

#include <QHBoxLayout>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

LoginDialog::LoginDialog(const UiPalette& palette, QWidget* parent)
    : QDialog(parent), palette_(palette) {
    setWindowTitle(tr("登录"));
    setModal(true);
    setFixedSize(420, 520);
    setSizeGripEnabled(false);
    setupPalette();
    setupUi();
    connectSignals();
}

void LoginDialog::setupPalette() {
    setAttribute(Qt::WA_StyledBackground, true);
}

void LoginDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(18);

    layout->addStretch(1);

    auto* title = new QLabel(tr("QQ"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:26px; font-weight:700; color:%1;")
                             .arg(palette_.textPrimary.name()));
    layout->addWidget(title);

    avatarLabel_ = new QLabel(this);
    avatarLabel_->setFixedSize(96, 96);
    avatarLabel_->setPixmap(BuildAvatar(QStringLiteral("QQ"), palette_.accent, 96));
    avatarLabel_->setScaledContents(true);
    avatarLabel_->setAlignment(Qt::AlignCenter);

    auto* avatarWrap = new QWidget(this);
    avatarWrap->setFixedHeight(110);
    auto* avatarLayout = new QHBoxLayout(avatarWrap);
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    avatarLayout->addStretch(1);
    avatarLayout->addWidget(avatarLabel_);
    avatarLayout->addStretch(1);
    layout->addWidget(avatarWrap);

    userBox_ = new QComboBox(this);
    userBox_->setEditable(true);
    userBox_->setPlaceholderText(tr("选择或输入用户名"));
    userBox_->setMinimumHeight(42);
    userBox_->addItems({QStringLiteral("demo_user"), QStringLiteral("secure_guest")});
    layout->addWidget(userBox_);

    loginButton_ = new QPushButton(tr("登录"), this);
    loginButton_->setMinimumHeight(44);
    loginButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(loginButton_);

    auto* linkRow = new QHBoxLayout();
    linkRow->setSpacing(16);

    addAccountLink_ = new QPushButton(tr("添加账号"), this);
    addAccountLink_->setFlat(true);
    addAccountLink_->setCursor(Qt::PointingHandCursor);
    addAccountLink_->setStyleSheet(QStringLiteral(
        "QPushButton { color:#4da6ff; background:transparent; border:none; font-size:12px; }"
        "QPushButton:hover { color:%1; }")
                                       .arg(palette_.accentHover.name()));
    linkRow->addWidget(addAccountLink_, 0, Qt::AlignCenter);

    removeAccountLink_ = new QPushButton(tr("移除账号"), this);
    removeAccountLink_->setFlat(true);
    removeAccountLink_->setCursor(Qt::PointingHandCursor);
    removeAccountLink_->setStyleSheet(QStringLiteral(
        "QPushButton { color:#4da6ff; background:transparent; border:none; font-size:12px; }"
        "QPushButton:hover { color:%1; }")
                                          .arg(palette_.accentHover.name()));
    linkRow->addWidget(removeAccountLink_, 0, Qt::AlignCenter);

    auto* linkWidget = new QWidget(this);
    linkWidget->setLayout(linkRow);
    layout->addWidget(linkWidget, 0, Qt::AlignCenter);

    layout->addStretch(1);
}

void LoginDialog::connectSignals() {
    connect(loginButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(addAccountLink_, &QPushButton::clicked, this, &LoginDialog::addAccountRequested);
    connect(removeAccountLink_, &QPushButton::clicked, this, [this]() {
        emit removeAccountRequested(userBox_->currentText());
    });
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
