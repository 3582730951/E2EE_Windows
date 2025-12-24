#ifndef MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
#define MI_E2EE_CLIENT_UI_QUICK_CLIENT_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

#include "client_core.h"
#include "media_session.h"

namespace mi::client::ui {

// 轻量桥接：Qt Quick 与 client_core 的同步调用
class QuickClient : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
  Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY tokenChanged)
  Q_PROPERTY(QString username READ username NOTIFY userChanged)
  Q_PROPERTY(QVariantList friends READ friends NOTIFY friendsChanged)
  Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged)
  Q_PROPERTY(QVariantList friendRequests READ friendRequests NOTIFY friendRequestsChanged)
  Q_PROPERTY(QString activeCallId READ activeCallId NOTIFY callStateChanged)
  Q_PROPERTY(QString activeCallPeer READ activeCallPeer NOTIFY callStateChanged)
  Q_PROPERTY(bool activeCallVideo READ activeCallVideo NOTIFY callStateChanged)

 public:
  explicit QuickClient(QObject* parent = nullptr);
  ~QuickClient() override;

  Q_INVOKABLE bool init(const QString& configPath);
  Q_INVOKABLE bool login(const QString& user, const QString& pass);
  Q_INVOKABLE void logout();
  Q_INVOKABLE bool joinGroup(const QString& groupId);
  Q_INVOKABLE QString createGroup();
  Q_INVOKABLE bool sendText(const QString& convId, const QString& text, bool isGroup);
  Q_INVOKABLE bool sendFile(const QString& convId, const QString& path, bool isGroup);
  Q_INVOKABLE bool sendSticker(const QString& convId, const QString& stickerId, bool isGroup);
  Q_INVOKABLE QVariantList loadHistory(const QString& convId, bool isGroup);
  Q_INVOKABLE QVariantList stickerItems();
  Q_INVOKABLE bool sendFriendRequest(const QString& targetUsername,
                                     const QString& remark);
  Q_INVOKABLE bool respondFriendRequest(const QString& requesterUsername,
                                        bool accept);
  Q_INVOKABLE QString startVoiceCall(const QString& peerUsername);
  Q_INVOKABLE QString startVideoCall(const QString& peerUsername);
  Q_INVOKABLE bool joinCall(const QString& peerUsername,
                            const QString& callIdHex,
                            bool video);
  Q_INVOKABLE void endCall();
  Q_INVOKABLE QString serverInfo() const;
  Q_INVOKABLE QString version() const;

  QString token() const;
  bool loggedIn() const;
  QString username() const;
  QVariantList friends() const;
  QVariantList groups() const;
  QVariantList friendRequests() const;
  QString activeCallId() const { return active_call_id_; }
  QString activeCallPeer() const { return active_call_peer_; }
  bool activeCallVideo() const { return active_call_video_; }

 signals:
  void tokenChanged();
  void userChanged();
  void friendsChanged();
  void groupsChanged();
  void friendRequestsChanged();
  void status(const QString& message);
  void messageEvent(const QVariantMap& message);
  void callStateChanged();

 private:
  void StartPolling();
  void StopPolling();
  void PollOnce();
  void EmitMessage(const QVariantMap& message);
  void UpdateFriendList(const std::vector<ClientCore::FriendEntry>& friends);
  void UpdateFriendRequests(
      const std::vector<ClientCore::FriendRequestEntry>& requests);
  void AddGroupIfMissing(const QString& groupId);
  QVariantMap BuildStickerMeta(const QString& stickerId) const;
  QVariantMap BuildHistoryMessage(const ClientCore::HistoryEntry& entry) const;
  void HandlePollResult(const ClientCore::ChatPollResult& result);
  bool InitMediaSession(const QString& peerUsername,
                        const QString& callIdHex,
                        bool initiator,
                        bool video,
                        QString& outError);

  static QString BytesToHex(const std::array<std::uint8_t, 16>& bytes);
  static bool HexToBytes16(const QString& hex,
                           std::array<std::uint8_t, 16>& out);

  QString config_path_{QStringLiteral("config/client_config.ini")};
  mi::client::ClientCore core_;
  QString token_;
  QString username_;
  QVariantList friends_;
  QVariantList groups_;
  QVariantList friend_requests_;
  QTimer poll_timer_;
  qint64 last_friend_sync_ms_{0};
  qint64 last_request_sync_ms_{0};
  qint64 last_heartbeat_ms_{0};
  std::unique_ptr<mi::client::media::MediaSession> media_session_;
  QString active_call_id_;
  QString active_call_peer_;
  bool active_call_video_{false};
};

}  // namespace mi::client::ui

#endif  // MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
