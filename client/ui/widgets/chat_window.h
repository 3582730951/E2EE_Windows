#ifndef MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H

#include <QScrollArea>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "image_preview_dialog.h"
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
    void openPreviewDialog();

    UiPalette palette_;
    QLabel* titleLabel_{nullptr};
    QScrollArea* messageScroll_{nullptr};
    QWidget* messageContainer_{nullptr};
    QVBoxLayout* messageLayout_{nullptr};
    QTextEdit* input_{nullptr};
    ImagePreviewDialog* previewDialog_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_CHAT_WINDOW_H
