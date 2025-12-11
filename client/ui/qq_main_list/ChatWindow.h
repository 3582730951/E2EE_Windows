// Chat window for a selected conversation.
#pragma once

#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QListView>
#include <QMenu>

#include "../common/FramelessWindowBase.h"

class MessageModel;
class BackendAdapter;

class ChatWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit ChatWindow(BackendAdapter *backend = nullptr, QWidget *parent = nullptr);

    void setConversation(const QString &id, const QString &title);
    void appendIncomingMessage(const QString &text, const QDateTime &time);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void sendMessage();

private:
    void buildUi();
    void appendMessage(const QString &text);
    void updateOverlayForTitle(const QString &title);
    void sendFilePlaceholder();

    QString conversationId_;
    QLabel *titleLabel_{nullptr};
    QListView *messageView_{nullptr};
    MessageModel *messageModel_{nullptr};
    QPlainTextEdit *inputEdit_{nullptr};
    QMenu *sendMenu_{nullptr};
    BackendAdapter *backend_{nullptr};
};
