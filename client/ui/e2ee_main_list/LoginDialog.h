#pragma once

#include <QDialog>
#include <QPoint>
#include <QString>

class AuthFlowWidget;
class BackendAdapter;
class QMouseEvent;
class QTimer;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(BackendAdapter *backend, QWidget *parent = nullptr);

signals:
    void authSucceeded();

private slots:
    void handleLogin(const QString &account, const QString &password, bool autoLogin);
    void onLoginFinished(bool success, const QString &error);
    void handleRegister(const QString &account, const QString &password);
    void onRegisterFinished(bool success, const QString &error);
    void handleQrLoginStart();
    void handleQrLoginCancel();
    void pollQrLogin();

private:
    void applyStyle();
    void setLoginBusy(bool busy);
    bool handlePendingServerTrust(const QString &account, const QString &password);
    bool handlePendingServerTrustForRegister(const QString &account, const QString &password);
    void stopQrPolling();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    AuthFlowWidget *authFlow_{nullptr};
    BackendAdapter *backend_{nullptr};
    bool loginBusy_{false};
    bool qrActive_{false};
    QString pendingAccount_;
    QString pendingPassword_;
    QString pendingRootCode_;
    QPoint dragPos_;
    QTimer *qrPollTimer_{nullptr};
};
