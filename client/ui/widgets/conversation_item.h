#ifndef MI_E2EE_CLIENT_UI_WIDGETS_CONVERSATION_ITEM_H
#define MI_E2EE_CLIENT_UI_WIDGETS_CONVERSATION_ITEM_H

#include <QLabel>
#include <QWidget>

#include "theme.h"

namespace mi::client::ui::widgets {

class ConversationItem : public QWidget {
    Q_OBJECT

public:
    ConversationItem(const QString& title, const QString& summary, const QString& time,
                     int unread, const UiPalette& palette, QWidget* parent = nullptr);

    QString title() const;
    void setUnreadCount(int unread);

private:
    UiPalette palette_;
    QLabel* unreadLabel_{nullptr};
    QLabel* titleLabel_{nullptr};
    QLabel* summaryLabel_{nullptr};
    QLabel* timeLabel_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_CONVERSATION_ITEM_H
