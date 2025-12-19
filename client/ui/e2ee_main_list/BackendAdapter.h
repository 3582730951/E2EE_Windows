#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QMetaType>
#include <vector>
#include <cstdint>
#include <memory>
#include <QTimer>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "../../include/client_core.h"

class BackendAdapter : public QObject {
    Q_OBJECT

public:
    explicit BackendAdapter(QObject *parent = nullptr);

    struct FriendEntry {
        QString username;
        QString remark;

        QString displayName() const { return remark.isEmpty() ? username : remark; }
    };

    struct FriendRequestEntry {
        QString requesterUsername;
        QString requesterRemark;
    };

    bool init(const QString &configPath = QString());
    bool login(const QString &account, const QString &password, QString &err);
    void loginAsync(const QString &account, const QString &password);
    bool registerUser(const QString &account, const QString &password, QString &err);
    void registerUserAsync(const QString &account, const QString &password);

    bool hasPendingServerTrust() const { return core_.HasPendingServerTrust(); }
    QString pendingServerFingerprint() const { return QString::fromStdString(core_.pending_server_fingerprint()); }
    QString pendingServerPin() const { return QString::fromStdString(core_.pending_server_pin()); }
    QString lastCoreError() const { return QString::fromStdString(core_.last_error()); }

    QVector<FriendEntry> listFriends(QString &err);
    void requestFriendList();
    bool addFriend(const QString &account, const QString &remark, QString &err);
    bool sendFriendRequest(const QString &account, const QString &remark, QString &err);
    QVector<FriendRequestEntry> listFriendRequests(QString &err);
    bool respondFriendRequest(const QString &requester, bool accept, QString &err);
    bool deleteFriend(const QString &account, QString &err);
    bool setUserBlocked(const QString &account, bool blocked, QString &err);
    bool setFriendRemark(const QString &account, const QString &remark, QString &err);
    bool sendText(const QString &targetId, const QString &text, QString &outMessageId, QString &err);
    bool sendTextWithReply(const QString &targetId,
                           const QString &text,
                           const QString &replyToMessageId,
                           const QString &replyPreview,
                           QString &outMessageId,
                           QString &err);
    bool resendText(const QString &targetId, const QString &messageId, const QString &text, QString &err);
    bool sendFile(const QString &targetId, const QString &filePath, QString &outMessageId, QString &err);
    bool resendFile(const QString &targetId, const QString &messageId, const QString &filePath, QString &err);
    bool sendLocation(const QString &targetId,
                      qint32 latE7,
                      qint32 lonE7,
                      const QString &label,
                      QString &outMessageId,
                      QString &err);
    bool sendContactCard(const QString &targetId,
                         const QString &cardUsername,
                         const QString &cardDisplay,
                         QString &outMessageId,
                         QString &err);
    bool sendSticker(const QString &targetId,
                     const QString &stickerId,
                     QString &outMessageId,
                     QString &err);
    bool resendSticker(const QString &targetId,
                       const QString &messageId,
                       const QString &stickerId,
                       QString &err);
    bool sendReadReceipt(const QString &targetId, const QString &messageId, QString &err);
    bool sendTyping(const QString &targetId, bool typing, QString &err);
    bool sendPresence(const QString &targetId, bool online, QString &err);
    bool saveReceivedFile(const QString &convId, const QString &messageId, const QString &outPath, QString &err);
    bool loadReceivedFileBytes(const QString &convId, const QString &messageId, QByteArray &outBytes,
                               qint64 maxBytes, bool wipeAfterRead, QString &err);

    struct HistoryMessageEntry {
        int kind{1};     // 1 text, 2 file, 3 sticker, 4 system
        int status{0};   // 0 sent, 1 delivered, 2 read, 3 failed
        bool outgoing{false};
        quint64 timestampSec{0};
        QString convId;
        QString sender;
        QString messageId;
        QString text;
        QString fileName;
        qint64 fileSize{0};
        QString stickerId;
    };
    bool loadChatHistory(const QString &convId, bool isGroup, int limit,
                         QVector<HistoryMessageEntry> &outEntries, QString &err);

    bool createGroup(QString &outGroupId, QString &err);
    bool joinGroup(const QString &groupId, QString &err);
    bool leaveGroup(const QString &groupId, QString &err);
    QVector<QString> listGroupMembers(const QString &groupId, QString &err);
    struct GroupMemberRoleEntry {
        QString username;
        int role{2};  // 0 owner, 1 admin, 2 member
    };
    QVector<GroupMemberRoleEntry> listGroupMembersInfo(const QString &groupId, QString &err);
    bool setGroupMemberRole(const QString &groupId, const QString &member, int role, QString &err);
    bool kickGroupMember(const QString &groupId, const QString &member, QString &err);
    bool sendGroupInvite(const QString &groupId, const QString &peer, QString &outMessageId, QString &err);
    bool sendGroupText(const QString &groupId, const QString &text, QString &outMessageId, QString &err);
    bool resendGroupText(const QString &groupId, const QString &messageId, const QString &text, QString &err);
    bool sendGroupFile(const QString &groupId, const QString &filePath, QString &outMessageId, QString &err);
    bool resendGroupFile(const QString &groupId, const QString &messageId, const QString &filePath, QString &err);
    bool trustPendingPeer(const QString &pin, QString &err);
    bool trustPendingServer(const QString &pin, QString &err);
    void startPolling(int intervalMs = 2000);

    bool isLoggedIn() const { return loggedIn_; }
    bool isOnline() const { return online_; }
    QString currentUser() const { return currentUser_; }
    QString currentDeviceId() const;
    QString configPath() const { return configPath_; }
    bool isPendingOutgoingMessage(const QString &messageId) const;

    struct DeviceEntry {
        QString deviceId;
        quint32 lastSeenSec{0};
    };
    QVector<DeviceEntry> listDevices(QString &err);
    bool kickDevice(const QString &deviceId, QString &err);

    bool deviceSyncEnabled() const { return core_.device_sync_enabled(); }
    bool deviceSyncIsPrimary() const { return core_.device_sync_is_primary(); }

    struct DevicePairingRequestEntry {
        QString deviceId;
        QString requestIdHex;
    };
    bool beginDevicePairingPrimary(QString &outPairingCode, QString &err);
    bool pollDevicePairingRequests(QVector<DevicePairingRequestEntry> &outRequests, QString &err);
    bool approveDevicePairingRequest(const DevicePairingRequestEntry &request, QString &err);
    bool beginDevicePairingLinked(const QString &pairingCode, QString &err);
    bool pollDevicePairingLinked(bool &outCompleted, QString &err);
    void cancelDevicePairing();

signals:
    void incomingMessage(const QString &convId, bool isGroup, const QString &sender,
                         const QString &messageId, const QString &text, bool isFile, qint64 fileSize);
    void syncedOutgoingMessage(const QString &convId, bool isGroup, const QString &sender,
                               const QString &messageId, const QString &text, bool isFile, qint64 fileSize);
    void incomingSticker(const QString &convId, const QString &sender,
                         const QString &messageId, const QString &stickerId);
    void syncedOutgoingSticker(const QString &convId, const QString &messageId,
                               const QString &stickerId);
    void delivered(const QString &convId, const QString &messageId);
    void read(const QString &convId, const QString &messageId);
    void typingChanged(const QString &convId, bool typing);
    void presenceChanged(const QString &convId, bool online);
    void peerTrustRequired(const QString &peer, const QString &fingerprintHex, const QString &pin);
    void serverTrustRequired(const QString &fingerprintHex, const QString &pin);
    void friendRequestReceived(const QString &requester, const QString &remark);
    void groupInviteReceived(const QString &groupId, const QString &fromUser, const QString &messageId);
    void groupNoticeReceived(const QString &groupId, const QString &text);
    void messageResent(const QString &convId, const QString &messageId);
    void connectionStateChanged(bool online, const QString &detail);
    void friendListLoaded(const QVector<FriendEntry> &friends, const QString &error);
    void fileSendFinished(const QString &convId, const QString &messageId,
                          bool success, const QString &error);
    void fileSaveFinished(const QString &convId, const QString &messageId,
                          bool success, const QString &error, const QString &outPath);
    void loginFinished(bool success, const QString &error);
    void registerFinished(bool success, const QString &error);

private:
    struct PendingOutgoing {
        enum class Kind { Text, ReplyText, Location, ContactCard, Sticker };

        QString convId;
        QString messageId;
        bool isGroup{false};
        bool isFile{false};
        QString text;
        QString filePath;
        Kind kind{Kind::Text};
        QString replyToMessageId;
        QString replyPreview;
        qint32 latE7{0};
        qint32 lonE7{0};
        QString locationLabel;
        QString cardUsername;
        QString cardDisplay;
        QString stickerId;
        int attempts{0};
        qint64 lastAttemptMs{0};
    };

    bool ensureInited(QString &err);
    void pollMessages();
    void handlePollResult(mi::client::ClientCore::ChatPollResult events,
                          std::vector<mi::client::ClientCore::FriendRequestEntry> friendRequests);
    void maybeEmitPeerTrustRequired(bool force);
    void maybeEmitServerTrustRequired(bool force);
    void maybeRetryPendingOutgoing();
    void updateConnectionState();
    void startAsyncFileSend(const QString &convId, bool isGroup,
                            const QString &messageId, const QString &filePath,
                            bool isResend);
    void startAsyncFileSave(const QString &convId,
                            const QString &messageId,
                            const mi::client::ClientCore::ChatFileMessage &file,
                            const QString &outPath);

    mi::client::ClientCore core_;
    bool inited_{false};
    bool loggedIn_{false};
    bool online_{true};
    std::atomic_bool coreWorkActive_{false};
    std::atomic_bool fileTransferActive_{false};
    bool pollingSuspended_{false};
    QString currentUser_;
    QString configPath_{"client_config.ini"};
    std::unique_ptr<QTimer> pollTimer_;
    int basePollIntervalMs_{2000};
    int currentPollIntervalMs_{2000};
    int backoffExp_{0};
    QString lastPeerTrustUser_;
    QString lastPeerTrustFingerprint_;
    QString lastServerTrustFingerprint_;
    bool attemptedAutoStartServer_{false};

    std::unordered_map<std::string, mi::client::ClientCore::ChatFileMessage> receivedFiles_;
    std::unordered_map<std::string, PendingOutgoing> pendingOutgoing_;
    std::unordered_set<std::string> seenFriendRequests_;
    std::unordered_map<std::string, std::string> groupPendingDeliveries_;
    std::vector<std::string> groupPendingOrder_;
};

Q_DECLARE_METATYPE(BackendAdapter::FriendEntry)
Q_DECLARE_METATYPE(QVector<BackendAdapter::FriendEntry>)
