// Chat window for a selected conversation.
#pragma once

#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QListView>
#include <QMenu>
#include <QSet>

#include "../common/FramelessWindowBase.h"

class MessageModel;
class BackendAdapter;
class QTimer;
class QStackedWidget;
class QPushButton;
class QModelIndex;

class ChatWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit ChatWindow(BackendAdapter *backend = nullptr, QWidget *parent = nullptr);

    enum class FileTransferState : int {
        None = 0,
        Uploading = 1,
        Downloading = 2,
    };

    void setConversation(const QString &id, const QString &title, bool isGroup);
    void appendIncomingMessage(const QString &sender, const QString &messageId, const QString &text,
                               bool isFile, qint64 fileSize, const QDateTime &time);
    void appendIncomingSticker(const QString &sender, const QString &messageId,
                               const QString &stickerId, const QDateTime &time);
    void appendSyncedOutgoingMessage(const QString &messageId, const QString &text,
                                     bool isFile, qint64 fileSize, const QDateTime &time);
    void appendSyncedOutgoingSticker(const QString &messageId, const QString &stickerId,
                                     const QDateTime &time);
    void appendSystemMessage(const QString &text, const QDateTime &time);
    void markDelivered(const QString &messageId);
    void markRead(const QString &messageId);
    void markSent(const QString &messageId);
    void markFailed(const QString &messageId);
    void setTypingIndicator(bool typing);
    void setPresenceIndicator(bool online);
    void setFileTransferState(const QString &messageId, FileTransferState state, int progress = -1);
    void setFileLocalPath(const QString &messageId, const QString &filePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void sendMessage();
    void sendStickerPlaceholder();
    void sendLocationPlaceholder();
    void sendContactCardPlaceholder();
    void exportEvidencePackage();
    void showMessageMenu(const QPoint &pos);
    void manageGroupMembers();
    void inviteMember();
    void leaveGroup();

private:
    void buildUi();
    void updateEmptyState();
    void updateOverlayForTitle(const QString &title);
    void setReplyContext(const QString &messageId, const QString &preview);
    void clearReplyContext();
    void activateMessage(const QModelIndex &index);
    void sendFilePlaceholder();
    void sendImagePlaceholder();
    void sendVoicePlaceholder();
    void sendVideoPlaceholder();
    bool isNearBottom() const;
    void clearNewMessagePill();
    void bumpNewMessagePill(int count);
    void updateNewMessagePillGeometry();
    void refreshFileTransferAnimation();

    QString conversationId_;
    bool isGroup_{false};
    QLabel *titleLabel_{nullptr};
    QLabel *presenceLabel_{nullptr};
    QStackedWidget *messageStack_{nullptr};
    QListView *messageView_{nullptr};
    MessageModel *messageModel_{nullptr};
    QPushButton *newMessagePill_{nullptr};
    int pendingNewMessages_{0};
    QPlainTextEdit *inputEdit_{nullptr};
    QWidget *replyBar_{nullptr};
    QLabel *replyLabel_{nullptr};
    QLabel *typingLabel_{nullptr};
    QMenu *sendMenu_{nullptr};
    QAction *sendLocationAction_{nullptr};
    QAction *sendCardAction_{nullptr};
    QAction *sendStickerAction_{nullptr};
    QAction *exportEvidenceAction_{nullptr};
    QAction *readReceiptAction_{nullptr};
    QAction *typingAction_{nullptr};
    QAction *presenceAction_{nullptr};
    QAction *membersAction_{nullptr};
    QAction *inviteAction_{nullptr};
    QAction *leaveAction_{nullptr};
    BackendAdapter *backend_{nullptr};
    QString replyToMessageId_;
    QString replyPreview_;
    QSet<QString> readReceiptSent_;
    bool typingSent_{false};
    qint64 lastTypingSentMs_{0};
    qint64 lastMessageInsertMs_{0};
    QTimer *typingStopSendTimer_{nullptr};
    QTimer *typingHideTimer_{nullptr};
    QTimer *presenceHideTimer_{nullptr};
    QTimer *presencePingTimer_{nullptr};
    QTimer *fileTransferAnimTimer_{nullptr};
};
