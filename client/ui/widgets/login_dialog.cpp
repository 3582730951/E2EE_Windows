#include "login_dialog.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

LoginDialog::LoginDialog(const UiPalette& palette, QWidget* parent)
    : QDialog(parent), palette_(palette) {
    setWindowTitle(tr("MI E2EE Client"));
    setModal(true);
    setFixedSize(420, 460);
    setSizeGripEnabled(false);
    setupPalette();
    setupUi();
    connectSignals();
}

void LoginDialog::setupPalette() {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
        "QLabel { color:%2; }")
                      .arg(palette_.background.name(), palette_.textPrimary.name()));
}

void LoginDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel(tr("账户"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:22px; font-weight:700; color:%1;")
                             .arg(palette_.textPrimary.name()));
    layout->addWidget(title, 0, Qt::AlignHCenter);

    userBox_ = new QComboBox(this);
    userBox_->setEditable(true);
    userBox_->setPlaceholderText(tr("用户名"));
    userBox_->setMinimumHeight(36);
    userBox_->setFixedWidth(240);
    layout->addWidget(userBox_, 0, Qt::AlignHCenter);

    auto* pwdLabel = new QLabel(tr("密码"), this);
    layout->addWidget(pwdLabel, 0, Qt::AlignHCenter);

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(tr("密码"));
    passwordEdit_->setMinimumHeight(36);
    passwordEdit_->setFixedWidth(240);
    layout->addWidget(passwordEdit_, 0, Qt::AlignHCenter);

    loginButton_ = new QPushButton(tr("登录"), this);
    loginButton_->setMinimumHeight(40);
    loginButton_->setFixedWidth(160);
    loginButton_->setCursor(Qt::PointingHandCursor);
    layout->addSpacing(12);
    layout->addWidget(loginButton_, 0, Qt::AlignHCenter);
}

void LoginDialog::connectSignals() {
    connect(loginButton_, &QPushButton::clicked, this, &QDialog::accept);
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
