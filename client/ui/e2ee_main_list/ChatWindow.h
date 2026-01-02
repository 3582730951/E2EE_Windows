// Chat window for a selected conversation.
#pragma once

#include <QListWidget>
#include <QPointer>
#include <QListView>
#include <QMenu>
#include <QSet>
#include <QVector>

#include "../common/FramelessWindowBase.h"

class MessageModel;
class MessageDelegate;
class BackendAdapter;
class QTimer;
class QStackedWidget;
class QPushButton;
class QModelIndex;
class QLabel;
class QLineEdit;
class IconButton;
class EmojiPickerDialog;
class ChatInputEdit;

class ChatWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit ChatWindow(BackendAdapter *backend = nullptr, QWidget *parent = nullptr);

    QString conversationId() const;
    void setEmbeddedMode(bool embedded);
    void focusMessageInput();
    bool isChineseInputMode() const;
    bool isThirdPartyImeActive() const;

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
    void setPresenceEnabled(bool enabled);
    void setFileTransferState(const QString &messageId, FileTransferState state, int progress = -1);
    void setFileLocalPath(const QString &messageId, const QString &filePath);

signals:
    void inputModeChanged(bool chinese);
    void imeSourceChanged(bool thirdParty);
    void startVoiceCallRequested(const QString &convId);
    void startVideoCallRequested(const QString &convId);

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
    void showEmojiPicker();

private:
    void buildUi();
    void updateEmptyState();
    void updateEmptyPrompt();
    void updateConversationUiState();
    bool ensureConversationSelected();
    void updateOverlayForTitle(const QString &title);
    void setReplyContext(const QString &messageId, const QString &preview);
    void clearReplyContext();
    void activateMessage(const QModelIndex &index);
    void toggleSearchBar();
    void setSearchActive(bool active);
    void updateSearchResults();
    void goToSearchResult(int index);
    void stepSearchResult(int delta);
    void clearSearchState();
    void updateInputHeight();
    void sendFilePlaceholder();
    void sendImagePlaceholder();
    void sendVoicePlaceholder();
    void sendVideoPlaceholder();
    bool isNearBottom() const;
    void clearNewMessagePill();
    void bumpNewMessagePill(int count);
    void updateNewMessagePillGeometry();
    void refreshFileTransferAnimation();
    bool isStealthActive() const;
    void applyStealthState();

    QString conversationId_;
    bool isGroup_{false};
    bool embeddedMode_{false};
    QLabel *titleLabel_{nullptr};
    QLabel *titleIcon_{nullptr};
    QLabel *presenceLabel_{nullptr};
    QVector<IconButton *> titleActionButtons_;
    IconButton *windowDownBtn_{nullptr};
    IconButton *windowMinBtn_{nullptr};
    IconButton *windowCloseBtn_{nullptr};
    QStackedWidget *messageStack_{nullptr};
    QLabel *emptyTitleLabel_{nullptr};
    QLabel *emptySubLabel_{nullptr};
    QListView *messageView_{nullptr};
    MessageModel *messageModel_{nullptr};
    MessageDelegate *messageDelegate_{nullptr};
    QPushButton *newMessagePill_{nullptr};
    int pendingNewMessages_{0};
    QWidget *searchBar_{nullptr};
    QLineEdit *searchEdit_{nullptr};
    QLabel *searchCountLabel_{nullptr};
    IconButton *searchPrevBtn_{nullptr};
    IconButton *searchNextBtn_{nullptr};
    IconButton *searchCloseBtn_{nullptr};
    QVector<int> searchMatchRows_;
    int searchMatchIndex_{-1};
    QWidget *composer_{nullptr};
    ChatInputEdit *inputEdit_{nullptr};
    IconButton *emojiBtn_{nullptr};
    EmojiPickerDialog *emojiPicker_{nullptr};
    QWidget *replyBar_{nullptr};
    QLabel *replyLabel_{nullptr};
    QLabel *typingLabel_{nullptr};
    QMenu *attachMenu_{nullptr};
    QMenu *sendMenu_{nullptr};
    QAction *sendLocationAction_{nullptr};
    QAction *sendCardAction_{nullptr};
    QAction *sendStickerAction_{nullptr};
    QAction *exportEvidenceAction_{nullptr};
    QAction *readReceiptAction_{nullptr};
    QAction *typingAction_{nullptr};
    QAction *presenceAction_{nullptr};
    QAction *stealthAction_{nullptr};
    QAction *membersAction_{nullptr};
    QAction *inviteAction_{nullptr};
    QAction *leaveAction_{nullptr};
    BackendAdapter *backend_{nullptr};
    QString replyToMessageId_;
    QString replyPreview_;
    QSet<QString> readReceiptSent_;
    QSet<QString> stealthConversations_;
    bool typingSent_{false};
    qint64 lastTypingSentMs_{0};
    qint64 lastMessageInsertMs_{0};
    QTimer *typingStopSendTimer_{nullptr};
    QTimer *typingHideTimer_{nullptr};
    QTimer *presenceHideTimer_{nullptr};
    QTimer *presencePingTimer_{nullptr};
    QTimer *fileTransferAnimTimer_{nullptr};
};
