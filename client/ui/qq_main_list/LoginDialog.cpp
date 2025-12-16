#include "LoginDialog.h"

#include <QEvent>
#include <QFrame>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPushButton>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "../common/IconButton.h"
#include "../common/SettingsDialog.h"
#include "../common/Theme.h"
#include "../common/UiSettings.h"
#include "../common/UiStyle.h"
#include "../common/Toast.h"
#include "BackendAdapter.h"

LoginDialog::LoginDialog(BackendAdapter *backend, QWidget *parent)
    : QDialog(parent), backend_(backend) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(320, 448);
    buildUi();
}

void LoginDialog::buildUi() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(0);

    auto *frame = new QFrame(this);
    frame->setObjectName("loginFrame");
    frame->setStyleSheet(
        "#loginFrame {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1A1733, stop:1 #2B1630);"
        "border-radius: 16px;"
        "}");
    auto *shadow = new QGraphicsDropShadowEffect(frame);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(0, 0, 0, 150));
    frame->setGraphicsEffect(shadow);
    outer->addWidget(frame);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    stack_ = new QStackedWidget(frame);
    layout->addWidget(stack_);

    // Top bar
    auto *titleBar = new QWidget(frame);
    titleBar->setFixedHeight(30);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setDirection(QBoxLayout::LeftToRight);
    titleLayout->addStretch();
    auto *settingBtnSimple = new IconButton(QString(), titleBar);
    settingBtnSimple->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/settings.svg"), 16);
    settingBtnSimple->setFixedSize(28, 28);
    settingBtnSimple->setColors(QColor("#C8C8D0"), QColor("#FFFFFF"), QColor("#D0D0D0"),
                                QColor(0, 0, 0, 0), QColor(255, 255, 255, 15),
                                QColor(255, 255, 255, 28));
    auto *settingsMenuSimple = new QMenu(settingBtnSimple);
    UiStyle::ApplyMenuStyle(*settingsMenuSimple);
    auto *settingsActionSimple = settingsMenuSimple->addAction(
        UiSettings::Tr(QStringLiteral("设置"), QStringLiteral("Settings")));
    settingsMenuSimple->addAction(
        UiSettings::Tr(QStringLiteral("帮助"), QStringLiteral("Help")));
    settingsMenuSimple->addAction(
        UiSettings::Tr(QStringLiteral("关于"), QStringLiteral("About")));
    settingBtnSimple->setMenu(settingsMenuSimple);
    settingBtnSimple->setStyleSheet("QToolButton { border-radius: 6px; }");
    connect(settingsActionSimple, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (backend_) {
            dlg.setClientConfigPath(backend_->configPath());
        }
        dlg.exec();
    });
    auto *closeBtnSimple = new IconButton(QString(), titleBar);
    closeBtnSimple->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 14);
    closeBtnSimple->setFixedSize(24, 24);
    closeBtnSimple->setColors(QColor("#C4C8D2"), QColor("#FFFFFF"), QColor("#FF6666"),
                              QColor(0, 0, 0, 0), QColor(255, 255, 255, 20), QColor(255, 255, 255, 30));
    connect(closeBtnSimple, &QPushButton::clicked, this, &LoginDialog::reject);
    titleLayout->addWidget(settingBtnSimple);
    titleLayout->addSpacing(6);
    titleLayout->addWidget(closeBtnSimple);
    // Simple page
    simplePage_ = new QWidget(frame);
    auto *simpleLayout = new QVBoxLayout(simplePage_);
    simpleLayout->setContentsMargins(26, 18, 26, 18);
    simpleLayout->setSpacing(12);
    simpleLayout->addWidget(titleBar);
    simpleLayout->addSpacing(6);

    auto *title = new QLabel(QStringLiteral("QQ"), simplePage_);
    title->setAlignment(Qt::AlignHCenter);
    title->setStyleSheet(
        "color: #6FC1FF; font-size: 30px; font-weight: 700; letter-spacing: 2px;");
    auto *titleGlow = new QGraphicsDropShadowEffect(title);
    titleGlow->setBlurRadius(24);
    titleGlow->setOffset(0, 0);
    titleGlow->setColor(QColor(111, 193, 255, 180));
    title->setGraphicsEffect(titleGlow);

    auto *avatar = new QLabel(simplePage_);
    avatar->setFixedSize(120, 120);
    avatar->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5FA0FF, stop:1 #7BB9FF);"
        "border: 3px solid rgba(255,255,255,0.9);"
        "border-radius: 60px;");
    auto *avatarShadow = new QGraphicsDropShadowEffect(avatar);
    avatarShadow->setBlurRadius(24);
    avatarShadow->setOffset(0, 6);
    avatarShadow->setColor(QColor(0, 0, 0, 120));
    avatar->setGraphicsEffect(avatarShadow);

    auto *nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignHCenter);
    auto *name = new QLabel(QStringLiteral("eds"), simplePage_);
    name->setStyleSheet("color: white; font-size: 16px; font-weight: 600;");
    auto *arrow = new QLabel(QStringLiteral("\u25BE"), simplePage_);
    arrow->setStyleSheet("color: #B7B9C5; font-size: 12px;");
    nameClick_ = new QWidget(simplePage_);
    auto *nameInner = new QHBoxLayout(nameClick_);
    nameInner->setContentsMargins(0, 0, 0, 0);
    nameInner->setSpacing(6);
    nameInner->addWidget(name);
    nameInner->addWidget(arrow);
    nameClick_->setCursor(Qt::PointingHandCursor);
    nameClick_->installEventFilter(this);
    nameLayout->addWidget(nameClick_);

    auto *loginBtn = new QPushButton(QStringLiteral("登录"), simplePage_);
    loginBtn->setFixedSize(260, 46);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        "QPushButton {"
        "  color: white;"
        "  background: #0B5ED7;"
        "  border: none;"
        "  border-radius: 9px;"
        "  font-size: 15px;"
        "}"
        "QPushButton:hover { background: #1D6FFF; }"
        "QPushButton:pressed { background: #094DB3; }");
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::handleLogin);

    auto *linksLayout = new QHBoxLayout();
    linksLayout->setAlignment(Qt::AlignHCenter);
    linksLayout->setSpacing(10);
    addLabel_ = new QLabel(QStringLiteral("添加账号"), simplePage_);
    addLabel_->setStyleSheet("color: #3B82F6; font-size: 12px;");
    addLabel_->setCursor(Qt::PointingHandCursor);
    addLabel_->installEventFilter(this);
    auto *divider = new QLabel(QStringLiteral("|"), simplePage_);
    divider->setStyleSheet("color: #4D78B3; font-size: 12px;");
    auto *removeLabel = new QLabel(QStringLiteral("移除账号"), simplePage_);
    removeLabel->setStyleSheet("color: #3B82F6; font-size: 12px;");
    removeLabel->setCursor(Qt::PointingHandCursor);
    linksLayout->addWidget(addLabel_);
    linksLayout->addWidget(divider);
    linksLayout->addWidget(removeLabel);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setAlignment(Qt::AlignHCenter);
    contentLayout->setSpacing(14);
    contentLayout->addWidget(title);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(avatar, 0, Qt::AlignHCenter);
    contentLayout->addLayout(nameLayout);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(loginBtn, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(10);
    contentLayout->addLayout(linksLayout);

    simpleLayout->addLayout(contentLayout);
    simpleLayout->addStretch();
    errorLabel_ = new QLabel(simplePage_);
    errorLabel_->setStyleSheet("color: #E96A6A; font-size: 11px;");
    errorLabel_->setVisible(false);
    simpleLayout->addWidget(errorLabel_);

    // Account page
    accountPage_ = new QWidget(frame);
    auto *accLayout = new QVBoxLayout(accountPage_);
    accLayout->setContentsMargins(20, 16, 20, 16);
    accLayout->setSpacing(10);
    auto *accTopBar = new QWidget(accountPage_);
    auto *accTopLayout = new QHBoxLayout(accTopBar);
    accTopLayout->setContentsMargins(0, 0, 0, 0);
    accTopLayout->setDirection(QBoxLayout::LeftToRight);
    accTopLayout->addStretch();
    auto *settingBtnAcc = new IconButton(QString(), accTopBar);
    settingBtnAcc->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/settings.svg"), 16);
    settingBtnAcc->setFixedSize(28, 28);
    settingBtnAcc->setColors(QColor("#C8C8D0"), QColor("#FFFFFF"), QColor("#D0D0D0"),
                             QColor(0, 0, 0, 0), QColor(255, 255, 255, 15),
                             QColor(255, 255, 255, 28));
    auto *settingsMenuAcc = new QMenu(settingBtnAcc);
    UiStyle::ApplyMenuStyle(*settingsMenuAcc);
    auto *settingsActionAcc = settingsMenuAcc->addAction(
        UiSettings::Tr(QStringLiteral("设置"), QStringLiteral("Settings")));
    settingsMenuAcc->addAction(
        UiSettings::Tr(QStringLiteral("帮助"), QStringLiteral("Help")));
    settingsMenuAcc->addAction(
        UiSettings::Tr(QStringLiteral("关于"), QStringLiteral("About")));
    settingBtnAcc->setMenu(settingsMenuAcc);
    settingBtnAcc->setStyleSheet("QToolButton { border-radius: 6px; }");
    connect(settingsActionAcc, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (backend_) {
            dlg.setClientConfigPath(backend_->configPath());
        }
        dlg.exec();
    });
    accTopLayout->addWidget(settingBtnAcc);
    accTopLayout->addSpacing(6);
    auto *closeBtnAcc = new IconButton(QString(), accTopBar);
    closeBtnAcc->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 14);
    closeBtnAcc->setFixedSize(24, 24);
    closeBtnAcc->setColors(QColor("#C4C8D2"), QColor("#FFFFFF"), QColor("#FF6666"),
                           QColor(0, 0, 0, 0), QColor(255, 255, 255, 20), QColor(255, 255, 255, 30));
    connect(closeBtnAcc, &QPushButton::clicked, this, &LoginDialog::reject);
    accTopLayout->addWidget(closeBtnAcc);
    accLayout->addWidget(accTopBar);

    auto *accAvatar = new QLabel(accountPage_);
    accAvatar->setFixedSize(90, 90);
    accAvatar->setStyleSheet(
        "background: #f0f0f0;"
        "border: 2px solid rgba(255,255,255,0.9);"
        "border-radius: 45px;");
    accLayout->addWidget(accAvatar, 0, Qt::AlignHCenter);

    accountBox_ = new QComboBox(accountPage_);
    accountBox_->setEditable(true);
    accountBox_->addItem(QStringLiteral("3960562879"));
    accountBox_->setStyleSheet(
        "QComboBox { background: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.10); "
        "border-radius: 10px; padding: 10px 36px 10px 12px; color: #FFFFFF; font-size: 14px; }"
        "QComboBox::drop-down { width: 28px; border: none; }"
        "QComboBox::down-arrow { image: none; }"
        "QComboBox QAbstractItemView { background: #1E1E1E; color: #FFFFFF; selection-background-color: #2A2D33; }");
    accLayout->addWidget(accountBox_);

    passwordAccount_ = new QLineEdit(accountPage_);
    passwordAccount_->setPlaceholderText(QStringLiteral("输入QQ密码"));
    passwordAccount_->setEchoMode(QLineEdit::Password);
    passwordAccount_->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.10); border: 1px solid rgba(255,255,255,0.10); "
        "border-radius: 10px; padding: 10px 12px; color: #FFFFFF; font-size: 14px; }"
        "QLineEdit:placeholder { color: #8B8FA0; }"
        "QLineEdit:focus { border-color: #3B82F6; }");
    accLayout->addWidget(passwordAccount_);

    auto *agreeRow = new QHBoxLayout();
    agreeRow->setContentsMargins(0, 0, 0, 0);
    agreeRow->setSpacing(6);
    agreeRow->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    agreeCheck_ = new QCheckBox(accountPage_);
    agreeCheck_->setStyleSheet(
        "QCheckBox { color: #FFFFFF; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QCheckBox::indicator:checked { image: none; border: 1px solid #3B82F6; background: #3B82F6; }"
        "QCheckBox::indicator:unchecked { image: none; border: 1px solid rgba(255,255,255,0.3); background: transparent; }");
    agreeRow->addWidget(agreeCheck_, 0, Qt::AlignTop);
    auto *agreeLabel =
        new QLabel(QStringLiteral("已阅读并同意 <a href=\"#\">服务协议</a> 和 <a href=\"#\">QQ隐私保护指引</a>"),
                   accountPage_);
    agreeLabel->setTextFormat(Qt::RichText);
    agreeLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    agreeLabel->setOpenExternalLinks(false);
    agreeLabel->setStyleSheet(
        "QLabel { color: #FFFFFF; font-size: 12px; } "
        "QLabel:hover { color: #FFFFFF; } "
        "QLabel a { color: #3B82F6; text-decoration: none; } "
        "QLabel a:hover { color: #5A9BFF; }");
    agreeLabel->setWordWrap(false);
    agreeRow->addWidget(agreeLabel, 1);
    accLayout->addLayout(agreeRow);

    accountLoginBtn_ = new QPushButton(QStringLiteral("登录"), accountPage_);
    accountLoginBtn_->setFixedHeight(46);
    accountLoginBtn_->setEnabled(false);
    accountLoginBtn_->setStyleSheet(
        "QPushButton { color: white; background: #0B5ED7; border: none; border-radius: 10px; font-size: 15px; }"
        "QPushButton:disabled { background: #22324A; color: #9FA5B2; }"
        "QPushButton:hover:enabled { background: #1D6FFF; }"
        "QPushButton:pressed:enabled { background: #094DB3; }");
    accLayout->addWidget(accountLoginBtn_);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);
    bottomRow->setSpacing(12);
    auto *scan = new QLabel(QStringLiteral("扫码登录"), accountPage_);
    scan->setStyleSheet("color: #3B82F6; font-size: 12px;");
    auto *more = new QLabel(QStringLiteral("更多选项"), accountPage_);
    more->setStyleSheet("color: #3B82F6; font-size: 12px;");
    bottomRow->addStretch();
    bottomRow->addWidget(scan);
    bottomRow->addSpacing(8);
    bottomRow->addWidget(more);
    bottomRow->addStretch();
    accLayout->addLayout(bottomRow);

    stack_->addWidget(simplePage_);
    stack_->addWidget(accountPage_);
    stack_->setCurrentWidget(simplePage_);

    connect(accountBox_->lineEdit(), &QLineEdit::textChanged, this, &LoginDialog::updateLoginEnabled);
    connect(passwordAccount_, &QLineEdit::textChanged, this, &LoginDialog::updateLoginEnabled);
    connect(agreeCheck_, &QCheckBox::toggled, this, &LoginDialog::updateLoginEnabled);
    connect(accountLoginBtn_, &QPushButton::clicked, this, &LoginDialog::handleLogin);
}

void LoginDialog::handleLogin() {
    const QString acc = accountBox_ ? accountBox_->currentText().trimmed() : QString();
    const QString pwd = passwordAccount_ ? passwordAccount_->text() : QString();
    if (stack_->currentWidget() == simplePage_ && (acc.isEmpty() || pwd.isEmpty())) {
        switchToAccountPage();
        errorLabel_->setText(QStringLiteral("请输入账号和密码"));
        errorLabel_->setVisible(true);
        return;
    }

    if (stack_->currentWidget() == accountPage_) {
        if (!accountLoginBtn_->isEnabled()) {
            errorLabel_->setText(QStringLiteral("请填写账号/密码并勾选协议"));
            errorLabel_->setVisible(true);
            return;
        }
        if (backend_) {
            QString err;
            if (!backend_->login(acc, pwd, err)) {
                if (handlePendingServerTrust(acc, pwd)) {
                    return;
                }
                errorLabel_->setText(err.isEmpty() ? QStringLiteral("登录失败") : err);
                errorLabel_->setVisible(true);
                Toast::Show(this,
                            err.isEmpty()
                                ? UiSettings::Tr(QStringLiteral("登录失败：请检查账号或网络"),
                                                 QStringLiteral("Login failed. Please check your account or network."))
                                : UiSettings::Tr(QStringLiteral("登录失败：%1").arg(err),
                                                 QStringLiteral("Login failed: %1").arg(err)),
                            Toast::Level::Error,
                            3200);
                return;
            }
        }
        errorLabel_->setVisible(false);
        emit addAccountRequested();
        accept();
    } else {
        // 简化态下若已有账号密码则尝试登录，否则切换到账号输入
        if (backend_) {
            QString err;
            if (!backend_->login(acc, pwd, err)) {
                if (handlePendingServerTrust(acc, pwd)) {
                    return;
                }
                errorLabel_->setText(err.isEmpty() ? QStringLiteral("登录失败") : err);
                errorLabel_->setVisible(true);
                Toast::Show(this,
                            err.isEmpty()
                                ? UiSettings::Tr(QStringLiteral("登录失败：请检查账号或网络"),
                                                 QStringLiteral("Login failed. Please check your account or network."))
                                : UiSettings::Tr(QStringLiteral("登录失败：%1").arg(err),
                                                 QStringLiteral("Login failed: %1").arg(err)),
                            Toast::Level::Error,
                            3200);
                return;
            }
        }
        errorLabel_->setVisible(false);
        accept();
    }
}

bool LoginDialog::handlePendingServerTrust(const QString &account, const QString &password) {
    if (!backend_ || !backend_->hasPendingServerTrust()) {
        return false;
    }

    const QString fingerprintHex = backend_->pendingServerFingerprint();
    const QString pin = backend_->pendingServerPin();

    const QString detail = UiSettings::Tr(
        QStringLiteral(
            "检测到需要验证服务器身份（首次连接或证书指纹变更）。\n\n"
            "指纹：%1\n"
            "安全码（SAS）：%2\n\n"
            "请通过线下可信渠道核对安全码/指纹后再继续。")
            .arg(fingerprintHex, pin),
        QStringLiteral(
            "Server identity verification required (first connection or certificate pin changed).\n\n"
            "Fingerprint: %1\n"
            "SAS: %2\n\n"
            "Verify via an out-of-band channel before trusting.")
            .arg(fingerprintHex, pin));

    QMessageBox box(QMessageBox::Warning,
                    UiSettings::Tr(QStringLiteral("验证服务器身份"),
                                   QStringLiteral("Verify server identity")),
                    detail,
                    QMessageBox::NoButton, this);
    auto *trustBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("我已核对，信任"),
                                     QStringLiteral("I verified it, trust")),
                      QMessageBox::AcceptRole);
    box.addButton(UiSettings::Tr(QStringLiteral("稍后"), QStringLiteral("Later")),
                  QMessageBox::RejectRole);
    box.setDefaultButton(trustBtn);
    box.exec();

    if (box.clickedButton() != trustBtn) {
        errorLabel_->setText(QStringLiteral("需要先信任服务器（TLS）"));
        errorLabel_->setVisible(true);
        return true;
    }

    bool ok = false;
    const QString input = QInputDialog::getText(
        this,
        UiSettings::Tr(QStringLiteral("输入安全码"),
                       QStringLiteral("Enter SAS")),
        UiSettings::Tr(QStringLiteral("请输入上面显示的安全码（可包含 '-'，忽略大小写）："),
                       QStringLiteral("Enter the SAS shown above (ignore '-' and case):")),
        QLineEdit::Normal, pin, &ok);
    if (!ok) {
        errorLabel_->setText(QStringLiteral("需要先信任服务器（TLS）"));
        errorLabel_->setVisible(true);
        return true;
    }

    QString trustErr;
    if (!backend_->trustPendingServer(input, trustErr)) {
        QMessageBox::warning(
            this,
            UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed")),
            trustErr.isEmpty()
                ? UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed"))
                : trustErr);
        errorLabel_->setText(trustErr.isEmpty() ? QStringLiteral("信任失败") : trustErr);
        errorLabel_->setVisible(true);
        return true;
    }

    QString err;
    if (!backend_->login(account, password, err)) {
        errorLabel_->setText(err.isEmpty() ? QStringLiteral("登录失败") : err);
        errorLabel_->setVisible(true);
        Toast::Show(
            this,
            err.isEmpty()
                ? UiSettings::Tr(QStringLiteral("登录失败：请检查账号或网络"),
                                 QStringLiteral("Login failed. Please check your account or network."))
                : UiSettings::Tr(QStringLiteral("登录失败：%1").arg(err),
                                 QStringLiteral("Login failed: %1").arg(err)),
            Toast::Level::Error,
            3200);
        return true;
    }

    errorLabel_->setVisible(false);
    if (stack_ && stack_->currentWidget() == accountPage_) {
        emit addAccountRequested();
    }
    accept();
    return true;
}

void LoginDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragPos_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QDialog::mousePressEvent(event);
}

void LoginDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPos_);
    }
    QDialog::mouseMoveEvent(event);
}

void LoginDialog::toggleInputs() {
    // In simple mode, toggle to account page.
    if (stack_->currentWidget() == simplePage_) {
        switchToAccountPage();
    } else {
        switchToSimplePage();
    }
}

void LoginDialog::switchToAccountPage() {
    stack_->setCurrentWidget(accountPage_);
    updateLoginEnabled();
}

void LoginDialog::switchToSimplePage() {
    stack_->setCurrentWidget(simplePage_);
    errorLabel_->setVisible(false);
}

void LoginDialog::updateLoginEnabled() {
    const bool ok = agreeCheck_->isChecked() &&
                    !accountBox_->currentText().trimmed().isEmpty() &&
                    !passwordAccount_->text().isEmpty();
    accountLoginBtn_->setEnabled(ok);
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == nameClick_ && event->type() == QEvent::MouseButtonPress) {
        toggleInputs();
        return true;
    }
    if (watched == addLabel_ && event->type() == QEvent::MouseButtonPress) {
        switchToAccountPage();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}
