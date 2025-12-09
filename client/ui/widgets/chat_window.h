#ifndef MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "message_bubble.h"
#include "theme.h"

namespace mi::client::ui::widgets {

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit ChatWindow(const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr);

    void setGroupName(const QString& name);
    void addMessage(const ChatMessage& message);

signals:
    void messageSent(const ChatMessage& message);

private:
    void buildHeader(QVBoxLayout* parentLayout);
    void buildMessageArea(QVBoxLayout* parentLayout);
    void buildInputArea(QVBoxLayout* parentLayout);
    void scrollToBottom();

    UiPalette palette_;
    QScrollArea* messageScroll_{nullptr};
    QWidget* messageContainer_{nullptr};
    QVBoxLayout* messageLayout_{nullptr};
    QLineEdit* input_{nullptr};
    QComboBox* threshold_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
