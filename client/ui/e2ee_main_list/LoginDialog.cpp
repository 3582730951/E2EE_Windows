#include "LoginDialog.h"

#include <QFile>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QtGlobal>

#include "AuthFlowWidget.h"
#include "BackendAdapter.h"
#include "TrustPromptDialog.h"
#include "../common/UiSettings.h"

LoginDialog::LoginDialog(BackendAdapter *backend, QWidget *parent)
    : QDialog(parent), backend_(backend) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("authDialog");
    resize(480, 620);
    setMinimumSize(420, 520);
    applyStyle();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setAlignment(Qt::AlignCenter);

    authFlow_ = new AuthFlowWidget(this);
    const bool demoAuth = qEnvironmentVariableIsSet("MI_E2EE_DEMO_AUTH") || backend_ == nullptr;
    authFlow_->setDemoMode(demoAuth);
    layout->addWidget(authFlow_, 0, Qt::AlignCenter);

    connect(authFlow_, &AuthFlowWidget::closeRequested, this, &QDialog::reject);
    connect(authFlow_, &AuthFlowWidget::authSucceeded, this, [this]() {
        emit authSucceeded();
        accept();
    });
    connect(authFlow_, &AuthFlowWidget::loginRequested,
            this, &LoginDialog::handleLogin);
    connect(authFlow_, &AuthFlowWidget::registerRequested,
            this, &LoginDialog::handleRegister);

    if (backend_) {
        connect(backend_, &BackendAdapter::loginFinished,
                this, &LoginDialog::onLoginFinished);
        connect(backend_, &BackendAdapter::registerFinished,
                this, &LoginDialog::onRegisterFinished);
    }
}

void LoginDialog::applyStyle() {
    QFile file(QStringLiteral(":/mi/e2ee/ui/qss/app.qss"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    setStyleSheet(QString::fromUtf8(file.readAll()));
}

void LoginDialog::setLoginBusy(bool busy) {
    loginBusy_ = busy;
    if (authFlow_) {
        authFlow_->setBusy(busy);
    }
}

void LoginDialog::handleLogin(const QString &account, const QString &password, bool autoLogin) {
    Q_UNUSED(autoLogin);
    if (loginBusy_) {
        return;
    }
    const QString acc = account.trimmed();
    const QString pwd = password;
    if (acc.isEmpty() || pwd.isEmpty()) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("????????"));
        }
        return;
    }
    if (!backend_) {
        emit authSucceeded();
        accept();
        return;
    }
    pendingAccount_ = acc;
    pendingPassword_ = pwd;
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
    }
    setLoginBusy(true);
    backend_->loginAsync(acc, pwd);
}

void LoginDialog::onLoginFinished(bool success, const QString &error) {
    setLoginBusy(false);
    if (!success) {
        if (handlePendingServerTrust(pendingAccount_, pendingPassword_)) {
            return;
        }
        const QString err = error.trimmed();
        if (authFlow_) {
            authFlow_->setErrorMessage(err.isEmpty()
                ? UiSettings::Tr(QStringLiteral("?????????????"),
                                 QStringLiteral("Login failed. Check your account or network."))
                : UiSettings::Tr(QStringLiteral("?????%1").arg(err),
                                 QStringLiteral("Login failed: %1").arg(err)));
        }
        return;
    }
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
    }
    emit authSucceeded();
    accept();
}

void LoginDialog::handleRegister(const QString &account, const QString &password) {
    if (loginBusy_) {
        return;
    }
    const QString acc = account.trimmed();
    const QString pwd = password;
    if (acc.isEmpty() || pwd.isEmpty()) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("?????????"));
        }
        return;
    }
    if (!backend_) {
        emit authSucceeded();
        accept();
        return;
    }
    pendingAccount_ = acc;
    pendingPassword_ = pwd;
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
    }
    setLoginBusy(true);
    backend_->registerUserAsync(acc, pwd);
}

void LoginDialog::onRegisterFinished(bool success, const QString &error) {
    setLoginBusy(false);
    if (!success) {
        if (handlePendingServerTrustForRegister(pendingAccount_, pendingPassword_)) {
            return;
        }
        const QString err = error.trimmed();
        if (authFlow_) {
            authFlow_->setErrorMessage(err.isEmpty()
                ? UiSettings::Tr(QStringLiteral("??????????????"),
                                 QStringLiteral("Registration failed. Check network or server."))
                : UiSettings::Tr(QStringLiteral("?????%1").arg(err),
                                 QStringLiteral("Registration failed: %1").arg(err)));
        }
        return;
    }
    setLoginBusy(true);
    backend_->loginAsync(pendingAccount_, pendingPassword_);
}

bool LoginDialog::handlePendingServerTrust(const QString &account, const QString &password) {
    if (!backend_ || !backend_->hasPendingServerTrust()) {
        return false;
    }

    const QString fingerprintHex = backend_->pendingServerFingerprint();
    const QString pin = backend_->pendingServerPin();

    const QString description = UiSettings::Tr(
        QStringLiteral("??????????????????????????\n"
                       "??????????????/???????"),
        QStringLiteral("Server identity verification required (first connection or certificate pin changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this,
                            UiSettings::Tr(QStringLiteral("???????"),
                                           QStringLiteral("Verify server identity")),
                            description,
                            fingerprintHex,
                            pin,
                            input)) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("?????????TLS?"));
        }
        return true;
    }

    QString trustErr;
    if (!backend_->trustPendingServer(input, trustErr)) {
        QMessageBox::warning(
            this,
            UiSettings::Tr(QStringLiteral("????"), QStringLiteral("Trust failed")),
            trustErr.isEmpty()
                ? UiSettings::Tr(QStringLiteral("????"), QStringLiteral("Trust failed"))
                : trustErr);
        if (authFlow_) {
            authFlow_->setErrorMessage(trustErr.isEmpty() ? QStringLiteral("????") : trustErr);
        }
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
        QStringLiteral("??????????????????????????\n"
                       "??????????????/???????"),
        QStringLiteral("Server identity verification required (first connection or certificate pin changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this,
                            UiSettings::Tr(QStringLiteral("???????"),
                                           QStringLiteral("Verify server identity")),
                            description,
                            fingerprintHex,
                            pin,
                            input)) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("?????????TLS?"));
        }
        return true;
    }

    QString trustErr;
    if (!backend_->trustPendingServer(input, trustErr)) {
        QMessageBox::warning(
            this,
            UiSettings::Tr(QStringLiteral("????"), QStringLiteral("Trust failed")),
            trustErr.isEmpty()
                ? UiSettings::Tr(QStringLiteral("????"), QStringLiteral("Trust failed"))
                : trustErr);
        if (authFlow_) {
            authFlow_->setErrorMessage(trustErr.isEmpty() ? QStringLiteral("????") : trustErr);
        }
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
