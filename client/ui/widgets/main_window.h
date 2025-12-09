#ifndef MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H

#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QSplitter>

#include "chat_window.h"
#include "conversation_item.h"
#include "member_panel.h"
#include "navigation_button.h"
#include "theme.h"

namespace mi::client::ui::widgets {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr);

    void setCurrentUser(const QString& user);

private:
    void buildLayout();
    void buildNavigation(QHBoxLayout* rootLayout);
    void buildConversations(QHBoxLayout* rootLayout);
    void buildChatArea(QHBoxLayout* rootLayout);
    void populateConversations();

    UiPalette palette_;
    QWidget* central_{nullptr};
    NavigationButton* contactsBtn_{nullptr};
    NavigationButton* groupsBtn_{nullptr};
    NavigationButton* filesBtn_{nullptr};
    QListWidget* conversationList_{nullptr};
    ChatWindow* chatWindow_{nullptr};
    MemberPanel* memberPanel_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_MAIN_WINDOW_H
