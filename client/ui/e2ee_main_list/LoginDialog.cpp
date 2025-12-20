#include "LoginDialog.h"

#include <QEvent>
#include <QFrame>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPushButton>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QEasingCurve>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "../common/IconButton.h"
#include "../common/SettingsDialog.h"
#include "../common/Theme.h"
#include "../common/UiSettings.h"
#include "../common/UiStyle.h"
#include "../common/Toast.h"
#include "BackendAdapter.h"
#include "TrustPromptDialog.h"

LoginDialog::LoginDialog(BackendAdapter *backend, QWidget *parent)
    : QDialog(parent), backend_(backend) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(380, 560);
    setMinimumSize(340, 480);
    buildUi();
    if (backend_) {
        connect(backend_, &BackendAdapter::loginFinished, this, &LoginDialog::onLoginFinished);
        connect(backend_, &BackendAdapter::registerFinished, this, &LoginDialog::onRegisterFinished);
    }
}

void LoginDialog::buildUi() {
    const bool lightScheme = (Theme::scheme() == Theme::Scheme::Light);
    const QColor frameTop = Theme::uiPanelBg().lighter(lightScheme ? 102 : 108);
    const QColor frameBottom = Theme::uiPanelBg().darker(lightScheme ? 102 : 94);
    const QColor border = Theme::uiBorder();
    const QColor accent = Theme::uiAccentBlue();
    const QColor accentHover = accent.lighter(110);
    const QColor accentPressed = accent.darker(110);
    const QColor danger = Theme::uiDangerRed();
    const QColor textMain = Theme::uiTextMain();
    const QColor textSub = Theme::uiTextSub();
    const QColor textMuted = Theme::uiTextMuted();
    const QColor disabledBg = Theme::uiBadgeGrey();
    const QColor hoverBg = Theme::uiHoverBg();
    const QColor selectedBg = Theme::uiSelectedBg();
    const QColor inputBg = Theme::uiInputBg();
    const QColor inputBorder = Theme::uiInputBorder();

    auto *outer = new QVBoxLayout(this);
    const int shadowBlur = 0;
    const int shadowOffsetY = 0;
    const int shadowPad = 0;
    outer->setContentsMargins(shadowBlur + shadowPad,
                              qMax(0, shadowBlur - shadowOffsetY) + shadowPad,
                              shadowBlur + shadowPad,
                              shadowBlur + shadowOffsetY + shadowPad);
    outer->setSpacing(0);

    auto *frame = new QFrame(this);
    frame_ = frame;
    frame->setObjectName("loginFrame");
    frame->setStyleSheet(
        QStringLiteral(
            "#loginFrame {"
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
            "border: 1px solid %3;"
            "border-radius: 20px;"
             "}")
             .arg(frameTop.name(), frameBottom.name(), border.name()));
    outer->addWidget(frame);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    stack_ = new QStackedWidget(frame);
    layout->addWidget(stack_);

    errorLabel_ = new QLabel(frame);
    errorLabel_->setTextFormat(Qt::PlainText);
    errorLabel_->setWordWrap(true);
    errorLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    errorLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; padding: 0 20px 12px 20px;")
            .arg(danger.name()));
    errorLabel_->setVisible(false);
    layout->addWidget(errorLabel_);

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
    settingBtnSimple->setPadding(4);
    settingBtnSimple->setFocusPolicy(Qt::NoFocus);
    settingBtnSimple->setColors(textSub, textMain, textMain, QColor(0, 0, 0, 0), hoverBg, selectedBg);
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
    closeBtnSimple->setColors(textSub, textMain, danger, QColor(0, 0, 0, 0), hoverBg, selectedBg);
    connect(closeBtnSimple, &QPushButton::clicked, this, &LoginDialog::reject);
    titleLayout->addWidget(settingBtnSimple);
    titleLayout->addSpacing(6);
    titleLayout->addWidget(closeBtnSimple);
    // Simple page
    simplePage_ = new QWidget(frame);
    auto *simpleLayout = new QVBoxLayout(simplePage_);
    simpleLayout->setContentsMargins(22, 16, 22, 16);
    simpleLayout->setSpacing(12);
    simpleLayout->addWidget(titleBar);
    simpleLayout->addSpacing(6);

    auto *title = new QLabel(QStringLiteral("E2EE"), simplePage_);
    title->setAlignment(Qt::AlignHCenter);
    title->setFont(Theme::defaultFont(30, QFont::Bold));
    title->setStyleSheet(QStringLiteral("color: %1; letter-spacing: 2px;")
                             .arg(accent.name()));

    auto *avatar = new QLabel(simplePage_);
    avatar->setFixedSize(120, 120);
    avatar->setStyleSheet(
        QStringLiteral(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);"
            "border: 2px solid %3;"
            "border-radius: 60px;")
            .arg(accent.lighter(118).name(),
                 accent.darker(105).name(),
                 border.name()));
    auto *nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignHCenter);
    auto *name = new QLabel(QStringLiteral("E2EE"), simplePage_);
    name->setFont(Theme::defaultFont(16, QFont::DemiBold));
    name->setStyleSheet(QStringLiteral("color: %1;").arg(textMain.name()));
    auto *arrow = new QLabel(QStringLiteral("\u25BE"), simplePage_);
    arrow->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(textMuted.name()));
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
    loginBtn->setFixedHeight(46);
    loginBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    loginBtn->setMaximumWidth(260);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  color: white;"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 16px;"
            "  font-size: 15px;"
            "}"
            "QPushButton:hover { background: %2; }"
            "QPushButton:pressed { background: %3; }")
             .arg(accent.name(), accentHover.name(), accentPressed.name()));
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::handleLogin);
    simpleLoginBtn_ = loginBtn;

    auto *linksLayout = new QHBoxLayout();
    linksLayout->setAlignment(Qt::AlignHCenter);
    linksLayout->setSpacing(10);
    addLabel_ = new QLabel(QStringLiteral("添加账号"), simplePage_);
    addLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(accent.name()));
    addLabel_->setCursor(Qt::PointingHandCursor);
    addLabel_->installEventFilter(this);
    auto *divider = new QLabel(QStringLiteral("|"), simplePage_);
    divider->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(textMuted.name()));
    auto *removeLabel = new QLabel(QStringLiteral("移除账号"), simplePage_);
    removeLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(accent.name()));
    removeLabel->setCursor(Qt::PointingHandCursor);
    linksLayout->addWidget(addLabel_);
    linksLayout->addWidget(divider);
    linksLayout->addWidget(removeLabel);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(14);
    contentLayout->addWidget(title);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(avatar, 0, Qt::AlignHCenter);
    contentLayout->addLayout(nameLayout);
    contentLayout->addSpacing(12);
    auto *loginRow = new QHBoxLayout();
    loginRow->setContentsMargins(0, 0, 0, 0);
    loginRow->addStretch();
    loginRow->addWidget(loginBtn);
    loginRow->addStretch();
    contentLayout->addLayout(loginRow);
    contentLayout->addSpacing(10);
    contentLayout->addLayout(linksLayout);

    simpleLayout->addLayout(contentLayout);
    simpleLayout->addStretch();

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
    settingBtnAcc->setPadding(4);
    settingBtnAcc->setFocusPolicy(Qt::NoFocus);
    settingBtnAcc->setColors(textSub, textMain, textMain, QColor(0, 0, 0, 0), hoverBg, selectedBg);
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
    closeBtnAcc->setColors(textSub, textMain, danger, QColor(0, 0, 0, 0), hoverBg, selectedBg);
    connect(closeBtnAcc, &QPushButton::clicked, this, &LoginDialog::reject);
    accTopLayout->addWidget(closeBtnAcc);
    accLayout->addWidget(accTopBar);

    auto *accAvatar = new QLabel(accountPage_);
    accAvatar->setFixedSize(90, 90);
    accAvatar->setStyleSheet(
        QStringLiteral(
            "background: %1;"
            "border: 1px solid %2;"
            "border-radius: 45px;")
            .arg(Theme::uiSearchBg().name(),
                 border.name()));
    accLayout->addWidget(accAvatar, 0, Qt::AlignHCenter);

    accountBox_ = new QComboBox(accountPage_);
    accountBox_->setEditable(true);
    if (auto *edit = accountBox_->lineEdit()) {
        edit->setPlaceholderText(UiSettings::Tr(QStringLiteral("输入账号"),
                                                QStringLiteral("Enter account")));
    }
    accountBox_->setStyleSheet(
        QStringLiteral(
            "QComboBox { background: %1; border: 1px solid %2; "
            "border-radius: 14px; padding: 12px 36px 12px 12px; color: %3; font-size: 14px; }"
            "QComboBox:focus { border-color: %4; }"
            "QComboBox::drop-down { width: 28px; border: none; }"
            "QComboBox::down-arrow { image: none; }"
            "QComboBox QAbstractItemView { background: %5; color: %3; selection-background-color: %6; }")
            .arg(inputBg.name(),
                 inputBorder.name(),
                 textMain.name(),
                 accent.name(),
                 Theme::uiMenuBg().name(),
                 selectedBg.name()));
    accLayout->addWidget(accountBox_);

    passwordAccount_ = new QLineEdit(accountPage_);
    passwordAccount_->setPlaceholderText(UiSettings::Tr(QStringLiteral("输入密码"),
                                                        QStringLiteral("Enter password")));
    passwordAccount_->setEchoMode(QLineEdit::Password);
    passwordAccount_->setStyleSheet(
        QStringLiteral(
            "QLineEdit { background: %1; border: 1px solid %2; "
            "border-radius: 14px; padding: 12px 12px; color: %3; font-size: 14px; }"
            "QLineEdit:placeholder { color: %4; }"
            "QLineEdit:focus { border-color: %5; }")
            .arg(inputBg.name(),
                 inputBorder.name(),
                 textMain.name(),
                 textMuted.name(),
                 accent.name()));
    accLayout->addWidget(passwordAccount_);

    auto *agreeRow = new QHBoxLayout();
    agreeRow->setContentsMargins(0, 0, 0, 0);
    agreeRow->setSpacing(6);
    agreeRow->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    agreeCheck_ = new QCheckBox(accountPage_);
    agreeCheck_->setStyleSheet(
        QStringLiteral("QCheckBox { color: %1; }").arg(textSub.name()) +
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; }"
        +
        QStringLiteral("QCheckBox::indicator:checked { image: url(:/mi/e2ee/ui/icons/check.svg); "
                       "border: 1px solid %1; background: %1; }")
            .arg(accent.name()) +
        QStringLiteral("QCheckBox::indicator:unchecked { image: none; border: 1px solid %1; background: transparent; }")
            .arg(inputBorder.name()));
    agreeRow->addWidget(agreeCheck_, 0, Qt::AlignTop);
    auto *agreeLabel =
        new QLabel(QStringLiteral("已阅读并同意 <a href=\"#\">服务协议</a> 和 <a href=\"#\">E2EE隐私保护指引</a>"),
                   accountPage_);
    agreeLabel->setTextFormat(Qt::RichText);
    agreeLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    agreeLabel->setOpenExternalLinks(false);
    agreeLabel->setStyleSheet(
        QStringLiteral(
            "QLabel { color: %1; font-size: 12px; } "
            "QLabel a { color: %2; text-decoration: none; } "
            "QLabel a:hover { color: %3; }")
            .arg(textSub.name(),
                 accent.name(),
                 accentHover.name()));
    agreeLabel->setWordWrap(true);
    agreeRow->addWidget(agreeLabel, 1);
    accLayout->addLayout(agreeRow);

    accountLoginBtn_ = new QPushButton(QStringLiteral("登录"), accountPage_);
    accountLoginBtn_->setFixedHeight(46);
    accountLoginBtn_->setEnabled(false);
    accountLoginBtn_->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: white; background: %1; border: none; border-radius: 16px; font-size: 15px; }"
            "QPushButton:disabled { background: %2; color: %3; }"
            "QPushButton:hover:enabled { background: %4; }"
            "QPushButton:pressed:enabled { background: %5; }")
            .arg(accent.name(),
                 disabledBg.name(),
                 textMuted.name(),
                 accentHover.name(),
                 accentPressed.name()));
    accLayout->addWidget(accountLoginBtn_);

    auto *registerRow = new QHBoxLayout();
    registerRow->setContentsMargins(0, 0, 0, 0);
    registerRow->setAlignment(Qt::AlignHCenter);
    auto *registerLabel = new QLabel(QStringLiteral("<a href=\"#\">注册账号</a>"), accountPage_);
    registerLabel->setTextFormat(Qt::RichText);
    registerLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    registerLabel->setOpenExternalLinks(false);
    registerLabel->setStyleSheet(
        QStringLiteral(
            "QLabel { color: %1; font-size: 12px; } "
            "QLabel a { color: %2; text-decoration: none; } "
            "QLabel a:hover { color: %3; }")
            .arg(textSub.name(),
                 accent.name(),
                 accentHover.name()));
    connect(registerLabel, &QLabel::linkActivated, this, [this](const QString &) { handleRegister(); });
    registerRow->addWidget(registerLabel);
    accLayout->addLayout(registerRow);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);
    bottomRow->setSpacing(12);
    auto *scan = new QLabel(QStringLiteral("扫码登录"), accountPage_);
    scan->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(accent.name()));
    auto *more = new QLabel(QStringLiteral("更多选项"), accountPage_);
    more->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(accent.name()));
    bottomRow->addStretch();
    bottomRow->addWidget(scan);
    bottomRow->addSpacing(8);
    bottomRow->addWidget(more);
    bottomRow->addStretch();
    accLayout->addLayout(bottomRow);

    stack_->addWidget(simplePage_);
    stack_->addWidget(accountPage_);
    stack_->setCurrentWidget(accountPage_);

    connect(accountBox_->lineEdit(), &QLineEdit::textChanged, this, &LoginDialog::updateLoginEnabled);
    connect(passwordAccount_, &QLineEdit::textChanged, this, &LoginDialog::updateLoginEnabled);
    connect(agreeCheck_, &QCheckBox::toggled, this, &LoginDialog::updateLoginEnabled);
    connect(accountLoginBtn_, &QPushButton::clicked, this, &LoginDialog::handleLogin);
}

void LoginDialog::setLoginBusy(bool busy) {
    loginBusy_ = busy;
    const QString text = busy ? QStringLiteral("登录中…") : QStringLiteral("登录");
    if (simpleLoginBtn_) {
        simpleLoginBtn_->setEnabled(!busy);
        simpleLoginBtn_->setText(text);
    }
    if (accountLoginBtn_) {
        accountLoginBtn_->setText(text);
        if (busy) {
            accountLoginBtn_->setEnabled(false);
        }
    }
    if (accountBox_) {
        accountBox_->setEnabled(!busy);
    }
    if (passwordAccount_) {
        passwordAccount_->setEnabled(!busy);
    }
    if (agreeCheck_) {
        agreeCheck_->setEnabled(!busy);
    }
    if (!busy) {
        updateLoginEnabled();
    }
}

void LoginDialog::handleLogin() {
    if (loginBusy_) {
        return;
    }
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
        if (!backend_) {
            errorLabel_->setText(QStringLiteral("后端未就绪"));
            errorLabel_->setVisible(true);
            return;
        }
        errorLabel_->setVisible(false);
        setLoginBusy(true);
        backend_->loginAsync(acc, pwd);
    } else {
        // 简化态下若已有账号密码则尝试登录，否则切换到账号输入
        if (!backend_) {
            errorLabel_->setText(QStringLiteral("后端未就绪"));
            errorLabel_->setVisible(true);
            return;
        }
        errorLabel_->setVisible(false);
        setLoginBusy(true);
        backend_->loginAsync(acc, pwd);
    }
}

void LoginDialog::onLoginFinished(bool success, const QString &error) {
    setLoginBusy(false);

    const QString acc = accountBox_ ? accountBox_->currentText().trimmed() : QString();
    const QString pwd = passwordAccount_ ? passwordAccount_->text() : QString();

    if (!success) {
        if (handlePendingServerTrust(acc, pwd)) {
            return;
        }
        QString err = error.trimmed();
        if (err.compare(QStringLiteral("invalid credentials"), Qt::CaseInsensitive) == 0 ||
            err.compare(QStringLiteral("client login finish failed"), Qt::CaseInsensitive) == 0 ||
            err.compare(QStringLiteral("opaque login finish failed"), Qt::CaseInsensitive) == 0) {
            err = UiSettings::Tr(QStringLiteral("账号不存在或密码错误，可先点击“注册账号”创建。"),
                                 QStringLiteral("Invalid credentials. You may need to register first."));
        }
        errorLabel_->setText(QString());
        errorLabel_->setVisible(false);
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

    errorLabel_->setVisible(false);
    if (stack_ && stack_->currentWidget() == accountPage_) {
        emit addAccountRequested();
    }
    accept();
}

void LoginDialog::handleRegister() {
    if (loginBusy_) {
        return;
    }
    if (!backend_) {
        errorLabel_->setText(QStringLiteral("后端未就绪"));
        errorLabel_->setVisible(true);
        return;
    }
    const QString acc = accountBox_ ? accountBox_->currentText().trimmed() : QString();
    const QString pwd = passwordAccount_ ? passwordAccount_->text() : QString();
    if (acc.isEmpty() || pwd.isEmpty()) {
        errorLabel_->setText(QStringLiteral("请输入账号和密码"));
        errorLabel_->setVisible(true);
        return;
    }
    if (!agreeCheck_ || !agreeCheck_->isChecked()) {
        errorLabel_->setText(QStringLiteral("请先勾选协议"));
        errorLabel_->setVisible(true);
        return;
    }

    errorLabel_->setVisible(false);
    setLoginBusy(true);
    backend_->registerUserAsync(acc, pwd);
}

void LoginDialog::onRegisterFinished(bool success, const QString &error) {
    setLoginBusy(false);
    if (!success) {
        const QString acc = accountBox_ ? accountBox_->currentText().trimmed() : QString();
        const QString pwd = passwordAccount_ ? passwordAccount_->text() : QString();
        if (handlePendingServerTrustForRegister(acc, pwd)) {
            return;
        }
        const QString err = error.trimmed();
        errorLabel_->setText(err.isEmpty() ? QStringLiteral("注册失败") : err);
        errorLabel_->setVisible(true);
        Toast::Show(this,
                    err.isEmpty()
                        ? UiSettings::Tr(QStringLiteral("注册失败：请检查网络或服务器状态"),
                                         QStringLiteral("Registration failed. Please check your network or server."))
                        : UiSettings::Tr(QStringLiteral("注册失败：%1").arg(err),
                                         QStringLiteral("Registration failed: %1").arg(err)),
                    Toast::Level::Error,
                    3200);
        return;
    }

    const QString acc = accountBox_ ? accountBox_->currentText().trimmed() : QString();
    const QString pwd = passwordAccount_ ? passwordAccount_->text() : QString();
    Toast::Show(this,
                UiSettings::Tr(QStringLiteral("账号已创建，正在登录…"),
                               QStringLiteral("Account created. Signing in…")),
                Toast::Level::Success,
                2000);
    setLoginBusy(true);
    backend_->loginAsync(acc, pwd);
}

bool LoginDialog::handlePendingServerTrust(const QString &account, const QString &password) {
    if (!backend_ || !backend_->hasPendingServerTrust()) {
        return false;
    }

    const QString fingerprintHex = backend_->pendingServerFingerprint();
    const QString pin = backend_->pendingServerPin();

    const QString description = UiSettings::Tr(
        QStringLiteral("检测到需要验证服务器身份（首次连接或证书指纹变更）。\n"
                       "请通过线下可信渠道核对安全码/指纹后再继续。"),
        QStringLiteral("Server identity verification required (first connection or certificate pin changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this,
                            UiSettings::Tr(QStringLiteral("验证服务器身份"),
                                           QStringLiteral("Verify server identity")),
                            description,
                            fingerprintHex,
                            pin,
                            input)) {
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

    setLoginBusy(true);
    backend_->loginAsync(account, password);
    return true;
}

bool LoginDialog::handlePendingServerTrustForRegister(const QString &account, const QString &password) {
    if (!backend_ || !backend_->hasPendingServerTrust()) {
        return false;
    }

    const QString fingerprintHex = backend_->pendingServerFingerprint();
    const QString pin = backend_->pendingServerPin();

    const QString description = UiSettings::Tr(
        QStringLiteral("检测到需要验证服务器身份（首次连接或证书指纹变更）。\n"
                       "请通过线下可信渠道核对安全码/指纹后再继续。"),
        QStringLiteral("Server identity verification required (first connection or certificate pin changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this,
                            UiSettings::Tr(QStringLiteral("验证服务器身份"),
                                           QStringLiteral("Verify server identity")),
                            description,
                            fingerprintHex,
                            pin,
                            input)) {
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

    setLoginBusy(true);
    backend_->registerUserAsync(account, password);
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

void LoginDialog::paintEvent(QPaintEvent *event) {
    QDialog::paintEvent(event);
    if (!frame_) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = frame_->geometry();
    if (r.isEmpty()) {
        return;
    }

    const int shadowSize = 0;
    const QPointF offset(0.0, 6.0);
    const qreal radius = 16.0;
    QColor base(0, 0, 0, 140);
    if (Theme::scheme() == Theme::Scheme::Light) {
        base.setAlpha(110);
    }

    if (shadowSize <= 0) {
        return;
    }
    for (int i = shadowSize; i >= 1; --i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(shadowSize);
        QColor c = base;
        c.setAlphaF(base.alphaF() * t * t);
        QRectF rr = r.adjusted(-i, -i, i, i);
        rr.translate(offset);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(rr, radius + i, radius + i);
    }
}

void LoginDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (introPlayed_) {
        return;
    }
    introPlayed_ = true;
    setWindowOpacity(0.0);
    auto *anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(120);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
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
    accountLoginBtn_->setEnabled(!loginBusy_ && ok);
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
