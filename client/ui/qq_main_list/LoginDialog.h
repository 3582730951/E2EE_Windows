#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;
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
    void switchToAccountPage();
    void switchToSimplePage();
    void updateLoginEnabled();

private:
    void buildUi();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void toggleInputs();

    QLineEdit *userEdit_{nullptr};
    QLineEdit *passEdit_{nullptr};
    QLabel *errorLabel_{nullptr};
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
    QPoint dragPos_;
    BackendAdapter *backend_{nullptr};
};
