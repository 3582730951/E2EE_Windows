#ifndef MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
#define MI_E2EE_CLIENT_UI_QUICK_CLIENT_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariant>
#include <QVideoSink>
#include <QMediaCaptureSession>
#include <QUrl>
#include <memory>
#include <mutex>

#include "client_core.h"
#include "media_pipeline.h"
#include "media_session.h"

class QAudioSink;
class QAudioSource;
class QCamera;
class QIODevice;
class QVideoFrame;

namespace mi::client::ui {

// 轻量桥接：Qt Quick 与 client_core 的同步调用
class QuickClient : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
  Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY tokenChanged)
  Q_PROPERTY(QString username READ username NOTIFY userChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
  Q_PROPERTY(QVariantList friends READ friends NOTIFY friendsChanged)
  Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged)
  Q_PROPERTY(QVariantList friendRequests READ friendRequests NOTIFY friendRequestsChanged)
  Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceChanged)
  Q_PROPERTY(bool remoteOk READ remoteOk NOTIFY connectionChanged)
  Q_PROPERTY(QString remoteError READ remoteError NOTIFY connectionChanged)
  Q_PROPERTY(bool hasPendingServerTrust READ hasPendingServerTrust NOTIFY trustStateChanged)
  Q_PROPERTY(QString pendingServerFingerprint READ pendingServerFingerprint NOTIFY trustStateChanged)
  Q_PROPERTY(QString pendingServerPin READ pendingServerPin NOTIFY trustStateChanged)
  Q_PROPERTY(bool hasPendingPeerTrust READ hasPendingPeerTrust NOTIFY trustStateChanged)
  Q_PROPERTY(QString pendingPeerUsername READ pendingPeerUsername NOTIFY trustStateChanged)
  Q_PROPERTY(QString pendingPeerFingerprint READ pendingPeerFingerprint NOTIFY trustStateChanged)
  Q_PROPERTY(QString pendingPeerPin READ pendingPeerPin NOTIFY trustStateChanged)
  Q_PROPERTY(QString activeCallId READ activeCallId NOTIFY callStateChanged)
  Q_PROPERTY(QString activeCallPeer READ activeCallPeer NOTIFY callStateChanged)
  Q_PROPERTY(bool activeCallVideo READ activeCallVideo NOTIFY callStateChanged)
  Q_PROPERTY(QVideoSink* remoteVideoSink READ remoteVideoSink CONSTANT)
  Q_PROPERTY(QVideoSink* localVideoSink READ localVideoSink CONSTANT)

 public:
  explicit QuickClient(QObject* parent = nullptr);
  ~QuickClient() override;

  Q_INVOKABLE bool init(const QString& configPath);
  Q_INVOKABLE bool registerUser(const QString& user, const QString& pass);
  Q_INVOKABLE bool login(const QString& user, const QString& pass);
  Q_INVOKABLE void logout();
  Q_INVOKABLE bool joinGroup(const QString& groupId);
  Q_INVOKABLE QString createGroup();
  Q_INVOKABLE bool sendGroupInvite(const QString& groupId,
                                   const QString& peerUsername);
  Q_INVOKABLE bool sendText(const QString& convId, const QString& text, bool isGroup);
  Q_INVOKABLE bool sendFile(const QString& convId, const QString& path, bool isGroup);
  Q_INVOKABLE bool sendSticker(const QString& convId, const QString& stickerId, bool isGroup);
  Q_INVOKABLE bool sendLocation(const QString& convId,
                                double lat,
                                double lon,
                                const QString& label,
                                bool isGroup);
  Q_INVOKABLE QVariantMap ensureAttachmentCached(const QString& fileId,
                                                 const QString& fileKeyHex,
                                                 const QString& fileName,
                                                 qint64 fileSize);
  Q_INVOKABLE bool requestAttachmentDownload(const QString& fileId,
                                             const QString& fileKeyHex,
                                             const QString& fileName,
                                             qint64 fileSize,
                                             const QString& savePath);
  Q_INVOKABLE bool requestImageEnhance(const QString& fileUrl,
                                       const QString& fileName);
  Q_INVOKABLE bool requestImageEnhanceForMessage(const QString& messageId,
                                                 const QString& fileUrl,
                                                 const QString& fileName);
  Q_INVOKABLE QVariantList loadHistory(const QString& convId, bool isGroup);
  Q_INVOKABLE QVariantList listGroupMembersInfo(const QString& groupId);
  Q_INVOKABLE QVariantList stickerItems();
  Q_INVOKABLE QVariantMap importSticker(const QString& path);
  Q_INVOKABLE bool sendFriendRequest(const QString& targetUsername,
                                     const QString& remark);
  Q_INVOKABLE bool respondFriendRequest(const QString& requesterUsername,
                                        bool accept);
  Q_INVOKABLE QVariantList listDevices();
  Q_INVOKABLE bool kickDevice(const QString& deviceId);
  Q_INVOKABLE bool sendReadReceipt(const QString& peerUsername,
                                   const QString& messageId);
  Q_INVOKABLE bool trustPendingServer(const QString& pin);
  Q_INVOKABLE bool trustPendingPeer(const QString& pin);
  Q_INVOKABLE QString startVoiceCall(const QString& peerUsername);
  Q_INVOKABLE QString startVideoCall(const QString& peerUsername);
  Q_INVOKABLE bool joinCall(const QString& peerUsername,
                            const QString& callIdHex,
                            bool video);
  Q_INVOKABLE void endCall();
  Q_INVOKABLE void bindRemoteVideoSink(QObject* sink);
  Q_INVOKABLE void bindLocalVideoSink(QObject* sink);
  Q_INVOKABLE QString serverInfo() const;
  Q_INVOKABLE QString version() const;
  Q_INVOKABLE QUrl defaultDownloadFileUrl(const QString& fileName) const;
  Q_INVOKABLE QString systemClipboardText() const;
  Q_INVOKABLE qint64 systemClipboardTimestamp() const;
  Q_INVOKABLE bool imeAvailable();
  Q_INVOKABLE bool imeRimeAvailable();
  Q_INVOKABLE QVariantList imeCandidates(const QString& input, int maxCandidates);
  Q_INVOKABLE QString imePreedit();
  Q_INVOKABLE bool imeCommit(int index);
  Q_INVOKABLE void imeClear();
  Q_INVOKABLE void imeReset();
  Q_INVOKABLE bool internalImeEnabled() const;
  Q_INVOKABLE void setInternalImeEnabled(bool enabled);
  Q_INVOKABLE bool aiEnhanceGpuAvailable() const;
  Q_INVOKABLE bool aiEnhanceEnabled() const;
  Q_INVOKABLE void setAiEnhanceEnabled(bool enabled);
  Q_INVOKABLE bool clipboardIsolation() const;
  Q_INVOKABLE void setClipboardIsolation(bool enabled);

  QString token() const;
  bool loggedIn() const;
  QString username() const;
  QString lastError() const;
  QVariantList friends() const;
  QVariantList groups() const;
  QVariantList friendRequests() const;
  QString deviceId() const;
  bool remoteOk() const;
  QString remoteError() const;
  bool hasPendingServerTrust() const;
  QString pendingServerFingerprint() const;
  QString pendingServerPin() const;
  bool hasPendingPeerTrust() const;
  QString pendingPeerUsername() const;
  QString pendingPeerFingerprint() const;
  QString pendingPeerPin() const;
  QString activeCallId() const { return active_call_id_; }
  QString activeCallPeer() const { return active_call_peer_; }
  bool activeCallVideo() const { return active_call_video_; }
  QVideoSink* remoteVideoSink() const { return remote_video_sink_; }
  QVideoSink* localVideoSink() const { return local_video_sink_; }

 signals:
  void tokenChanged();
  void userChanged();
  void friendsChanged();
  void groupsChanged();
  void friendRequestsChanged();
  void errorChanged();
  void deviceChanged();
  void connectionChanged();
  void trustStateChanged();
  void serverTrustRequired(const QString& fingerprint, const QString& pin);
  void peerTrustRequired(const QString& peer, const QString& fingerprint,
                         const QString& pin);
  void status(const QString& message);
  void messageEvent(const QVariantMap& message);
  void callStateChanged();
  void attachmentCacheReady(const QString& fileId,
                            const QUrl& fileUrl,
                            const QUrl& previewUrl,
                            const QString& error);
  void attachmentDownloadFinished(const QString& fileId,
                                  const QString& savePath,
                                  bool ok,
                                  const QString& error);
  void attachmentDownloadProgress(const QString& fileId,
                                  const QString& savePath,
                                  double progress);
  void imageEnhanceFinished(const QString& messageId,
                            const QString& sourceUrl,
                            const QString& outputUrl,
                            bool ok,
                            const QString& error);

 private:
  void StartPolling();
  void StopPolling();
  void PollOnce();
  void EmitMessage(const QVariantMap& message);
  void UpdateFriendList(const std::vector<ClientCore::FriendEntry>& friends);
  void UpdateFriendRequests(
      const std::vector<ClientCore::FriendRequestEntry>& requests);
  bool AddGroupIfMissing(const QString& groupId);
  QVariantMap BuildStickerMeta(const QString& stickerId) const;
  QVariantMap BuildHistoryMessage(const ClientCore::HistoryEntry& entry) const;
  void HandlePollResult(const ClientCore::ChatPollResult& result);
  void HandleSessionInvalid(const QString& message);
  void UpdateLastError(const QString& message);
  void UpdateConnectionState(bool force_emit);
  void MaybeEmitTrustSignals();
  void EmitDownloadProgress(const QString& fileId,
                            const QString& savePath,
                            double progress);
  bool InitMediaSession(const QString& peerUsername,
                        const QString& callIdHex,
                        bool initiator,
                        bool video,
                        QString& outError);
  void StartMedia();
  void StopMedia();
  void PumpMedia();
  void DrainAudioInput();
  void FlushAudioOutput();
  bool SetupAudio(QString& outError);
  bool SetupVideo(QString& outError);
  void ShutdownAudio();
  void ShutdownVideo();
  void HandleAudioReady();
  void HandleLocalVideoFrame(const QVideoFrame& frame);
  bool ConvertVideoFrameToNv12(const QVideoFrame& frame,
                               std::vector<std::uint8_t>& out,
                               std::uint32_t& width,
                               std::uint32_t& height,
                               std::size_t& stride) const;
  bool SelectCameraFormat();
  QMediaCaptureSession* EnsureCaptureSession();
  void* EnsureImeSession();
  void QueueAttachmentCacheTask(const QString& fileId,
                                const std::array<std::uint8_t, 32>& fileKey,
                                const QString& fileName,
                                qint64 fileSize,
                                bool highPriority);
  void QueueAttachmentRestoreTask(const QString& fileId,
                                  const QString& fileName,
                                  const QString& savePath,
                                  bool highPriority);
  void HandleCacheTaskFinished(const QString& fileId,
                               const QUrl& fileUrl,
                               const QUrl& previewUrl,
                               const QString& error,
                               bool ok);
  void HandleRestoreTaskFinished(const QString& fileId,
                                 const QString& savePath,
                                 bool ok,
                                 const QString& error);
  void MaybeAutoEnhanceImage(const QString& messageId,
                             const QString& filePath,
                             const QString& fileName);

  static QString BytesToHex(const std::array<std::uint8_t, 16>& bytes);
  static bool HexToBytes16(const QString& hex,
                           std::array<std::uint8_t, 16>& out);
  static QString BytesToHex32(const std::array<std::uint8_t, 32>& bytes);
  static bool HexToBytes32(const QString& hex,
                           std::array<std::uint8_t, 32>& out);

  QString config_path_{QStringLiteral("config/client_config.ini")};
  mi::client::ClientCore core_;
  QString token_;
  QString username_;
  QString last_error_;
  QVariantList friends_;
  QVariantList groups_;
  QVariantList friend_requests_;
  QTimer poll_timer_;
  QTimer media_timer_;
  qint64 last_friend_sync_ms_{0};
  qint64 last_request_sync_ms_{0};
  qint64 last_heartbeat_ms_{0};
  bool last_remote_ok_{true};
  QString last_remote_error_;
  QString last_pending_server_fingerprint_;
  QString last_pending_peer_fingerprint_;
  QString last_system_clipboard_text_;
  qint64 last_system_clipboard_ms_{0};
  void* ime_session_{nullptr};
  bool clipboard_isolation_enabled_{true};
  bool internal_ime_enabled_{true};
  bool ai_enhance_enabled_{false};
  std::unique_ptr<mi::client::media::MediaSession> media_session_;
  std::unique_ptr<mi::client::media::AudioPipeline> audio_pipeline_;
  std::unique_ptr<mi::client::media::VideoPipeline> video_pipeline_;
  mi::client::media::AudioPipelineConfig audio_config_{};
  mi::client::media::VideoPipelineConfig video_config_{};
  std::unique_ptr<QAudioSource> audio_source_;
  std::unique_ptr<QAudioSink> audio_sink_;
  QIODevice* audio_in_device_{nullptr};
  QIODevice* audio_out_device_{nullptr};
  QByteArray audio_in_buffer_;
  qsizetype audio_in_offset_{0};
  QByteArray audio_out_pending_;
  std::vector<std::int16_t> audio_frame_tmp_;
  std::unique_ptr<QCamera> camera_;
  std::unique_ptr<QMediaCaptureSession> capture_session_;
  QVideoSink* local_video_sink_{nullptr};
  QVideoSink* remote_video_sink_{nullptr};
  std::vector<std::uint8_t> video_send_buffer_;
  QString active_call_id_;
  QString active_call_peer_;
  bool active_call_video_{false};
  QThreadPool cache_pool_;
  QSet<QString> cache_inflight_;
  QSet<QString> enhance_inflight_;
  QHash<QString, QStringList> pending_downloads_;
  QHash<QString, QString> pending_download_names_;
  QHash<QString, double> download_progress_base_;
  QHash<QString, double> download_progress_span_;
};

}  // namespace mi::client::ui

#endif  // MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
