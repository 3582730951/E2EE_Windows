#include "quick_client.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QUrl>

#include <array>
#include <cstring>
#include <utility>

#include "common/EmojiPackManager.h"
#include "common/UiRuntimePaths.h"

namespace mi::client::ui {

namespace {
constexpr char kCallVoicePrefix[] = "[call]voice:";
constexpr char kCallVideoPrefix[] = "[call]video:";

QString FindConfigFile(const QString& name) {
  if (name.isEmpty()) {
    return {};
  }
  const QFileInfo info(name);
  const QString appRoot = UiRuntimePaths::AppRootDir();
  const QString baseDir =
      appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
  if (info.isAbsolute()) {
    return QFile::exists(name) ? name : QString();
  }
  if (info.path() != QStringLiteral(".") && !info.path().isEmpty()) {
    const QString candidate = baseDir + QStringLiteral("/") + name;
    if (QFile::exists(candidate)) {
      return candidate;
    }
    if (QFile::exists(name)) {
      return QFileInfo(name).absoluteFilePath();
    }
    return {};
  }
  const QString in_config = baseDir + QStringLiteral("/config/") + name;
  if (QFile::exists(in_config)) {
    return in_config;
  }
  const QString in_app = baseDir + QStringLiteral("/") + name;
  if (QFile::exists(in_app)) {
    return in_app;
  }
  if (QFile::exists(name)) {
    return QFileInfo(name).absoluteFilePath();
  }
  return {};
}

QString NowTimeString() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

struct CallInvite {
  bool ok{false};
  bool video{false};
  QString callId;
};

CallInvite ParseCallInvite(const QString& text) {
  CallInvite invite;
  if (text.startsWith(QString::fromLatin1(kCallVoicePrefix))) {
    invite.ok = true;
    invite.video = false;
    invite.callId = text.mid(static_cast<int>(std::strlen(kCallVoicePrefix)));
  } else if (text.startsWith(QString::fromLatin1(kCallVideoPrefix))) {
    invite.ok = true;
    invite.video = true;
    invite.callId = text.mid(static_cast<int>(std::strlen(kCallVideoPrefix)));
  }
  invite.callId = invite.callId.trimmed();
  if (invite.callId.isEmpty()) {
    invite.ok = false;
  }
  return invite;
}

}  // namespace

QuickClient::QuickClient(QObject* parent) : QObject(parent) {
  poll_timer_.setInterval(500);
  poll_timer_.setTimerType(Qt::CoarseTimer);
  connect(&poll_timer_, &QTimer::timeout, this, &QuickClient::PollOnce);
}

QuickClient::~QuickClient() {
  StopPolling();
  core_.Logout();
}

bool QuickClient::init(const QString& configPath) {
  if (!configPath.isEmpty()) {
    config_path_ = configPath;
  } else {
    config_path_ = FindConfigFile(QStringLiteral("config/client_config.ini"));
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("client_config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("config.ini"));
    }
    if (config_path_.isEmpty()) {
      const QString appRoot = UiRuntimePaths::AppRootDir();
      const QString baseDir =
          appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
      config_path_ = baseDir + QStringLiteral("/config/client_config.ini");
    }
  }
  const bool ok = core_.Init(config_path_.toStdString());
  if (!ok) {
    emit status(QStringLiteral("初始化失败"));
  }
  return ok;
}

bool QuickClient::login(const QString& user, const QString& pass) {
  const bool ok = core_.Login(user.toStdString(), pass.toStdString());
  if (!ok) {
    emit status(QStringLiteral("登录失败"));
    token_.clear();
    username_.clear();
    StopPolling();
  } else {
    token_ = QString::fromStdString(core_.token());
    username_ = user.trimmed();
    emit status(QStringLiteral("登录成功"));
    StartPolling();
    UpdateFriendList(core_.ListFriends());
    UpdateFriendRequests(core_.ListFriendRequests());
  }
  emit tokenChanged();
  emit userChanged();
  return ok;
}

void QuickClient::logout() {
  StopPolling();
  core_.Logout();
  token_.clear();
  username_.clear();
  friends_.clear();
  groups_.clear();
  friend_requests_.clear();
  media_session_.reset();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  emit tokenChanged();
  emit userChanged();
  emit friendsChanged();
  emit groupsChanged();
  emit friendRequestsChanged();
  emit callStateChanged();
  emit status(QStringLiteral("已登出"));
}

bool QuickClient::joinGroup(const QString& groupId) {
  const QString trimmed = groupId.trimmed();
  const bool ok = core_.JoinGroup(trimmed.toStdString());
  if (ok) {
    AddGroupIfMissing(trimmed);
    emit groupsChanged();
  }
  emit status(ok ? QStringLiteral("加入群成功") : QStringLiteral("加入群失败"));
  return ok;
}

QString QuickClient::createGroup() {
  std::string out_id;
  if (!core_.CreateGroup(out_id)) {
    emit status(QStringLiteral("创建群失败"));
    return {};
  }
  const QString group_id = QString::fromStdString(out_id);
  AddGroupIfMissing(group_id);
  emit groupsChanged();
  emit status(QStringLiteral("已创建群"));
  return group_id;
}

bool QuickClient::sendText(const QString& convId, const QString& text, bool isGroup) {
  const QString trimmed = convId.trimmed();
  const QString message = text.trimmed();
  if (trimmed.isEmpty() || message.isEmpty()) {
    return false;
  }
  std::string msg_id;
  bool ok = false;
  if (isGroup) {
    ok = core_.SendGroupChatText(trimmed.toStdString(), message.toStdString(),
                                 msg_id);
  } else {
    ok = core_.SendChatText(trimmed.toStdString(), message.toStdString(), msg_id);
  }
  if (!ok) {
    emit status(QStringLiteral("发送失败"));
    return false;
  }

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
  msg.insert(QStringLiteral("text"), message);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendFile(const QString& convId, const QString& path, bool isGroup) {
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty() || path.trimmed().isEmpty()) {
    return false;
  }
  QString resolved = path;
  if (resolved.startsWith(QStringLiteral("file:"))) {
    resolved = QUrl(resolved).toLocalFile();
  }
  QFileInfo info(resolved);
  if (!info.exists() || !info.isFile()) {
    emit status(QStringLiteral("文件不存在"));
    return false;
  }
  std::string msg_id;
  bool ok = false;
  if (isGroup) {
    ok = core_.SendGroupChatFile(trimmed.toStdString(),
                                 info.absoluteFilePath().toStdString(), msg_id);
  } else {
    ok = core_.SendChatFile(trimmed.toStdString(),
                            info.absoluteFilePath().toStdString(), msg_id);
  }
  if (!ok) {
    emit status(QStringLiteral("文件发送失败"));
    return false;
  }

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
  msg.insert(QStringLiteral("fileName"), info.fileName());
  msg.insert(QStringLiteral("fileSize"), info.size());
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendSticker(const QString& convId,
                              const QString& stickerId,
                              bool isGroup) {
  const QString trimmed = convId.trimmed();
  const QString sid = stickerId.trimmed();
  if (trimmed.isEmpty() || sid.isEmpty()) {
    return false;
  }
  if (isGroup) {
    emit status(QStringLiteral("群聊暂不支持贴纸"));
    return false;
  }

  std::string msg_id;
  const bool ok =
      core_.SendChatSticker(trimmed.toStdString(), sid.toStdString(), msg_id);
  if (!ok) {
    emit status(QStringLiteral("贴纸发送失败"));
    return false;
  }

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
  msg.insert(QStringLiteral("stickerId"), sid);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  const auto meta = BuildStickerMeta(sid);
  msg.insert(QStringLiteral("stickerUrl"), meta.value(QStringLiteral("stickerUrl")));
  msg.insert(QStringLiteral("stickerAnimated"), meta.value(QStringLiteral("stickerAnimated")));
  EmitMessage(msg);
  return true;
}

QVariantList QuickClient::loadHistory(const QString& convId, bool isGroup) {
  QVariantList out;
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty()) {
    return out;
  }
  const auto entries =
      core_.LoadChatHistory(trimmed.toStdString(), isGroup, 200);
  out.reserve(static_cast<int>(entries.size()));
  for (const auto& entry : entries) {
    out.push_back(BuildHistoryMessage(entry));
  }
  return out;
}

QVariantList QuickClient::stickerItems() {
  QVariantList out;
  const auto items = EmojiPackManager::Instance().Items();
  out.reserve(items.size());
  for (const auto& item : items) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), item.id);
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("animated"), item.animated);
    map.insert(QStringLiteral("path"), QUrl::fromLocalFile(item.filePath));
    out.push_back(map);
  }
  return out;
}

bool QuickClient::sendFriendRequest(const QString& targetUsername,
                                    const QString& remark) {
  const QString target = targetUsername.trimmed();
  if (target.isEmpty()) {
    return false;
  }
  const bool ok = core_.SendFriendRequest(target.toStdString(), remark.toStdString());
  emit status(ok ? QStringLiteral("好友请求已发送") : QStringLiteral("好友请求失败"));
  return ok;
}

bool QuickClient::respondFriendRequest(const QString& requesterUsername,
                                       bool accept) {
  const QString requester = requesterUsername.trimmed();
  if (requester.isEmpty()) {
    return false;
  }
  const bool ok = core_.RespondFriendRequest(requester.toStdString(), accept);
  emit status(ok ? QStringLiteral("好友请求已处理") : QStringLiteral("好友请求处理失败"));
  if (ok) {
    UpdateFriendRequests(core_.ListFriendRequests());
  }
  return ok;
}

QString QuickClient::startVoiceCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  std::array<std::uint8_t, 16> call_id{};
  for (auto& b : call_id) {
    b = static_cast<std::uint8_t>(QRandomGenerator::global()->generate() & 0xFF);
  }
  const QString call_hex = BytesToHex(call_id);
  QString err;
  if (!InitMediaSession(peer, call_hex, true, false, err)) {
    emit status(err.isEmpty() ? QStringLiteral("语音通话初始化失败") : err);
    return {};
  }

  const QString invite =
      QString::fromLatin1(kCallVoicePrefix) + call_hex;
  std::string msg_id;
  core_.SendChatText(peer.toStdString(), invite.toStdString(), msg_id);

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), false);
  msg.insert(QStringLiteral("time"), NowTimeString());
  EmitMessage(msg);
  emit status(QStringLiteral("语音通话已发起"));
  return call_hex;
}

QString QuickClient::startVideoCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  std::array<std::uint8_t, 16> call_id{};
  for (auto& b : call_id) {
    b = static_cast<std::uint8_t>(QRandomGenerator::global()->generate() & 0xFF);
  }
  const QString call_hex = BytesToHex(call_id);
  QString err;
  if (!InitMediaSession(peer, call_hex, true, true, err)) {
    emit status(err.isEmpty() ? QStringLiteral("视频通话初始化失败") : err);
    return {};
  }

  const QString invite =
      QString::fromLatin1(kCallVideoPrefix) + call_hex;
  std::string msg_id;
  core_.SendChatText(peer.toStdString(), invite.toStdString(), msg_id);

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), true);
  msg.insert(QStringLiteral("time"), NowTimeString());
  EmitMessage(msg);
  emit status(QStringLiteral("视频通话已发起"));
  return call_hex;
}

bool QuickClient::joinCall(const QString& peerUsername,
                           const QString& callIdHex,
                           bool video) {
  QString err;
  if (!InitMediaSession(peerUsername, callIdHex, false, video, err)) {
    emit status(err.isEmpty() ? QStringLiteral("加入通话失败") : err);
    return false;
  }
  emit status(QStringLiteral("已加入通话"));
  return true;
}

void QuickClient::endCall() {
  media_session_.reset();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  emit callStateChanged();
  emit status(QStringLiteral("通话已结束"));
}

QString QuickClient::serverInfo() const {
  return QStringLiteral("config: %1").arg(config_path_);
}

QString QuickClient::version() const {
  return QStringLiteral("UI QML 1.0");
}

QString QuickClient::token() const {
  return token_;
}

bool QuickClient::loggedIn() const {
  return !token_.isEmpty();
}

QString QuickClient::username() const {
  return username_;
}

QVariantList QuickClient::friends() const {
  return friends_;
}

QVariantList QuickClient::groups() const {
  return groups_;
}

QVariantList QuickClient::friendRequests() const {
  return friend_requests_;
}

void QuickClient::StartPolling() {
  if (!poll_timer_.isActive()) {
    last_friend_sync_ms_ = 0;
    last_request_sync_ms_ = 0;
    last_heartbeat_ms_ = 0;
    poll_timer_.start();
  }
}

void QuickClient::StopPolling() {
  if (poll_timer_.isActive()) {
    poll_timer_.stop();
  }
}

void QuickClient::PollOnce() {
  if (!loggedIn()) {
    return;
  }
  HandlePollResult(core_.PollChat());

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - last_friend_sync_ms_ > 2000) {
    std::vector<ClientCore::FriendEntry> out;
    bool changed = false;
    if (core_.SyncFriends(out, changed) && changed) {
      UpdateFriendList(out);
    }
    last_friend_sync_ms_ = now;
  }
  if (now - last_request_sync_ms_ > 4000) {
    UpdateFriendRequests(core_.ListFriendRequests());
    last_request_sync_ms_ = now;
  }
  if (now - last_heartbeat_ms_ > 5000) {
    core_.Heartbeat();
    last_heartbeat_ms_ = now;
  }

  if (media_session_) {
    std::string err;
    media_session_->PollIncoming(32, 0, err);
  }
}

void QuickClient::EmitMessage(const QVariantMap& message) {
  emit messageEvent(message);
}

void QuickClient::UpdateFriendList(
    const std::vector<ClientCore::FriendEntry>& friends) {
  QVariantList updated;
  updated.reserve(static_cast<int>(friends.size()));
  for (const auto& entry : friends) {
    QVariantMap map;
    map.insert(QStringLiteral("username"),
               QString::fromStdString(entry.username));
    map.insert(QStringLiteral("remark"), QString::fromStdString(entry.remark));
    updated.push_back(map);
  }
  friends_ = updated;
  emit friendsChanged();
}

void QuickClient::UpdateFriendRequests(
    const std::vector<ClientCore::FriendRequestEntry>& requests) {
  QVariantList updated;
  updated.reserve(static_cast<int>(requests.size()));
  for (const auto& entry : requests) {
    QVariantMap map;
    map.insert(QStringLiteral("username"),
               QString::fromStdString(entry.requester_username));
    map.insert(QStringLiteral("remark"),
               QString::fromStdString(entry.requester_remark));
    updated.push_back(map);
  }
  friend_requests_ = updated;
  emit friendRequestsChanged();
}

void QuickClient::AddGroupIfMissing(const QString& groupId) {
  for (const auto& entry : groups_) {
    const auto map = entry.toMap();
    if (map.value(QStringLiteral("id")).toString() == groupId) {
      return;
    }
  }
  QVariantMap map;
  map.insert(QStringLiteral("id"), groupId);
  map.insert(QStringLiteral("name"), groupId);
  map.insert(QStringLiteral("unread"), 0);
  groups_.push_back(map);
}

QVariantMap QuickClient::BuildStickerMeta(const QString& stickerId) const {
  QVariantMap meta;
  const auto* item = EmojiPackManager::Instance().Find(stickerId);
  if (!item) {
    return meta;
  }
  meta.insert(QStringLiteral("stickerId"), item->id);
  meta.insert(QStringLiteral("stickerTitle"), item->title);
  meta.insert(QStringLiteral("stickerAnimated"), item->animated);
  meta.insert(QStringLiteral("stickerUrl"),
              QUrl::fromLocalFile(item->filePath));
  return meta;
}

QVariantMap QuickClient::BuildHistoryMessage(
    const ClientCore::HistoryEntry& entry) const {
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), QString::fromStdString(entry.conv_id));
  msg.insert(QStringLiteral("sender"), QString::fromStdString(entry.sender));
  msg.insert(QStringLiteral("outgoing"), entry.outgoing);
  msg.insert(QStringLiteral("isGroup"), entry.is_group);
  msg.insert(QStringLiteral("messageId"),
             QString::fromStdString(entry.message_id_hex));
  msg.insert(QStringLiteral("time"),
             QDateTime::fromSecsSinceEpoch(
                 static_cast<qint64>(entry.timestamp_sec))
                 .toString(QStringLiteral("HH:mm:ss")));

  switch (entry.kind) {
    case ClientCore::HistoryKind::kText:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      msg.insert(QStringLiteral("text"), QString::fromStdString(entry.text_utf8));
      break;
    case ClientCore::HistoryKind::kFile:
      msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
      msg.insert(QStringLiteral("fileName"),
                 QString::fromStdString(entry.file_name));
      msg.insert(QStringLiteral("fileSize"),
                 static_cast<qint64>(entry.file_size));
      msg.insert(QStringLiteral("fileId"), QString::fromStdString(entry.file_id));
      break;
    case ClientCore::HistoryKind::kSticker: {
      msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
      const QString sid = QString::fromStdString(entry.sticker_id);
      msg.insert(QStringLiteral("stickerId"), sid);
      const auto meta = BuildStickerMeta(sid);
      msg.insert(QStringLiteral("stickerUrl"),
                 meta.value(QStringLiteral("stickerUrl")));
      msg.insert(QStringLiteral("stickerAnimated"),
                 meta.value(QStringLiteral("stickerAnimated")));
      break;
    }
    case ClientCore::HistoryKind::kSystem:
      msg.insert(QStringLiteral("kind"), QStringLiteral("system"));
      msg.insert(QStringLiteral("text"), QString::fromStdString(entry.text_utf8));
      break;
    default:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      break;
  }
  return msg;
}

void QuickClient::HandlePollResult(const ClientCore::ChatPollResult& result) {
  const QString now = NowTimeString();

  for (const auto& t : result.texts) {
    const QString text = QString::fromStdString(t.text_utf8);
    const auto invite = ParseCallInvite(text);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    if (invite.ok) {
      msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
      msg.insert(QStringLiteral("callId"), invite.callId);
      msg.insert(QStringLiteral("video"), invite.video);
    } else {
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      msg.insert(QStringLiteral("text"), text);
    }
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_texts) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& s : result.stickers) {
    const QString sid = QString::fromStdString(s.sticker_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(s.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(s.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
    msg.insert(QStringLiteral("stickerId"), sid);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(s.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    const auto meta = BuildStickerMeta(sid);
    msg.insert(QStringLiteral("stickerUrl"),
               meta.value(QStringLiteral("stickerUrl")));
    msg.insert(QStringLiteral("stickerAnimated"),
               meta.value(QStringLiteral("stickerAnimated")));
    EmitMessage(msg);
  }

  for (const auto& s : result.outgoing_stickers) {
    const QString sid = QString::fromStdString(s.sticker_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(s.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
    msg.insert(QStringLiteral("stickerId"), sid);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(s.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    const auto meta = BuildStickerMeta(sid);
    msg.insert(QStringLiteral("stickerUrl"),
               meta.value(QStringLiteral("stickerUrl")));
    msg.insert(QStringLiteral("stickerAnimated"),
               meta.value(QStringLiteral("stickerAnimated")));
    EmitMessage(msg);
  }

  for (const auto& f : result.files) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.outgoing_files) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(f.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& t : result.group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    AddGroupIfMissing(group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    AddGroupIfMissing(group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    AddGroupIfMissing(group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.outgoing_group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    AddGroupIfMissing(group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& inv : result.group_invites) {
    const QString group_id = QString::fromStdString(inv.group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(inv.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("group_invite"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(inv.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& n : result.group_notices) {
    const QString group_id = QString::fromStdString(n.group_id);
    AddGroupIfMissing(group_id);
    QString text;
    switch (n.kind) {
      case 1:
        text = QStringLiteral("%1 加入群聊").arg(
            QString::fromStdString(n.target_username));
        break;
      case 2:
        text = QStringLiteral("%1 离开群聊").arg(
            QString::fromStdString(n.target_username));
        break;
      case 3:
        text = QStringLiteral("%1 被移出群聊").arg(
            QString::fromStdString(n.target_username));
        break;
      case 4:
        text = QStringLiteral("%1 权限变更").arg(
            QString::fromStdString(n.target_username));
        break;
      default:
        text = QStringLiteral("群通知更新");
        break;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(n.actor_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("notice"));
    msg.insert(QStringLiteral("text"), text);
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }
}

bool QuickClient::InitMediaSession(const QString& peerUsername,
                                   const QString& callIdHex,
                                   bool initiator,
                                   bool video,
                                   QString& outError) {
  outError.clear();
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty() || callIdHex.trimmed().isEmpty()) {
    outError = QStringLiteral("通话参数无效");
    return false;
  }
  std::array<std::uint8_t, 16> call_id{};
  if (!HexToBytes16(callIdHex, call_id)) {
    outError = QStringLiteral("通话 ID 格式错误");
    return false;
  }
  mi::client::media::MediaSessionConfig cfg;
  cfg.peer_username = peer.toStdString();
  cfg.call_id = call_id;
  cfg.initiator = initiator;
  cfg.enable_audio = true;
  cfg.enable_video = video;

  auto session = std::make_unique<mi::client::media::MediaSession>(core_, cfg);
  std::string err;
  if (!session->Init(err)) {
    outError = err.empty() ? QStringLiteral("通话初始化失败")
                           : QString::fromStdString(err);
    return false;
  }
  media_session_ = std::move(session);
  active_call_id_ = callIdHex.trimmed();
  active_call_peer_ = peer;
  active_call_video_ = video;
  emit callStateChanged();
  return true;
}

QString QuickClient::BytesToHex(const std::array<std::uint8_t, 16>& bytes) {
  const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<int>(bytes.size()));
  return QString::fromLatin1(raw.toHex());
}

bool QuickClient::HexToBytes16(const QString& hex,
                               std::array<std::uint8_t, 16>& out) {
  const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
  if (raw.size() != static_cast<int>(out.size())) {
    return false;
  }
  std::memcpy(out.data(), raw.data(), out.size());
  return true;
}

}  // namespace mi::client::ui
