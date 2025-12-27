#pragma once

#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QWidget;

class AuthFlowWidget : public QWidget {
    Q_OBJECT

public:
    explicit AuthFlowWidget(QWidget *parent = nullptr);

    void setDemoMode(bool enabled);
    void setBusy(bool busy);
    void setErrorMessage(const QString &message);

signals:
    void loginRequested(const QString &account, const QString &password, bool autoLogin);
    void registerRequested(const QString &account, const QString &password);
    void authSucceeded();
    void closeRequested();

private:
    void buildUi();
    void showAccountPage();
    void showRegisterPage();
    void showQrPage();
    void startQrCountdown();
    void updateQrHint();
    QPixmap buildFakeQrPixmap(int size) const;

    void handleLoginClicked();
    void handleRegisterClicked();
    void handleQrSimulateClicked();
    bool eventFilter(QObject *watched, QEvent *event) override;

    QStackedWidget *stack_{nullptr};
    QComboBox *accountBox_{nullptr};
    QLineEdit *passwordEdit_{nullptr};
    QCheckBox *autoLoginCheck_{nullptr};
    QPushButton *loginButton_{nullptr};

    QLineEdit *registerAccountEdit_{nullptr};
    QLineEdit *registerPasswordEdit_{nullptr};
    QLineEdit *registerConfirmEdit_{nullptr};
    QPushButton *registerButton_{nullptr};

    QLabel *qrImage_{nullptr};
    QLabel *qrHint_{nullptr};
    QPushButton *qrRefreshButton_{nullptr};

    QLabel *errorLabel_{nullptr};
    QToolButton *menuButton_{nullptr};
    QToolButton *closeButton_{nullptr};
    QWidget *dragRegion_{nullptr};
    QTimer *qrTimer_{nullptr};
    int qrRemaining_{30};
    bool demoMode_{true};
    bool busy_{false};
    bool dragging_{false};
    QPoint dragOffset_;
};
