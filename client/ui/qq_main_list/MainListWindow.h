// QQ main session list window replica.
#pragma once

#include <QHash>
#include <QPointer>
#include <QHash>

#include <QListView>
#include <QLineEdit>
#include <QStandardItemModel>

#include "../common/FramelessWindowBase.h"

class ChatWindow;
class BackendAdapter;
class QLabel;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QCloseEvent;

class MainListWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit MainListWindow(BackendAdapter *backend, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openChatForIndex(const QModelIndex &index);
    void handleAddFriend();
    void handleCreateGroup();
    void handleJoinGroup();
    void handleDeviceManager();
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
    void initTray();
    void showTrayMessage(const QString &title, const QString &message);

    QListView *listView_{nullptr};
    QStandardItemModel *model_{nullptr};
    QHash<QString, QPointer<ChatWindow>> chatWindows_;
    QLineEdit *searchEdit_{nullptr};
    BackendAdapter *backend_{nullptr};
    QLabel *connLabel_{nullptr};
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
