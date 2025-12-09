#ifndef MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>

#include "theme.h"

namespace mi::client::ui::widgets {

struct ChatMessage {
    QString sender;
    QString text;
    QString time;
    bool fromSelf{false};
};

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit ChatWindow(const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr, bool showHeader = true);

    void setGroupName(const QString& name);
    void addMessage(const ChatMessage& message);

signals:
    void messageSent(const ChatMessage& message);

private:
    void buildHeader(QVBoxLayout* parentLayout);
    void buildMessageArea(QVBoxLayout* parentLayout);
    void buildInputArea(QVBoxLayout* parentLayout);
    void scrollToBottom();
    QWidget* buildBubble(const ChatMessage& message, QWidget* parent);
    bool eventFilter(QObject* watched, QEvent* event) override;

    UiPalette palette_;
    QScrollArea* messageScroll_{nullptr};
    QWidget* messageContainer_{nullptr};
    QVBoxLayout* messageLayout_{nullptr};
    QLineEdit* input_{nullptr};
    QLabel* titleLabel_{nullptr};
    QWidget* titleBar_{nullptr};
    QToolButton* btnMin_{nullptr};
    QToolButton* btnMax_{nullptr};
    QToolButton* btnClose_{nullptr};
    bool showHeader_{true};
    QPoint dragPos_;
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
