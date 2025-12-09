#ifndef MI_E2EE_CLIENT_UI_WIDGETS_LOGIN_DIALOG_H
#define MI_E2EE_CLIENT_UI_WIDGETS_LOGIN_DIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

#include "theme.h"

namespace mi::client::ui::widgets {

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(const UiPalette& palette = DefaultPalette(),
                         QWidget* parent = nullptr);

    QString username() const;
    void setAccounts(const QStringList& users);

signals:
    void addAccountRequested();
    void removeAccountRequested(const QString& user);

private:
    void setupUi();
    void setupPalette();
    void connectSignals();

    UiPalette palette_;
    QLabel* avatarLabel_{nullptr};
    QComboBox* userBox_{nullptr};
    QPushButton* loginButton_{nullptr};
    QPushButton* addAccountLink_{nullptr};
    QPushButton* removeAccountLink_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_LOGIN_DIALOG_H
