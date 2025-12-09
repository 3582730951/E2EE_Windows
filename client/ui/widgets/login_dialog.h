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

private:
    void setupUi();
    void setupPalette();
    void connectSignals();

    UiPalette palette_;
    QComboBox* userBox_{nullptr};
    QLineEdit* passwordEdit_{nullptr};
    QPushButton* loginButton_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_LOGIN_DIALOG_H
