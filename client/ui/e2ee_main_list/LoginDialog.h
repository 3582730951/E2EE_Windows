#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
class QFrame;
class QPaintEvent;
class QStackedWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class BackendAdapter;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(BackendAdapter *backend, QWidget *parent = nullptr);

signals:
    void addAccountRequested();

private slots:
    void handleLogin();
    void onLoginFinished(bool success, const QString &error);
    void handleRegister();
    void onRegisterFinished(bool success, const QString &error);
    void switchToAccountPage();
    void switchToSimplePage();
    void updateLoginEnabled();

private:
    void buildUi();
    void setLoginBusy(bool busy);
    bool handlePendingServerTrust(const QString &account, const QString &password);
    bool handlePendingServerTrustForRegister(const QString &account, const QString &password);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void toggleInputs();

    QLineEdit *userEdit_{nullptr};
    QLineEdit *passEdit_{nullptr};
    QLabel *errorLabel_{nullptr};
    QFrame *frame_{nullptr};
    QWidget *inputsContainer_{nullptr};
    QWidget *nameClick_{nullptr};
    QLabel *addLabel_{nullptr};
    bool inputsVisible_{false};
    QStackedWidget *stack_{nullptr};
    QWidget *simplePage_{nullptr};
    QWidget *accountPage_{nullptr};
    QComboBox *accountBox_{nullptr};
    QLineEdit *passwordAccount_{nullptr};
    QCheckBox *agreeCheck_{nullptr};
    QPushButton *accountLoginBtn_{nullptr};
    QPushButton *simpleLoginBtn_{nullptr};
    bool loginBusy_{false};
    bool introPlayed_{false};
    QPoint dragPos_;
    BackendAdapter *backend_{nullptr};
};
