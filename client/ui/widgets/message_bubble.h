#ifndef MI_E2EE_CLIENT_UI_WIDGETS_MESSAGE_BUBBLE_H
#define MI_E2EE_CLIENT_UI_WIDGETS_MESSAGE_BUBBLE_H

#include <QWidget>

#include "theme.h"

namespace mi::client::ui::widgets {

struct ChatMessage {
    QString sender;
    QString text;
    QString time;
    bool fromSelf{false};
};

class MessageBubble : public QWidget {
    Q_OBJECT

public:
    MessageBubble(const ChatMessage& message, const UiPalette& palette,
                  QWidget* parent = nullptr);

private:
    UiPalette palette_;
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_MESSAGE_BUBBLE_H
