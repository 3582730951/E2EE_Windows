#include "AuthFlowWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "../common/SettingsDialog.h"
#include "../common/UiSettings.h"
#include "../common/UiStyle.h"

namespace {

QPushButton *CreateLinkButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("linkStyle", true);
    return btn;
}

QLabel *CreateTitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("authTitle");
    return label;
}

QLabel *CreateSubtitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("authSubtitle");
    label->setWordWrap(true);
    return label;
}

}  // namespace

AuthFlowWidget::AuthFlowWidget(QWidget *parent) : QWidget(parent) {
    buildUi();
}

void AuthFlowWidget::setDemoMode(bool enabled) {
    demoMode_ = enabled;
}

void AuthFlowWidget::setBusy(bool busy) {
    busy_ = busy;
    const QString loginText = busy ? QStringLiteral("登录中...") : QStringLiteral("登录");
    if (loginButton_) {
        loginButton_->setText(loginText);
        loginButton_->setEnabled(!busy);
    }
    const QString registerText = busy ? QStringLiteral("注册中...") : QStringLiteral("注册");
    if (registerButton_) {
        registerButton_->setText(registerText);
        registerButton_->setEnabled(!busy);
    }
    if (accountBox_) {
        accountBox_->setEnabled(!busy);
    }
    if (passwordEdit_) {
        passwordEdit_->setEnabled(!busy);
    }
    if (autoLoginCheck_) {
        autoLoginCheck_->setEnabled(!busy);
    }
    if (registerAccountEdit_) {
        registerAccountEdit_->setEnabled(!busy);
    }
    if (registerPasswordEdit_) {
        registerPasswordEdit_->setEnabled(!busy);
    }
    if (registerConfirmEdit_) {
        registerConfirmEdit_->setEnabled(!busy);
    }
}

void AuthFlowWidget::setErrorMessage(const QString &message) {
    if (!errorLabel_) {
        return;
    }
    errorLabel_->setText(message);
    errorLabel_->setVisible(!message.isEmpty());
}

void AuthFlowWidget::buildUi() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame(this);
    card->setObjectName("authCard");
    card->setFixedWidth(420);
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 140));
    shadow->setOffset(0, 10);
    card->setGraphicsEffect(shadow);
    outer->addWidget(card, 0, Qt::AlignCenter);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 18, 22, 20);
    cardLayout->setSpacing(12);

    auto *topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 0);
    topBar->addStretch();

    menuButton_ = new QToolButton(card);
    menuButton_->setObjectName("topTool");
    menuButton_->setIcon(QIcon(QStringLiteral(":/mi/e2ee/ui/icons/menu-lines.svg")));
    menuButton_->setIconSize(QSize(16, 16));
    menuButton_->setFixedSize(26, 26);
    menuButton_->setCursor(Qt::PointingHandCursor);
    topBar->addWidget(menuButton_);

    closeButton_ = new QToolButton(card);
    closeButton_->setObjectName("topTool");
    closeButton_->setIcon(QIcon(QStringLiteral(":/mi/e2ee/ui/icons/close-x.svg")));
    closeButton_->setIconSize(QSize(16, 16));
    closeButton_->setFixedSize(26, 26);
    closeButton_->setCursor(Qt::PointingHandCursor);
    topBar->addSpacing(6);
    topBar->addWidget(closeButton_);
    cardLayout->addLayout(topBar);

    auto *menu = new QMenu(menuButton_);
    UiStyle::ApplyMenuStyle(*menu);
    auto *settingsAction = menu->addAction(
        UiSettings::Tr(QStringLiteral("Settings"), QStringLiteral("Settings")));
    menu->addAction(UiSettings::Tr(QStringLiteral("Help"), QStringLiteral("Help")));
    menu->addAction(UiSettings::Tr(QStringLiteral("About"), QStringLiteral("About")));
    connect(menuButton_, &QToolButton::clicked, this, [menu, this]() {
        if (!menu) {
            return;
        }
        menu->exec(menuButton_->mapToGlobal(QPoint(0, menuButton_->height())));
    });
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        dlg.exec();
    });
    connect(closeButton_, &QToolButton::clicked, this, &AuthFlowWidget::closeRequested);

    stack_ = new QStackedWidget(card);
    cardLayout->addWidget(stack_, 1);

    // Account login page
    auto *accountPage = new QWidget(card);
    auto *accountLayout = new QVBoxLayout(accountPage);
    accountLayout->setContentsMargins(0, 0, 0, 0);
    accountLayout->setSpacing(10);
    accountLayout->addWidget(CreateTitle(QStringLiteral("Account Login"), accountPage));
    accountLayout->addWidget(CreateSubtitle(QStringLiteral("Sign in with account credentials."), accountPage));

    accountBox_ = new QComboBox(accountPage);
    accountBox_->setEditable(true);
    if (auto *edit = accountBox_->lineEdit()) {
        edit->setPlaceholderText(QStringLiteral("Account / phone / email"));
    }
    accountLayout->addWidget(accountBox_);

    passwordEdit_ = new QLineEdit(accountPage);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("Password"));
    accountLayout->addWidget(passwordEdit_);

    autoLoginCheck_ = new QCheckBox(QStringLiteral("Auto login"), accountPage);
    accountLayout->addWidget(autoLoginCheck_);

    loginButton_ = new QPushButton(QStringLiteral("Sign in"), accountPage);
    loginButton_->setObjectName("primaryButton");
    loginButton_->setFixedHeight(40);
    loginButton_->setCursor(Qt::PointingHandCursor);
    accountLayout->addWidget(loginButton_);

    auto *accountLinks = new QHBoxLayout();
    accountLinks->addStretch();
    auto *toRegisterBtn = CreateLinkButton(QStringLiteral("Create account"), accountPage);
    auto *toQrBtn = CreateLinkButton(QStringLiteral("QR login"), accountPage);
    accountLinks->addWidget(toRegisterBtn);
    accountLinks->addSpacing(12);
    accountLinks->addWidget(toQrBtn);
    accountLinks->addStretch();
    accountLayout->addLayout(accountLinks);
    accountLayout->addStretch();

    // Register page
    auto *registerPage = new QWidget(card);
    auto *registerLayout = new QVBoxLayout(registerPage);
    registerLayout->setContentsMargins(0, 0, 0, 0);
    registerLayout->setSpacing(10);
    registerLayout->addWidget(CreateTitle(QStringLiteral("Register"), registerPage));
    registerLayout->addWidget(CreateSubtitle(QStringLiteral("Create a new account."), registerPage));

    registerAccountEdit_ = new QLineEdit(registerPage);
    registerAccountEdit_->setPlaceholderText(QStringLiteral("Username / phone / email"));
    registerLayout->addWidget(registerAccountEdit_);

    registerPasswordEdit_ = new QLineEdit(registerPage);
    registerPasswordEdit_->setPlaceholderText(QStringLiteral("Password"));
    registerPasswordEdit_->setEchoMode(QLineEdit::Password);
    registerLayout->addWidget(registerPasswordEdit_);

    registerConfirmEdit_ = new QLineEdit(registerPage);
    registerConfirmEdit_->setPlaceholderText(QStringLiteral("Confirm password"));
    registerConfirmEdit_->setEchoMode(QLineEdit::Password);
    registerLayout->addWidget(registerConfirmEdit_);

    registerButton_ = new QPushButton(QStringLiteral("Register"), registerPage);
    registerButton_->setObjectName("primaryButton");
    registerButton_->setFixedHeight(40);
    registerButton_->setCursor(Qt::PointingHandCursor);
    registerLayout->addWidget(registerButton_);

    auto *registerLinks = new QHBoxLayout();
    registerLinks->addStretch();
    auto *backToLoginBtn = CreateLinkButton(QStringLiteral("Back to login"), registerPage);
    auto *registerQrBtn = CreateLinkButton(QStringLiteral("QR login"), registerPage);
    registerLinks->addWidget(backToLoginBtn);
    registerLinks->addSpacing(12);
    registerLinks->addWidget(registerQrBtn);
    registerLinks->addStretch();
    registerLayout->addLayout(registerLinks);
    registerLayout->addStretch();

    // QR page
    auto *qrPage = new QWidget(card);
    auto *qrLayout = new QVBoxLayout(qrPage);
    qrLayout->setContentsMargins(0, 0, 0, 0);
    qrLayout->setSpacing(10);
    qrLayout->addWidget(CreateTitle(QStringLiteral("QR Login"), qrPage));
    qrLayout->addWidget(CreateSubtitle(QStringLiteral("Use your phone to scan the code."), qrPage));

    qrImage_ = new QLabel(qrPage);
    qrImage_->setObjectName("qrBox");
    qrImage_->setFixedSize(200, 200);
    qrImage_->setAlignment(Qt::AlignCenter);
    qrImage_->setPixmap(buildFakeQrPixmap(180));
    qrImage_->setCursor(Qt::PointingHandCursor);
    qrImage_->installEventFilter(this);
    qrLayout->addWidget(qrImage_, 0, Qt::AlignHCenter);

    auto *hintRow = new QHBoxLayout();
    qrHint_ = CreateSubtitle(QStringLiteral("QR refresh in 30s"), qrPage);
    qrRefreshButton_ = CreateLinkButton(QStringLiteral("Refresh"), qrPage);
    hintRow->addStretch();
    hintRow->addWidget(qrHint_);
    hintRow->addSpacing(8);
    hintRow->addWidget(qrRefreshButton_);
    hintRow->addStretch();
    qrLayout->addLayout(hintRow);

    auto *qrLinks = new QHBoxLayout();
    qrLinks->addStretch();
    auto *qrBackBtn = CreateLinkButton(QStringLiteral("Back to login"), qrPage);
    auto *qrRegisterBtn = CreateLinkButton(QStringLiteral("Create account"), qrPage);
    qrLinks->addWidget(qrBackBtn);
    qrLinks->addSpacing(12);
    qrLinks->addWidget(qrRegisterBtn);
    qrLinks->addStretch();
    qrLayout->addLayout(qrLinks);
    qrLayout->addStretch();

    stack_->addWidget(accountPage);
    stack_->addWidget(registerPage);
    stack_->addWidget(qrPage);
    stack_->setCurrentWidget(accountPage);

    errorLabel_ = new QLabel(card);
    errorLabel_->setObjectName("authError");
    errorLabel_->setAlignment(Qt::AlignCenter);
    errorLabel_->setVisible(false);
    cardLayout->addWidget(errorLabel_);

    connect(loginButton_, &QPushButton::clicked, this, &AuthFlowWidget::handleLoginClicked);
    connect(registerButton_, &QPushButton::clicked, this, &AuthFlowWidget::handleRegisterClicked);
    connect(toRegisterBtn, &QPushButton::clicked, this, &AuthFlowWidget::showRegisterPage);
    connect(toQrBtn, &QPushButton::clicked, this, &AuthFlowWidget::showQrPage);
    connect(backToLoginBtn, &QPushButton::clicked, this, &AuthFlowWidget::showAccountPage);
    connect(registerQrBtn, &QPushButton::clicked, this, &AuthFlowWidget::showQrPage);
    connect(qrBackBtn, &QPushButton::clicked, this, &AuthFlowWidget::showAccountPage);
    connect(qrRegisterBtn, &QPushButton::clicked, this, &AuthFlowWidget::showRegisterPage);
    connect(qrRefreshButton_, &QPushButton::clicked, this, [this]() {
        qrImage_->setPixmap(buildFakeQrPixmap(180));
        startQrCountdown();
    });

    qrTimer_ = new QTimer(this);
    qrTimer_->setInterval(1000);
    connect(qrTimer_, &QTimer::timeout, this, [this]() {
        if (qrRemaining_ > 0) {
            --qrRemaining_;
            updateQrHint();
            if (qrRemaining_ == 0) {
                qrTimer_->stop();
            }
        }
    });
    startQrCountdown();
}

void AuthFlowWidget::showAccountPage() {
    if (stack_) {
        stack_->setCurrentIndex(0);
    }
    setErrorMessage(QString());
}

void AuthFlowWidget::showRegisterPage() {
    if (stack_) {
        stack_->setCurrentIndex(1);
    }
    setErrorMessage(QString());
}

void AuthFlowWidget::showQrPage() {
    if (stack_) {
        stack_->setCurrentIndex(2);
    }
    setErrorMessage(QString());
}

void AuthFlowWidget::startQrCountdown() {
    qrRemaining_ = 30;
    updateQrHint();
    if (qrTimer_) {
        qrTimer_->start();
    }
}

void AuthFlowWidget::updateQrHint() {
    if (!qrHint_) {
        return;
    }
    if (qrRemaining_ > 0) {
        qrHint_->setText(QStringLiteral("QR refresh in %1s").arg(qrRemaining_));
    } else {
        qrHint_->setText(QStringLiteral("QR expired. Please refresh."));
    }
}

QPixmap AuthFlowWidget::buildFakeQrPixmap(int size) const {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 12, 12));

    const int cells = 21;
    const int cell = size / cells;
    auto drawMarker = [&](int cx, int cy) {
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 7; ++x) {
                const bool border = (x == 0 || x == 6 || y == 0 || y == 6);
                const bool inner = (x >= 2 && x <= 4 && y >= 2 && y <= 4);
                if (border || inner) {
                    painter.drawRect((cx + x) * cell, (cy + y) * cell, cell, cell);
                }
            }
        }
    };
    drawMarker(0, 0);
    drawMarker(cells - 7, 0);
    drawMarker(0, cells - 7);

    for (int y = 0; y < cells; ++y) {
        for (int x = 0; x < cells; ++x) {
            const bool inMarker = (x < 7 && y < 7) ||
                                  (x >= cells - 7 && y < 7) ||
                                  (x < 7 && y >= cells - 7);
            if (inMarker) {
                continue;
            }
            if (((x * 7 + y * 11) % 13) < 5) {
                painter.drawRect(x * cell, y * cell, cell, cell);
            }
        }
    }
    return pixmap;
}

void AuthFlowWidget::handleLoginClicked() {
    if (busy_) {
        return;
    }
    const QString account = accountBox_ ? accountBox_->currentText().trimmed() : QString();
    const QString password = passwordEdit_ ? passwordEdit_->text() : QString();
    if (account.isEmpty() || password.isEmpty()) {
        setErrorMessage(QStringLiteral("Enter account and password."));
        return;
    }
    setErrorMessage(QString());
    if (demoMode_) {
        emit authSucceeded();
        return;
    }
    emit loginRequested(account, password, autoLoginCheck_ && autoLoginCheck_->isChecked());
}

void AuthFlowWidget::handleRegisterClicked() {
    if (busy_) {
        return;
    }
    const QString account = registerAccountEdit_ ? registerAccountEdit_->text().trimmed() : QString();
    const QString password = registerPasswordEdit_ ? registerPasswordEdit_->text() : QString();
    const QString confirm = registerConfirmEdit_ ? registerConfirmEdit_->text() : QString();
    if (account.isEmpty() || password.isEmpty() || confirm.isEmpty()) {
        setErrorMessage(QStringLiteral("Complete the registration fields."));
        return;
    }
    if (password != confirm) {
        setErrorMessage(QStringLiteral("Passwords do not match."));
        return;
    }
    setErrorMessage(QString());
    if (demoMode_) {
        emit authSucceeded();
        return;
    }
    emit registerRequested(account, password);
}

void AuthFlowWidget::handleQrSimulateClicked() {
    if (busy_) {
        return;
    }
    setErrorMessage(QString());
    emit authSucceeded();
}

bool AuthFlowWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == qrImage_ && event->type() == QEvent::MouseButtonPress) {
        handleQrSimulateClicked();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
