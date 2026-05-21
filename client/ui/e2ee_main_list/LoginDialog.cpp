#include "LoginDialog.h"

#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTimer>
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
    layout->addWidget(authFlow_, 0, Qt::AlignCenter);

    connect(authFlow_, &AuthFlowWidget::closeRequested, this, &QDialog::reject);
    connect(authFlow_, &AuthFlowWidget::authSucceeded, this, [this]() {
        if (authFlow_) {
            authFlow_->clearAllSecrets();
        }
        emit authSucceeded();
        accept();
    });
    connect(authFlow_, &AuthFlowWidget::loginRequested,
            this, &LoginDialog::handleLogin);
    connect(authFlow_, &AuthFlowWidget::registerRequested,
            this, &LoginDialog::handleRegister);
    connect(authFlow_, &AuthFlowWidget::qrLoginRequested,
            this, &LoginDialog::handleQrLoginStart);
    connect(authFlow_, &AuthFlowWidget::qrLoginCancelRequested,
            this, &LoginDialog::handleQrLoginCancel);

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
    handleQrLoginCancel();
    const QString acc = account.trimmed();
    const QString pwd = password;
    const QString rootCode = authFlow_ ? authFlow_->rootCode() : QString();
    if (acc.isEmpty() || pwd.isEmpty()) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("????????"));
            authFlow_->clearLoginSecrets();
        }
        return;
    }
    if (!backend_) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("后端不可用"));
            authFlow_->clearLoginSecrets();
        }
        return;
    }
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
    }
    setLoginBusy(true);
    backend_->loginAsync(acc, pwd, rootCode);
    if (authFlow_) {
        authFlow_->clearLoginSecrets();
    }
}

void LoginDialog::onLoginFinished(bool success, const QString &error) {
    setLoginBusy(false);
    if (!success) {
        if (authFlow_) {
            authFlow_->clearLoginSecrets();
        }
        if (handlePendingServerTrust()) {
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
        authFlow_->clearAllSecrets();
        authFlow_->setErrorMessage(QString());
    }
    emit authSucceeded();
    accept();
}

void LoginDialog::handleRegister(const QString &account, const QString &password) {
    if (loginBusy_) {
        return;
    }
    handleQrLoginCancel();
    const QString acc = account.trimmed();
    const QString pwd = password;
    if (acc.isEmpty() || pwd.isEmpty()) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("?????????"));
            authFlow_->clearRegisterSecrets();
        }
        return;
    }
    if (!backend_) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("后端不可用"));
            authFlow_->clearRegisterSecrets();
        }
        return;
    }
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
    }
    setLoginBusy(true);
    backend_->registerUserAsync(acc, pwd);
    if (authFlow_) {
        authFlow_->clearRegisterSecrets();
    }
}

void LoginDialog::onRegisterFinished(bool success, const QString &error) {
    setLoginBusy(false);
    if (!success) {
        if (authFlow_) {
            authFlow_->clearRegisterSecrets();
        }
        if (handlePendingServerTrustForRegister()) {
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
    if (authFlow_) {
        authFlow_->clearRegisterSecrets();
        authFlow_->setErrorMessage(UiSettings::Tr(
            QStringLiteral("注册成功，请重新输入口令登录。"),
            QStringLiteral("Registration succeeded. Enter your password again to sign in.")));
    }
}

void LoginDialog::handleQrLoginStart() {
    if (loginBusy_) {
        return;
    }
    if (!backend_) {
        if (authFlow_) {
            authFlow_->setErrorMessage(QStringLiteral("后端不可用"));
        }
        return;
    }
    QString payload;
    QString err;
    const QString account = authFlow_ ? authFlow_->account() : QString();
    if (!backend_->beginQrLogin(account, payload, err)) {
        if (authFlow_) {
            authFlow_->setErrorMessage(err.isEmpty()
                ? QStringLiteral("生成二维码失败")
                : err);
        }
        stopQrPolling();
        return;
    }
    if (authFlow_) {
        authFlow_->setErrorMessage(QString());
        authFlow_->setQrPayload(payload);
    }
    qrActive_ = true;
    if (!qrPollTimer_) {
        qrPollTimer_ = new QTimer(this);
        qrPollTimer_->setInterval(1500);
        connect(qrPollTimer_, &QTimer::timeout, this, &LoginDialog::pollQrLogin);
    }
    qrPollTimer_->start();
}

void LoginDialog::handleQrLoginCancel() {
    stopQrPolling();
    if (backend_) {
        backend_->cancelQrLogin();
    }
    if (authFlow_) {
        authFlow_->setQrPayload(QString());
    }
}

void LoginDialog::pollQrLogin() {
    if (!backend_ || !qrActive_) {
        return;
    }
    bool completed = false;
    QString err;
    if (!backend_->pollQrLogin(completed, err)) {
        if (authFlow_) {
            authFlow_->setErrorMessage(err.isEmpty()
                ? QStringLiteral("二维码登录失败")
                : err);
        }
        stopQrPolling();
        return;
    }
    if (completed) {
        stopQrPolling();
        QString regErr;
        if (backend_ && !backend_->registerDevice(QString(), regErr)) {
            const QString lowered = regErr.toLower();
            if (lowered.contains(QStringLiteral("root auth")) ||
                regErr.contains(QStringLiteral("根"))) {
                bool ok = false;
                QString code = QInputDialog::getText(
                    this,
                    QStringLiteral("根授权码"),
                    QStringLiteral("请输入根授权码或授权字符串"),
                    QLineEdit::Password,
                    QString(),
                    &ok);
                if (!ok || code.trimmed().isEmpty()) {
                    if (backend_) {
                        backend_->logout();
                    }
                    if (authFlow_) {
                        authFlow_->setErrorMessage(QStringLiteral("需要根授权码"));
                    }
                    code.clear();
                    return;
                }
                const bool registered = backend_->registerDevice(code, regErr);
                code.clear();
                if (!registered) {
                    if (backend_) {
                        backend_->logout();
                    }
                    if (authFlow_) {
                        authFlow_->setErrorMessage(regErr.isEmpty()
                            ? QStringLiteral("设备注册失败")
                            : regErr);
                    }
                    return;
                }
            } else {
                if (backend_) {
                    backend_->logout();
                }
                if (authFlow_) {
                    authFlow_->setErrorMessage(regErr.isEmpty()
                        ? QStringLiteral("设备注册失败")
                        : regErr);
                }
                return;
            }
        }
        if (authFlow_) {
            authFlow_->clearAllSecrets();
            authFlow_->setErrorMessage(QString());
        }
        emit authSucceeded();
        accept();
    }
}

void LoginDialog::stopQrPolling() {
    qrActive_ = false;
    if (qrPollTimer_) {
        qrPollTimer_->stop();
    }
}

bool LoginDialog::handlePendingServerTrust() {
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

    if (authFlow_) {
        authFlow_->setErrorMessage(UiSettings::Tr(
            QStringLiteral("服务器身份已信任，请重新输入口令登录。"),
            QStringLiteral("Server identity trusted. Enter your password again to sign in.")));
    }
    return true;
}

bool LoginDialog::handlePendingServerTrustForRegister() {
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

    if (authFlow_) {
        authFlow_->setErrorMessage(UiSettings::Tr(
            QStringLiteral("服务器身份已信任，请重新输入口令完成注册。"),
            QStringLiteral("Server identity trusted. Enter your password again to finish registration.")));
    }
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
