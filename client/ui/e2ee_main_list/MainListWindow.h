// E2EE main session list window.
#pragma once

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>

#include <QListView>
#include <QLineEdit>
#include <QStandardItemModel>

#include "../common/FramelessWindowBase.h"

class ChatWindow;
class BackendAdapter;
class QLabel;
class QToolButton;
class IconButton;
class QSortFilterProxyModel;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QActionGroup;
class QCloseEvent;

class MainListWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit MainListWindow(BackendAdapter *backend, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openChatForIndex(const QModelIndex &index);
    void previewChatForIndex(const QModelIndex &index);
    void handleAddFriend();
    void handleCreateGroup();
    void handleJoinGroup();
    void handleDeviceManager();
    void handleSettings();
    void handleNotificationCenter();
    void handleSearchTextChanged(const QString &text);
    void handleIncomingMessage(const QString &convId, bool isGroup, const QString &sender,
                               const QString &messageId, const QString &text, bool isFile, qint64 fileSize);
    void handleIncomingSticker(const QString &convId, const QString &sender,
                               const QString &messageId, const QString &stickerId);
    void handleSyncedOutgoingMessage(const QString &convId, bool isGroup, const QString &sender,
                                     const QString &messageId, const QString &text, bool isFile, qint64 fileSize);
    void handleSyncedOutgoingSticker(const QString &convId, const QString &messageId, const QString &stickerId);
    void handleDelivered(const QString &convId, const QString &messageId);
    void handleRead(const QString &convId, const QString &messageId);
    void handleTypingChanged(const QString &convId, bool typing);
    void handlePresenceChanged(const QString &convId, bool online);
    void handleMessageResent(const QString &convId, const QString &messageId);
    void handleFileSendFinished(const QString &convId, const QString &messageId,
                                bool success, const QString &error);
    void handleFileSaveFinished(const QString &convId, const QString &messageId,
                                bool success, const QString &error, const QString &outPath);
    void handlePeerTrustRequired(const QString &peer, const QString &fingerprintHex, const QString &pin);
    void handleServerTrustRequired(const QString &fingerprintHex, const QString &pin);
    void handleFriendRequestReceived(const QString &requester, const QString &remark);
    void handleGroupInviteReceived(const QString &groupId, const QString &fromUser, const QString &messageId);
    void handleGroupNoticeReceived(const QString &groupId, const QString &text);
    void handleConnectionStateChanged(bool online, const QString &detail);

private:
    enum class ConversationListMode {
        All = 0,
        PinnedOnly = 1,
        GroupsOnly = 2,
    };

    enum class UserPresenceMode {
        Online = 0,
        DoNotDisturb = 1,
        Invisible = 2,
        Offline = 3,
    };

    struct PendingGroupInvite {
        QString groupId;
        QString fromUser;
        QString messageId;
        qint64 receivedMs{0};
    };

    void initTray();
    void showTrayMessage(const QString &title, const QString &message);
    QStandardItem *findItemById(const QString &id) const;
    QModelIndex viewIndexForId(const QString &id) const;
    void selectConversation(const QString &id);
    void setConversationListMode(ConversationListMode mode);
    void updateModePlaceholder();
    void updateNavSelection();
    void updateNotificationBadge();
    void showAppMenu();
    void togglePinnedForId(const QString &id);
    void loadPinned();
    void savePinned() const;
    void setPresenceMode(UserPresenceMode mode);
    void applyPresenceMode();
    void updatePresenceLabel();
    bool presenceEnabled() const;

    QListView *listView_{nullptr};
    QStandardItemModel *model_{nullptr};
    QSortFilterProxyModel *proxyModel_{nullptr};
    QHash<QString, QPointer<ChatWindow>> chatWindows_;
    QPointer<ChatWindow> embeddedChat_;
    QString embeddedConvId_;
    QLineEdit *searchEdit_{nullptr};
    BackendAdapter *backend_{nullptr};
    QToolButton *statusBtn_{nullptr};
    QMenu *statusMenu_{nullptr};
    QActionGroup *statusGroup_{nullptr};
    QAction *statusOnlineAction_{nullptr};
    QAction *statusDndAction_{nullptr};
    QAction *statusInvisibleAction_{nullptr};
    QAction *statusOfflineAction_{nullptr};
    UserPresenceMode presenceMode_{UserPresenceMode::Online};
    bool backendOnline_{false};
    QString connectionDetail_;
    QLabel *bellBadge_{nullptr};
    IconButton *navBellBtn_{nullptr};
    IconButton *navAllBtn_{nullptr};
    IconButton *navPinnedBtn_{nullptr};
    IconButton *navGroupsBtn_{nullptr};
    IconButton *navFilesBtn_{nullptr};
    IconButton *navSettingsBtn_{nullptr};
    IconButton *navMenuBtn_{nullptr};
    QMenu *appMenu_{nullptr};
    QAction *modeAllAction_{nullptr};
    QAction *modePinnedAction_{nullptr};
    QAction *modeGroupsAction_{nullptr};
    ConversationListMode listMode_{ConversationListMode::All};
    QSet<QString> pinnedIds_;
    QHash<QString, QString> pendingFriendRequests_;
    QVector<PendingGroupInvite> pendingGroupInvites_;
    QSystemTrayIcon *tray_{nullptr};
    QMenu *trayMenu_{nullptr};
    QAction *traySettingsAction_{nullptr};
    QAction *trayShowAction_{nullptr};
    QAction *trayNotifyAction_{nullptr};
    QAction *trayPreviewAction_{nullptr};
    QAction *trayAutostartAction_{nullptr};
    QAction *trayExitAction_{nullptr};
    bool closing_{false};
    QHash<QString, qint64> lastNotifyMs_;
};
