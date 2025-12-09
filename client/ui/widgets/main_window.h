#ifndef MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H

#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>

#include "chat_window.h"
#include "theme.h"

namespace mi::client::ui::widgets {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr);

    void setCurrentUser(const QString& user);
    void openConversation(const QString& title);

private:
    void buildLeftPanel(QHBoxLayout* rootLayout);
    void buildMiddlePanel(QHBoxLayout* rootLayout);
    void buildRightPanel(QHBoxLayout* rootLayout);
    void populateGroups();

    UiPalette palette_;
    QWidget* central_{nullptr};
    QListWidget* conversationList_{nullptr};
    ChatWindow* chatWindow_{nullptr};
    QLineEdit* groupEdit_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H
