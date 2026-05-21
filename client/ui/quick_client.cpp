#include "quick_client.h"
#include "c_api_client.h"
#include "cpp_client_adapter.h"
#include "display_contract.h"
#include "media_transport_capi.h"
#include "protected_text_vm.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QByteArray>
#include <QBuffer>
#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QDataStream>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSettings>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QStringList>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>
#include <QVector>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "common/EmojiPackManager.h"
#include "common/ImePluginLoader.h"
#include "common/QrCodeGenerator.h"
#include "common/UiPathSecurity.h"
#include "common/UiRuntimePaths.h"
#include "platform_time.h"
#include "protocol.h"

namespace mi::client::ui {

#include "quick_client_helpers.inc"

QuickClient::QuickClient(QObject* parent) : QObject(parent) {
  poll_timer_.setInterval(500);
  poll_timer_.setTimerType(Qt::CoarseTimer);
  connect(&poll_timer_, &QTimer::timeout, this, &QuickClient::PollOnce);
  media_timer_.setInterval(20);
  media_timer_.setTimerType(Qt::PreciseTimer);
  connect(&media_timer_, &QTimer::timeout, this, &QuickClient::PumpMedia);
  local_video_sink_ = new QVideoSink(this);
  remote_video_sink_ = new QVideoSink(this);
  int ideal = QThread::idealThreadCount();
  if (ideal < 1) {
    ideal = 2;
  }
  cache_pool_.setMaxThreadCount(std::clamp(ideal, 4, 12));
  cache_pool_.setExpiryTimeout(30000);
}

QuickClient::~QuickClient() {
  cache_pool_.clear();
  cache_pool_.waitForDone();
  if (ime_session_) {
    ImePluginLoader::instance().destroySession(ime_session_);
    ime_session_ = nullptr;
  }
  StopMedia();
  StopPolling();
  media_transport_.reset();
  if (c_api_) {
    mi_client_logout(c_api_);
    mi_client_destroy(c_api_);
    c_api_ = nullptr;
  }
}

bool QuickClient::init(const QString& configPath) {
  const QString appRoot = UiRuntimePaths::AppRootDir();
  const QString baseDir =
      appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
  QString dataDir = resolve_ui_data_dir();
  if (dataDir.isEmpty()) {
    dataDir = QDir(baseDir).filePath(QStringLiteral("database"));
  }
  QDir().mkpath(dataDir);
#ifdef _WIN32
  QString aclError;
  if (!UiPathSecurity::HardenDataDirAcl(dataDir, &aclError)) {
    const QString msg =
        aclError.isEmpty() ? QStringLiteral("data dir acl harden failed")
                           : aclError;
    UpdateLastError(msg);
    emit status(msg);
    return false;
  }
#endif
  qputenv("MI_E2EE_DATA_DIR",
          QDir::toNativeSeparators(dataDir).toUtf8());
  ai_gpu_name_ = query_gpu_name();
  ai_gpu_series_ = parse_nvidia_series(ai_gpu_name_);
  ai_gpu_available_ = detect_ai_enhance_gpu_available();
  const AiEnhanceRecommendation rec =
      build_ai_enhance_recommendation(ai_gpu_series_, ai_gpu_available_);
  ai_rec_perf_scale_ = rec.perf_scale;
  ai_rec_quality_scale_ = rec.quality_scale;
  bool enabled = ai_enhance_enabled_;
  int quality = ai_rec_perf_scale_;
  bool x4Confirmed = ai_enhance_x4_confirmed_;
  load_ai_enhance_settings(ai_gpu_available_, rec, enabled, quality, x4Confirmed);
  ai_enhance_enabled_ = enabled;
  ai_enhance_quality_ = quality;
  ai_enhance_x4_confirmed_ = x4Confirmed;
  if (!configPath.isEmpty()) {
    config_path_ = configPath;
  } else {
    config_path_ = find_config_file(QStringLiteral("config/client_config.ini"));
    if (config_path_.isEmpty()) {
      config_path_ = find_config_file(QStringLiteral("client_config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = find_config_file(QStringLiteral("config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = baseDir + QStringLiteral("/config/client_config.ini");
    }
  }
  if (c_api_) {
    mi_client_destroy(c_api_);
    c_api_ = nullptr;
  }
  c_api_ = mi_client_create(config_path_.toStdString().c_str());
  const bool ok = c_api_ != nullptr;
  if (!ok) {
    const char* err = mi_client_last_create_error();
    const QString msg = err ? QString::fromUtf8(err) : QString();
    UpdateLastError(msg.isEmpty() ? QStringLiteral("初始化失败") : msg);
    emit status(QStringLiteral("初始化失败"));
  } else {
    UpdateLastError(QString());
    emit deviceChanged();
    StopMedia();
    ResetMediaTransport();
  }
  qr_login_active_ = false;
  qr_login_payload_.clear();
  ClearQrLoginCache();
  emit qrLoginChanged();
  bool historySave = history_save_enabled_;
  load_privacy_settings(historySave);
  history_save_enabled_ = historySave;
  if (c_api_) {
    mi_client_set_history_enabled(c_api_, history_save_enabled_ ? 1 : 0);
  }
  load_chat_backgrounds(chat_backgrounds_);
  return ok;
}

bool QuickClient::registerUser(const QString& user, const QString& pass) {
  const QString account = user.trimmed();
  if (account.isEmpty() || pass.isEmpty()) {
    UpdateLastError(QStringLiteral("账号或密码为空"));
    emit status(QStringLiteral("注册失败"));
    return false;
  }
  bool ok = false;
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(QStringLiteral("注册失败"));
    return false;
  }
  ok = mi_client_register(c_api_, account.toStdString().c_str(),
                          pass.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    const QString msg = err ? QString::fromUtf8(err) : QString();
    UpdateLastError(msg.isEmpty() ? QStringLiteral("注册失败") : msg);
    emit status(QStringLiteral("注册失败"));
  } else {
    UpdateLastError(QString());
    emit status(QStringLiteral("注册成功"));
  }
  MaybeEmitTrustSignals();
  return ok;
}

bool QuickClient::login(const QString& user, const QString& pass) {
  return loginWithRootCode(user, pass, QString());
}

bool QuickClient::loginWithRootCode(const QString& user,
                                    const QString& pass,
                                    const QString& rootCode) {
  bool ok = false;
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(QStringLiteral("登录失败"));
    return false;
  }
  const std::string account = user.trimmed().toStdString();
  const std::string password = pass.toStdString();
  const std::string root = rootCode.trimmed().toStdString();
  if (root.empty()) {
    ok = mi_client_login(c_api_, account.c_str(), password.c_str()) != 0;
  } else {
    ok = mi_client_login_with_root_code(c_api_, account.c_str(),
                                        password.c_str(), root.c_str()) != 0;
  }
  if (!ok) {
    emit status(QStringLiteral("登录失败"));
    logged_in_ = false;
    username_.clear();
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    StopPolling();
  } else {
    HandleLoginSuccess(QString::fromStdString(account));
  }
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();
  emit authStateChanged();
  emit userChanged();
  return ok;
}

bool QuickClient::beginQrLogin(const QString& username) {
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  if (logged_in_) {
    UpdateLastError(QStringLiteral("已登录"));
    return false;
  }
  const QByteArray user = username.trimmed().toUtf8();
  const char* user_ptr = user.isEmpty() ? nullptr : user.constData();
  char* out_payload = nullptr;
  const bool ok =
      mi_client_begin_qr_login_with_username(c_api_, user_ptr, &out_payload) != 0;
  if (out_payload) {
    qr_login_payload_ = QString::fromUtf8(out_payload);
    mi_client_free(out_payload);
  } else {
    qr_login_payload_.clear();
  }
  const char* err = mi_client_last_error(c_api_);
  const QString errMsg = err ? QString::fromUtf8(err) : QString();
  if (!ok) {
    qr_login_active_ = false;
    qr_login_payload_.clear();
    ClearQrLoginCache();
    emit qrLoginChanged();
    UpdateLastError(errMsg.isEmpty() ? QStringLiteral("生成二维码失败") : errMsg);
    return false;
  }
  qr_login_active_ = true;
  ClearQrLoginCache();
  emit qrLoginChanged();
  UpdateLastError(QString());
  return true;
}

bool QuickClient::pollQrLogin() {
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  int completed = 0;
  char* out_user = nullptr;
  const bool ok = mi_client_poll_qr_login(c_api_, &completed, &out_user) != 0;
  QString user;
  if (out_user) {
    user = QString::fromUtf8(out_user);
    mi_client_free(out_user);
  }
  const char* err = mi_client_last_error(c_api_);
  const QString errMsg = err ? QString::fromUtf8(err) : QString();
  if (!ok) {
    UpdateLastError(errMsg.isEmpty() ? QStringLiteral("二维码登录失败") : errMsg);
    qr_login_active_ = false;
    qr_login_payload_.clear();
    ClearQrLoginCache();
    emit qrLoginChanged();
    UpdateConnectionState(true);
    MaybeEmitTrustSignals();
    return false;
  }
  UpdateLastError(QString());
  if (completed) {
    qr_login_active_ = false;
    qr_login_payload_.clear();
    ClearQrLoginCache();
    emit qrLoginChanged();
    if (user.isEmpty()) {
      user = username_;
    }
    HandleLoginSuccess(user);
    UpdateConnectionState(true);
    MaybeEmitTrustSignals();
    emit authStateChanged();
    emit userChanged();
  } else {
    UpdateConnectionState(false);
    MaybeEmitTrustSignals();
  }
  return true;
}

void QuickClient::cancelQrLogin() {
  if (c_api_) {
    mi_client_cancel_qr_login(c_api_);
  }
  qr_login_active_ = false;
  qr_login_payload_.clear();
  ClearQrLoginCache();
  emit qrLoginChanged();
}

QString QuickClient::qrLoginImage(int size) {
  if (qr_login_payload_.isEmpty() || size <= 0) {
    return {};
  }
  const int safeSize = std::max(64, std::min(size, 512));
  auto it = qr_login_image_cache_.find(safeSize);
  if (it != qr_login_image_cache_.end()) {
    return it.value();
  }
  const QImage img = mi::ui::BuildQrImage(qr_login_payload_, safeSize, 2);
  if (img.isNull()) {
    return {};
  }
  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  if (!img.save(&buffer, "PNG")) {
    return {};
  }
  const QString url =
      QStringLiteral("data:image/png;base64,") +
      QString::fromLatin1(png.toBase64());
  qr_login_image_cache_.insert(safeSize, url);
  return url;
}

void QuickClient::logout() {
  StopPolling();
  StopMedia();
  if (c_api_) {
    mi_client_logout(c_api_);
  }
  qr_login_active_ = false;
  qr_login_payload_.clear();
  ClearQrLoginCache();
  emit qrLoginChanged();
  logged_in_ = false;
  username_.clear();
  UpdateLastError(QString());
  friends_.clear();
  groups_.clear();
  friend_requests_.clear();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  group_call_rooms_map_.clear();
  group_call_media_flags_.clear();
  group_call_rooms_.clear();
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();
  emit authStateChanged();
  emit userChanged();
  emit friendsChanged();
  emit groupsChanged();
  emit friendRequestsChanged();
  emit callStateChanged();
  emit groupCallStateChanged();
  emit groupCallParticipantsChanged();
  emit groupCallRoomsChanged();
  emit status(QStringLiteral("已登出"));
}

bool QuickClient::joinGroup(const QString& groupId) {
  const QString trimmed = groupId.trimmed();
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(QStringLiteral("加入群失败"));
    return false;
  }
  const bool ok =
      mi_client_join_group(c_api_, trimmed.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  }
  if (ok) {
    if (AddGroupIfMissing(trimmed)) {
      emit groupsChanged();
    }
    UpdateLastError(QString());
  }
  emit status(ok ? QStringLiteral("加入群成功") : QStringLiteral("加入群失败"));
  return ok;
}

QString QuickClient::createGroup() {
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(QStringLiteral("创建群失败"));
    return {};
  }
  std::string out_id;
  char* out_group = nullptr;
  const bool ok = mi_client_create_group(c_api_, &out_group) != 0;
  if (out_group) {
    out_id.assign(out_group);
    mi_client_free(out_group);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    emit status(QStringLiteral("创建群失败"));
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return {};
  }
  const QString group_id = QString::fromStdString(out_id);
  if (AddGroupIfMissing(group_id)) {
    emit groupsChanged();
  }
  emit status(QStringLiteral("已创建群"));
  UpdateLastError(QString());
  return group_id;
}

bool QuickClient::sendGroupInvite(const QString& groupId,
                                  const QString& peerUsername) {
  const QString gid = groupId.trimmed();
  const QString peer = peerUsername.trimmed();
  if (gid.isEmpty() || peer.isEmpty()) {
    UpdateLastError(QStringLiteral("群或成员为空"));
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(QStringLiteral("邀请失败"));
    return false;
  }
  std::string msg_id;
  char* out_id = nullptr;
  const bool ok =
      mi_client_send_group_invite(c_api_, gid.toStdString().c_str(),
                                  peer.toStdString().c_str(), &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(QStringLiteral("邀请失败"));
    return false;
  }
  UpdateLastError(QString());
  emit status(QStringLiteral("邀请已发送"));
  return true;
}

bool QuickClient::sendText(const QString& convId, const QString& text, bool isGroup) {
  const QString trimmed = convId.trimmed();
  const QString message = text.trimmed();
  if (trimmed.isEmpty() || message.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  std::string msg_id;
  bool ok = false;
  char* out_id = nullptr;
  if (isGroup) {
    ok = mi_client_send_group_text(c_api_, trimmed.toStdString().c_str(),
                                   message.toStdString().c_str(), &out_id) != 0;
  } else {
    ok = mi_client_send_private_text(c_api_, trimmed.toStdString().c_str(),
                                     message.toStdString().c_str(), &out_id) != 0;
  }
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    emit status(QStringLiteral("发送失败"));
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
  InsertProtectedUiText(msg, message);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendFile(const QString& convId, const QString& path, bool isGroup) {
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty() || path.trimmed().isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
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
  char* out_id = nullptr;
  const QByteArray pathUtf8 = info.absoluteFilePath().toUtf8();
  if (isGroup) {
    ok = mi_client_send_group_file(c_api_, trimmed.toStdString().c_str(),
                                   pathUtf8.constData(), &out_id) != 0;
  } else {
    ok = mi_client_send_private_file(c_api_, trimmed.toStdString().c_str(),
                                     pathUtf8.constData(), &out_id) != 0;
  }
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    emit status(QStringLiteral("文件发送失败"));
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
  msg.insert(QStringLiteral("fileName"), info.fileName());
  msg.insert(QStringLiteral("fileSize"), info.size());
  msg.insert(QStringLiteral("filePath"), info.absoluteFilePath());
  msg.insert(QStringLiteral("fileUrl"),
             QUrl::fromLocalFile(info.absoluteFilePath()).toString());
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  MaybeAutoEnhanceImage(QString::fromStdString(msg_id),
                        info.absoluteFilePath(),
                        info.fileName());
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
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }

  std::string msg_id;
  bool ok = false;
  char* out_id = nullptr;
  ok = mi_client_send_private_sticker(c_api_, trimmed.toStdString().c_str(),
                                      sid.toStdString().c_str(), &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    emit status(QStringLiteral("贴纸发送失败"));
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
  msg.insert(QStringLiteral("stickerId"), sid);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  const auto meta = BuildStickerMeta(sid);
  msg.insert(QStringLiteral("stickerUrl"), meta.value(QStringLiteral("stickerUrl")));
  msg.insert(QStringLiteral("stickerAnimated"), meta.value(QStringLiteral("stickerAnimated")));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendLocation(const QString& convId,
                               double lat,
                               double lon,
                               const QString& label,
                               bool isGroup) {
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  if (isGroup) {
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
      emit status(QStringLiteral("位置参数无效"));
      UpdateLastError(QStringLiteral("位置参数无效"));
      return false;
    }
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
      emit status(QStringLiteral("位置超出范围"));
      UpdateLastError(QStringLiteral("位置超出范围"));
      return false;
    }
    const QString text = format_location_text(lat, lon, label);
    std::string msg_id;
    bool ok = false;
    char* out_id = nullptr;
    ok = mi_client_send_group_text(c_api_, trimmed.toStdString().c_str(),
                                   text.toStdString().c_str(), &out_id) != 0;
    if (out_id) {
      msg_id.assign(out_id);
      mi_client_free(out_id);
    }
    if (!ok) {
      const char* err = mi_client_last_error(c_api_);
      emit status(QStringLiteral("位置发送失败"));
      UpdateLastError(err ? QString::fromUtf8(err) : QString());
      return false;
    }
    UpdateLastError(QString());
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), trimmed);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("location"));
    msg.insert(QStringLiteral("locationLabel"), label);
    msg.insert(QStringLiteral("locationLat"), lat);
    msg.insert(QStringLiteral("locationLon"), lon);
    InsertProtectedUiText(msg, text);
    msg.insert(QStringLiteral("time"), now_time_string());
    msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
    EmitMessage(msg);
    return true;
  }
  if (!std::isfinite(lat) || !std::isfinite(lon)) {
    emit status(QStringLiteral("位置参数无效"));
    UpdateLastError(QStringLiteral("位置参数无效"));
    return false;
  }
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    emit status(QStringLiteral("位置超出范围"));
    UpdateLastError(QStringLiteral("位置超出范围"));
    return false;
  }
  const auto lat_e7 = static_cast<std::int32_t>(std::llround(lat * 10000000.0));
  const auto lon_e7 = static_cast<std::int32_t>(std::llround(lon * 10000000.0));
  std::string msg_id;
  bool ok = false;
  char* out_id = nullptr;
  ok = mi_client_send_private_location(c_api_, trimmed.toStdString().c_str(),
                                       lat_e7, lon_e7,
                                       label.toStdString().c_str(),
                                       &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    emit status(QStringLiteral("位置发送失败"));
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("location"));
  msg.insert(QStringLiteral("locationLabel"), label);
  msg.insert(QStringLiteral("locationLat"), lat);
  msg.insert(QStringLiteral("locationLon"), lon);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendContactCard(const QString& convId,
                                  const QString& cardUsername,
                                  const QString& cardDisplay) {
  const QString target = convId.trimmed();
  const QString card_user = cardUsername.trimmed();
  const QString card_name = cardDisplay.trimmed();
  if (target.isEmpty() || card_user.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }

  std::string msg_id;
  char* out_id = nullptr;
  const bool ok =
      mi_client_send_private_contact(c_api_, target.toStdString().c_str(),
                                     card_user.toStdString().c_str(),
                                     card_name.toStdString().c_str(),
                                     &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    emit status(QStringLiteral("名片发送失败"));
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), target);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("contact"));
  msg.insert(QStringLiteral("contactUsername"), card_user);
  msg.insert(QStringLiteral("contactDisplay"), card_name);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::recallMessage(const QString& convId,
                                const QString& messageId,
                                bool isGroup) {
  const QString target = convId.trimmed();
  const QString msgId = messageId.trimmed();
  if (target.isEmpty() || msgId.isEmpty()) {
    UpdateLastError(QStringLiteral("撤回参数无效"));
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  const QString payload =
      QString::fromLatin1(kRecallPrefix) + msgId;
  std::string out_id;
  bool ok = false;
  char* out_msg = nullptr;
  if (isGroup) {
    ok = mi_client_send_group_text(c_api_, target.toStdString().c_str(),
                                   payload.toStdString().c_str(),
                                   &out_msg) != 0;
  } else {
    ok = mi_client_send_private_text(c_api_, target.toStdString().c_str(),
                                     payload.toStdString().c_str(),
                                     &out_msg) != 0;
  }
  if (out_msg) {
    out_id.assign(out_msg);
    mi_client_free(out_msg);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  }
  return ok;
}

QVariantMap QuickClient::ensureAttachmentCached(const QString& fileId,
                                                const QString& fileKeyHex,
                                                const QString& fileName,
                                                qint64 fileSize) {
  QVariantMap out;
  out.insert(QStringLiteral("ok"), false);
  const QString fid = fileId.trimmed();
  if (fid.isEmpty()) {
    out.insert(QStringLiteral("error"), QStringLiteral("file id empty"));
    return out;
  }
  std::array<std::uint8_t, 32> file_key{};
  if (!HexToBytes32(fileKeyHex.trimmed(), file_key)) {
    out.insert(QStringLiteral("error"), QStringLiteral("invalid file key"));
    return out;
  }
  if (fileSize > 0 &&
      static_cast<quint64>(fileSize) > kMaxAttachmentCacheBytes) {
    out.insert(QStringLiteral("error"), QStringLiteral("file too large"));
    return out;
  }

  QDir cacheRoot;
  QString rootErr;
  if (!ensure_cache_root_dir(cacheRoot, rootErr)) {
    out.insert(QStringLiteral("error"), rootErr);
    return out;
  }

  const QString safeId = sanitize_file_id(fid);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = is_image_ext(ext) || is_gif_ext(ext) || is_video_ext(ext);

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    const QString previewPath =
        cacheRoot.filePath(safeId + QStringLiteral(".preview.jpg"));
    if (QFileInfo::exists(filePath)) {
      out.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(filePath));
      if (is_video_ext(ext)) {
        if (QFileInfo::exists(previewPath)) {
          out.insert(QStringLiteral("previewUrl"),
                     QUrl::fromLocalFile(previewPath));
        } else if (!cache_inflight_.contains(fid)) {
          cache_inflight_.insert(fid);
          QueueAttachmentCacheTask(fid, file_key, fileName, fileSize, false);
        }
      } else {
        out.insert(QStringLiteral("previewUrl"), QUrl::fromLocalFile(filePath));
      }
      out.insert(QStringLiteral("ok"), true);
      return out;
    }
  } else {
    QDir fileDir(cacheRoot.filePath(safeId));
    const QString indexPath = cache_index_path(fileDir);
    CacheIndex existing;
    QString readErr;
    if (QFileInfo::exists(indexPath) &&
        read_cache_index(indexPath, existing, readErr) &&
        cache_chunks_ready(fileDir, existing)) {
      if ((existing.flags & kCacheFlagKeepRaw) && !existing.rawName.isEmpty()) {
        const QString rawPath = fileDir.filePath(existing.rawName);
        if (QFileInfo::exists(rawPath)) {
          out.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(rawPath));
        }
      }
      out.insert(QStringLiteral("ok"), true);
      return out;
    }
  }

  if (!cache_inflight_.contains(fid)) {
    cache_inflight_.insert(fid);
    QueueAttachmentCacheTask(fid, file_key, fileName, fileSize, false);
  }
  out.insert(QStringLiteral("pending"), true);
  return out;
}

bool QuickClient::requestAttachmentDownload(const QString& fileId,
                                            const QString& fileKeyHex,
                                            const QString& fileName,
                                            qint64 fileSize,
                                            const QString& savePath) {
  const QString fid = fileId.trimmed();
  if (fid.isEmpty()) {
    UpdateLastError(QStringLiteral("file id empty"));
    return false;
  }
  std::array<std::uint8_t, 32> file_key{};
  if (!HexToBytes32(fileKeyHex.trimmed(), file_key)) {
    UpdateLastError(QStringLiteral("invalid file key"));
    return false;
  }
  QString resolved = savePath.trimmed();
  if (resolved.startsWith(QStringLiteral("file:"))) {
    resolved = QUrl(resolved).toLocalFile();
  }
  if (resolved.isEmpty()) {
    UpdateLastError(QStringLiteral("save path empty"));
    return false;
  }
  QFileInfo destInfo(resolved);
  if (destInfo.isDir() || resolved.endsWith(QLatin1Char('/')) ||
      resolved.endsWith(QLatin1Char('\\'))) {
    const QString fallbackName = sanitize_download_file_name(
        fileName, sanitize_file_id(fid) + QStringLiteral(".bin"));
    resolved = QDir(resolved).filePath(fallbackName);
    destInfo = QFileInfo(resolved);
  }
  QDir destDir = destInfo.dir();
  if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
    UpdateLastError(QStringLiteral("save path invalid"));
    return false;
  }
  if (fileSize > 0 &&
      static_cast<quint64>(fileSize) > kMaxAttachmentCacheBytes) {
    UpdateLastError(QStringLiteral("file too large"));
    return false;
  }

  const QString safeId = sanitize_file_id(fid);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const QString effectiveName = sanitize_download_file_name(
      fileName, safeId + QStringLiteral(".") + ext);

  QDir cacheRoot;
  QString rootErr;
  if (!ensure_cache_root_dir(cacheRoot, rootErr)) {
    UpdateLastError(rootErr);
    return false;
  }
  const bool isMedia = is_image_ext(ext) || is_gif_ext(ext) || is_video_ext(ext);
  bool cacheReady = false;

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    if (QFileInfo::exists(filePath)) {
      cacheReady = true;
    }
  } else {
    const QDir fileDir(cacheRoot.filePath(safeId));
    const QString indexPath = cache_index_path(fileDir);
    CacheIndex existing;
    QString readErr;
    if (QFileInfo::exists(indexPath) &&
        read_cache_index(indexPath, existing, readErr) &&
        cache_chunks_ready(fileDir, existing)) {
      cacheReady = true;
    }
  }

  download_progress_base_.insert(fid, 0.0);
  download_progress_span_.insert(fid, cacheReady ? 1.0 : 0.9);
  EmitDownloadProgress(fid, resolved, 0.0);

  if (cacheReady) {
    QueueAttachmentRestoreTask(fid, effectiveName, resolved, true);
    return true;
  }

  pending_downloads_[fid].append(resolved);
  if (!pending_download_names_.contains(fid) ||
      pending_download_names_.value(fid).isEmpty()) {
    pending_download_names_.insert(fid, effectiveName);
  }
  if (!cache_inflight_.contains(fid)) {
    cache_inflight_.insert(fid);
    QueueAttachmentCacheTask(fid, file_key, effectiveName, fileSize, true);
  }
  return true;
}

bool QuickClient::requestImageEnhance(const QString& fileUrl,
                                      const QString& fileName) {
  return requestImageEnhanceForMessage(QString(), fileUrl, fileName);
}

bool QuickClient::requestImageEnhanceForMessage(const QString& messageId,
                                                const QString& fileUrl,
                                                const QString& fileName) {
  if (!ai_enhance_enabled_) {
    UpdateLastError(QStringLiteral("AI超清已关闭"));
    return false;
  }
  const QString sourceUrl = fileUrl.trimmed();
  const QString sourcePath = resolve_local_file_path(sourceUrl);
  if (sourcePath.isEmpty()) {
    UpdateLastError(QStringLiteral("图片路径为空"));
    return false;
  }
  const QFileInfo sourceInfo(sourcePath);
  if (!sourceInfo.exists() || !sourceInfo.isFile()) {
    UpdateLastError(QStringLiteral("图片不存在"));
    return false;
  }
  if (!is_image_ext(sourceInfo.suffix())) {
    UpdateLastError(QStringLiteral("仅支持图片优化"));
    return false;
  }

  const QString trimmedMsg = messageId.trimmed();
  const QString inflightKey = trimmedMsg.isEmpty() ? sourcePath : trimmedMsg;
  if (!inflightKey.isEmpty() && enhance_inflight_.contains(inflightKey)) {
    return true;
  }
  if (!trimmedMsg.isEmpty()) {
    const QString existing = enhanced_image_path_if_exists(trimmedMsg);
    if (!existing.isEmpty()) {
      const QString outputUrl = QUrl::fromLocalFile(existing).toString();
      emit imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, true,
                                QString());
      UpdateLastError(QString());
      return true;
    }
  }

  QString outPath;
  QString pathError;
  const int scale =
      resolve_enhance_scale(ai_enhance_quality_, ai_enhance_x4_confirmed_);
  if (!trimmedMsg.isEmpty()) {
    outPath = build_enhanced_image_path(trimmedMsg, scale, pathError);
  } else {
    QDir outDir;
    if (!ensure_ai_upscale_dir(outDir, pathError)) {
      UpdateLastError(pathError);
      return false;
    }
    const QString stem = sanitize_file_stem(
        fileName.trimmed().isEmpty() ? sourceInfo.fileName() : fileName);
    const QString suffix = QStringLiteral("_x%1").arg(scale);
    outPath = outDir.filePath(stem + suffix + QStringLiteral(".png"));
    if (QFileInfo::exists(outPath)) {
      int suffix = 2;
      while (suffix < 1000) {
        const QString candidate =
            outDir.filePath(stem + QStringLiteral("_x%1_").arg(scale) +
                            QString::number(suffix) +
                            QStringLiteral(".png"));
        if (!QFileInfo::exists(candidate)) {
          outPath = candidate;
          break;
        }
        ++suffix;
      }
    }
  }
  if (outPath.isEmpty()) {
    UpdateLastError(pathError.isEmpty() ? QStringLiteral("创建超清目录失败")
                                        : pathError);
    return false;
  }

  if (QFileInfo::exists(outPath)) {
    const QString outputUrl = QUrl::fromLocalFile(outPath).toString();
    emit imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, true, QString());
    UpdateLastError(QString());
    return true;
  }

  if (!inflightKey.isEmpty()) {
    enhance_inflight_.insert(inflightKey);
  }

  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, trimmedMsg, inflightKey, sourceUrl,
                              sourcePath, outPath, scale]() {
    if (!self) {
      return;
    }
    bool gpuSupported = false;
    const QString exe = find_real_esrgan_path(&gpuSupported);
    const ImageQualityMetrics metrics = analyze_image_quality(sourcePath);
    QString modelName = select_real_esrgan_model_name(scale, metrics.anime_like);
    QString modelDir = find_real_esrgan_model_dir(exe, modelName);
    if (modelDir.isEmpty() && metrics.anime_like) {
      modelName = select_real_esrgan_model_name(scale, false);
      modelDir = find_real_esrgan_model_dir(exe, modelName);
    }
    QString error;
    bool ok = false;
    QString outputUrl;

    if (exe.isEmpty()) {
      error = QStringLiteral("未找到超清工具");
    } else if (modelDir.isEmpty()) {
      error = QStringLiteral("未找到超清模型");
    } else {
      QStringList args;
      args << QStringLiteral("-i") << sourcePath
           << QStringLiteral("-o") << outPath
           << QStringLiteral("-n") << modelName
           << QStringLiteral("-s") << QString::number(scale)
           << QStringLiteral("-m") << modelDir;
      int exitCode = -1;
      if (gpuSupported) {
        QStringList gpuArgs = args;
        gpuArgs << QStringLiteral("-g") << QStringLiteral("0");
        exitCode = run_real_esrgan_quietly(exe, gpuArgs);
        if (exitCode != 0) {
          QStringList cpuArgs = args;
          cpuArgs << QStringLiteral("-g") << QStringLiteral("-1");
          exitCode = run_real_esrgan_quietly(exe, cpuArgs);
        }
      } else {
        exitCode = run_real_esrgan_quietly(exe, args);
      }

      if (exitCode == 0 && QFileInfo::exists(outPath)) {
        ok = true;
        outputUrl = QUrl::fromLocalFile(outPath).toString();
      } else {
        error = QStringLiteral("超清优化失败");
      }
    }

    QMetaObject::invokeMethod(
        self,
        [self, trimmedMsg, inflightKey, sourceUrl, outputUrl, ok, error]() {
          if (!self) {
            return;
          }
          if (!inflightKey.isEmpty()) {
            self->enhance_inflight_.remove(inflightKey);
          }
          if (!ok) {
            self->UpdateLastError(error);
          }
          emit self->imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, ok,
                                          error);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, 0);
  UpdateLastError(QString());
  emit status(QStringLiteral("已提交超清优化"));
  return true;
}

void QuickClient::QueueAttachmentCacheTask(
    const QString& fileId,
    const std::array<std::uint8_t, 32>& fileKey,
    const QString& fileName,
    qint64 fileSize,
    bool highPriority) {
  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, fileId, fileKey, fileName, fileSize]() {
    if (!self) {
      return;
    }
    const CacheTaskResult result = build_attachment_cache(
        self->c_api_, fileId, fileKey, fileName, fileSize,
        [self, fileId](double progress) {
          if (!self) {
            return;
          }
          QMetaObject::invokeMethod(
              self,
              [self, fileId, progress]() {
                if (!self) {
                  return;
                }
                self->EmitDownloadProgress(fileId, QString(), progress);
              },
              Qt::QueuedConnection);
        });
    QMetaObject::invokeMethod(
        self,
        [self, fileId, result]() {
          if (!self) {
            return;
          }
          self->HandleCacheTaskFinished(
              fileId,
              result.fileUrl.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.fileUrl),
              result.previewUrl.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.previewUrl),
              result.error,
              result.ok);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, highPriority ? 1 : 0);
}

void QuickClient::QueueAttachmentRestoreTask(const QString& fileId,
                                             const QString& fileName,
                                             const QString& savePath,
                                             bool highPriority) {
  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, fileId, fileName, savePath]() {
    if (!self) {
      return;
    }
    QString error;
    const bool ok = restore_attachment_from_cache(
        fileId, fileName, savePath,
        [self, fileId, savePath](double progress) {
          if (!self) {
            return;
          }
          QMetaObject::invokeMethod(
              self,
              [self, fileId, savePath, progress]() {
                if (!self) {
                  return;
                }
                self->EmitDownloadProgress(fileId, savePath, progress);
              },
              Qt::QueuedConnection);
        },
        error);
    QMetaObject::invokeMethod(
        self,
        [self, fileId, savePath, ok, error]() {
          if (!self) {
            return;
          }
          self->HandleRestoreTaskFinished(fileId, savePath, ok, error);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, highPriority ? 1 : 0);
}

void QuickClient::HandleCacheTaskFinished(const QString& fileId,
                                          const QUrl& fileUrl,
                                          const QUrl& previewUrl,
                                          const QString& error,
                                          bool ok) {
  cache_inflight_.remove(fileId);
  emit attachmentCacheReady(fileId, fileUrl, previewUrl, error);
  if (!pending_downloads_.contains(fileId)) {
    download_progress_base_.remove(fileId);
    download_progress_span_.remove(fileId);
    return;
  }
  const QStringList paths = pending_downloads_.take(fileId);
  const QString name = pending_download_names_.take(fileId);
  if (!ok) {
    download_progress_base_.remove(fileId);
    download_progress_span_.remove(fileId);
    for (const auto& path : paths) {
      emit attachmentDownloadFinished(fileId, path, false, error);
    }
    return;
  }
  if (download_progress_span_.value(fileId, 1.0) < 1.0) {
    download_progress_base_.insert(fileId, 0.9);
    download_progress_span_.insert(fileId, 0.1);
  }
  for (const auto& path : paths) {
    QueueAttachmentRestoreTask(fileId, name, path, true);
  }
}

void QuickClient::HandleRestoreTaskFinished(const QString& fileId,
                                            const QString& savePath,
                                            bool ok,
                                            const QString& error) {
  if (!ok && !error.isEmpty()) {
    UpdateLastError(error);
  }
  emit attachmentDownloadFinished(fileId, savePath, ok, error);
  download_progress_base_.remove(fileId);
  download_progress_span_.remove(fileId);
}

void QuickClient::MaybeAutoEnhanceImage(const QString& messageId,
                                        const QString& filePath,
                                        const QString& fileName) {
  if (!ai_enhance_enabled_) {
    return;
  }
  const QString trimmedMsg = messageId.trimmed();
  if (trimmedMsg.isEmpty()) {
    return;
  }
  const QString trimmedPath = filePath.trimmed();
  if (trimmedPath.isEmpty()) {
    return;
  }
  const QFileInfo info(trimmedPath);
  if (!info.exists() || !info.isFile()) {
    return;
  }
  if (!is_image_ext(info.suffix())) {
    return;
  }
  if (!enhanced_image_path_if_exists(trimmedMsg).isEmpty()) {
    return;
  }

  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, trimmedMsg, trimmedPath, fileName]() {
    if (!self) {
      return;
    }
    const bool shouldEnhance = should_auto_enhance_image(trimmedPath);
    QMetaObject::invokeMethod(
        self,
        [self, trimmedMsg, trimmedPath, fileName, shouldEnhance]() {
          if (!self || !shouldEnhance) {
            return;
          }
          self->requestImageEnhanceForMessage(
              trimmedMsg,
              QUrl::fromLocalFile(trimmedPath).toString(),
              fileName);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, 0);
}

QVariantList QuickClient::loadHistory(const QString& convId, bool isGroup) {
  QVariantList out;
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty()) {
    return out;
  }
  if (!c_api_) {
    return out;
  }
  std::vector<mi_history_entry_t> buffer(kMaxHistoryEntries);
  const std::uint32_t count =
      mi_client_load_chat_history(c_api_, trimmed.toStdString().c_str(),
                                  isGroup ? 1 : 0, kMaxHistoryEntries,
                                  buffer.data(), kMaxHistoryEntries);
  out.reserve(static_cast<int>(count));
  for (std::uint32_t i = 0; i < count; ++i) {
    out.push_back(BuildHistoryMessageFromC(buffer[i]));
  }
  return out;
}

QVariantList QuickClient::listGroupMembersInfo(const QString& groupId) {
  QVariantList out;
  const QString gid = groupId.trimmed();
  if (gid.isEmpty()) {
    return out;
  }
  if (!c_api_) {
    return out;
  }
  std::vector<mi_group_member_entry_t> buffer(kMaxGroupMemberEntries);
  const std::uint32_t count =
      mi_client_list_group_members_info(c_api_, gid.toStdString().c_str(),
                                        buffer.data(), kMaxGroupMemberEntries);
  out.reserve(static_cast<int>(count));
  for (std::uint32_t i = 0; i < count; ++i) {
    QVariantMap map;
    if (buffer[i].username) {
      map.insert(QStringLiteral("username"),
                 QString::fromUtf8(buffer[i].username));
    } else {
      map.insert(QStringLiteral("username"), QString());
    }
    map.insert(QStringLiteral("role"),
               static_cast<int>(buffer[i].role));
    out.push_back(map);
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

QVariantMap QuickClient::importSticker(const QString& path) {
  QVariantMap out;
  QString id;
  QString err;
  const bool ok = EmojiPackManager::Instance().ImportSticker(path, &id, &err);
  out.insert(QStringLiteral("ok"), ok);
  if (ok) {
    out.insert(QStringLiteral("stickerId"), id);
    emit status(QStringLiteral("贴纸已导入"));
  } else {
    out.insert(QStringLiteral("error"),
               err.isEmpty() ? QStringLiteral("贴纸导入失败") : err);
    emit status(err.isEmpty() ? QStringLiteral("贴纸导入失败") : err);
  }
  return out;
}

bool QuickClient::sendFriendRequest(const QString& targetUsername,
                                    const QString& remark) {
  const QString target = targetUsername.trimmed();
  if (target.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_send_friend_request(
           c_api_, target.toStdString().c_str(),
           remark.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  emit status(ok ? QStringLiteral("好友请求已发送") : QStringLiteral("好友请求失败"));
  return ok;
}

bool QuickClient::respondFriendRequest(const QString& requesterUsername,
                                       bool accept) {
  const QString requester = requesterUsername.trimmed();
  if (requester.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_respond_friend_request(
           c_api_, requester.toStdString().c_str(), accept ? 1 : 0) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  emit status(ok ? QStringLiteral("好友请求已处理") : QStringLiteral("好友请求处理失败"));
  if (ok) {
    std::vector<mi_friend_request_entry_t> req_buffer(
        kMaxFriendRequestEntries);
    const std::uint32_t req_count =
        mi_client_list_friend_requests(c_api_, req_buffer.data(),
                                       kMaxFriendRequestEntries);
    UpdateFriendRequests(read_friend_request_entries(req_buffer.data(),
                                                  req_count));
    if (accept) {
      std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
      const std::uint32_t count =
          mi_client_list_friends(c_api_, buffer.data(), kMaxFriendEntries);
      UpdateFriendList(read_friend_entries(buffer.data(), count));
    }
  }
  return ok;
}

bool QuickClient::setUserBlocked(const QString& blockedUsername,
                                 bool blocked) {
  const QString target = blockedUsername.trimmed();
  if (target.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  const bool ok =
      mi_client_set_user_blocked(c_api_, target.toStdString().c_str(),
                                 blocked ? 1 : 0) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  emit status(ok ? QStringLiteral("屏蔽状态已更新")
                 : QStringLiteral("屏蔽状态更新失败"));
  return ok;
}

QVariantList QuickClient::listDevices() {
  QVariantList out;
  if (!c_api_) {
    return out;
  }
  constexpr std::uint32_t kMaxDeviceEntries = 128;
  std::vector<mi_device_entry_t> buffer(kMaxDeviceEntries);
  const std::uint32_t count =
      mi_client_list_devices(c_api_, buffer.data(), kMaxDeviceEntries);
  out.reserve(static_cast<int>(count));
  for (std::uint32_t i = 0; i < count; ++i) {
    QVariantMap map;
    if (buffer[i].device_id) {
      map.insert(QStringLiteral("deviceId"),
                 QString::fromUtf8(buffer[i].device_id));
    } else {
      map.insert(QStringLiteral("deviceId"), QString());
    }
    if (buffer[i].display_id) {
      map.insert(QStringLiteral("deviceDisplayId"),
                 QString::fromUtf8(buffer[i].display_id));
    } else {
      map.insert(QStringLiteral("deviceDisplayId"), QString());
    }
    map.insert(QStringLiteral("lastSeenSec"),
               static_cast<int>(buffer[i].last_seen_sec));
    out.push_back(map);
  }
  return out;
}

QVariantList QuickClient::listDevicesDisplay() {
  return display_contract::BuildDeviceDisplayList(
      listDevices(), deviceId(), deviceDisplayId());
}

bool QuickClient::kickDevice(const QString& deviceId) {
  const QString id = deviceId.trimmed();
  if (id.isEmpty()) {
    UpdateLastError(QStringLiteral("设备 ID 为空"));
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_kick_device(c_api_, id.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(QStringLiteral("踢出失败"));
    return false;
  }
  UpdateLastError(QString());
  emit status(QStringLiteral("已踢出设备"));
  return true;
}

QString QuickClient::gatewayDisplayState() const {
  return mi::client::ui::display_contract::BuildGatewayDisplayInfo(config_path_)
      .state;
}

QString QuickClient::gatewayDisplayDetail() const {
  return mi::client::ui::display_contract::BuildGatewayDisplayInfo(config_path_)
      .detail;
}

QString QuickClient::maskedCurrentDeviceId() const {
  QString value = deviceDisplayId();
  if (value.trimmed().isEmpty()) {
    value = deviceId();
  }
  return mi::client::ui::display_contract::MaskedDeviceId(value);
}

bool QuickClient::sendReadReceipt(const QString& peerUsername,
                                  const QString& messageId) {
  const QString peer = peerUsername.trimmed();
  const QString msgId = messageId.trimmed();
  if (peer.isEmpty() || msgId.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_send_read_receipt(c_api_,
                                   peer.toStdString().c_str(),
                                   msgId.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  }
  return ok;
}

bool QuickClient::trustPendingServer(const QString& pin) {
  const QString p = pin.trimmed();
  if (p.isEmpty()) {
    UpdateLastError(QStringLiteral("验证码为空"));
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_trust_pending_server(c_api_, p.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  MaybeEmitTrustSignals();
  UpdateConnectionState(true);
  return ok;
}

bool QuickClient::trustPendingPeer(const QString& pin) {
  const QString p = pin.trimmed();
  if (p.isEmpty()) {
    UpdateLastError(QStringLiteral("验证码为空"));
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  bool ok = false;
  ok = mi_client_trust_pending_peer(c_api_, p.toStdString().c_str()) != 0;
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  MaybeEmitTrustSignals();
  return ok;
}

#include "quick_client_call_methods.inc"

QString QuickClient::serverInfo() const {
  return QStringLiteral("config: %1").arg(config_path_);
}

QString QuickClient::version() const {
  return QStringLiteral("UI QML 1.0");
}

QUrl QuickClient::defaultDownloadFileUrl(const QString& fileName) const {
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  if (base.isEmpty()) {
    base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  }
  if (base.isEmpty()) {
    base = QDir::homePath();
  }
  if (fileName.trimmed().isEmpty()) {
    return QUrl::fromLocalFile(base);
  }
  return QUrl::fromLocalFile(
      QDir(base).filePath(sanitize_download_file_name(
          fileName, QStringLiteral("download.bin"))));
}

bool QuickClient::imeAvailable() {
  if (EnsureImeSession() != nullptr) {
    return true;
  }
  return !get_pinyin_index().dict.isEmpty();
}

bool QuickClient::imeRimeAvailable() {
  return EnsureImeSession() != nullptr;
}

QVariantList QuickClient::imeCandidates(const QString& input,
                                        int maxCandidates) {
  QVariantList items;
  const QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return items;
  }
  const int limit =
      maxCandidates > 0 ? maxCandidates : kMaxPinyinCandidatesPerKey;
  QStringList list;
  void* session = EnsureImeSession();
  if (session) {
    list = ImePluginLoader::instance().queryCandidates(session, trimmed, limit);
  }
  if (list.isEmpty()) {
    list = build_pinyin_candidates(trimmed, limit);
  }
  for (const auto& candidate : list) {
    items.push_back(candidate);
  }
  return items;
}

QString QuickClient::imePreedit() {
  if (!ime_session_) {
    return {};
  }
  return ImePluginLoader::instance().queryPreedit(ime_session_);
}

bool QuickClient::imeCommit(int index) {
  if (!ime_session_) {
    return false;
  }
  return ImePluginLoader::instance().commitCandidate(ime_session_, index);
}

void QuickClient::imeClear() {
  if (!ime_session_) {
    return;
  }
  ImePluginLoader::instance().clearComposition(ime_session_);
}

void QuickClient::imeReset() {
  if (!ime_session_) {
    return;
  }
  ImePluginLoader::instance().destroySession(ime_session_);
  ime_session_ = nullptr;
}

bool QuickClient::internalImeEnabled() const {
  return internal_ime_enabled_;
}

void QuickClient::setInternalImeEnabled(bool enabled) {
  internal_ime_enabled_ = enabled;
}

bool QuickClient::aiEnhanceGpuAvailable() const {
  return ai_gpu_available_;
}

bool QuickClient::aiEnhanceEnabled() const {
  return ai_enhance_enabled_;
}

void QuickClient::setAiEnhanceEnabled(bool enabled) {
  ai_enhance_enabled_ = enabled;
  save_ai_enhance_settings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

int QuickClient::aiEnhanceQualityLevel() const {
  return ai_enhance_quality_;
}

void QuickClient::setAiEnhanceQualityLevel(int level) {
  ai_enhance_quality_ = clamp_enhance_scale(level);
  save_ai_enhance_settings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

bool QuickClient::aiEnhanceX4Confirmed() const {
  return ai_enhance_x4_confirmed_;
}

void QuickClient::setAiEnhanceX4Confirmed(bool confirmed) {
  ai_enhance_x4_confirmed_ = confirmed;
  save_ai_enhance_settings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

QVariantMap QuickClient::aiEnhanceRecommendations() const {
  QVariantMap out;
  out.insert(QStringLiteral("gpuAvailable"), ai_gpu_available_);
  out.insert(QStringLiteral("gpuName"), ai_gpu_name_);
  out.insert(QStringLiteral("gpuSeries"), ai_gpu_series_);
  out.insert(QStringLiteral("perfScale"), ai_rec_perf_scale_);
  out.insert(QStringLiteral("qualityScale"), ai_rec_quality_scale_);
  return out;
}

bool QuickClient::historySaveEnabled() const {
  return history_save_enabled_;
}

void QuickClient::setHistorySaveEnabled(bool enabled) {
  history_save_enabled_ = enabled;
  save_privacy_settings(history_save_enabled_);
  if (c_api_) {
    mi_client_set_history_enabled(c_api_, history_save_enabled_ ? 1 : 0);
  }
  if (!history_save_enabled_ && loggedIn() && c_api_) {
    if (!mi_client_clear_all_history(c_api_, 1, 0)) {
      const char* err = mi_client_last_error(c_api_);
      UpdateLastError(err ? QString::fromUtf8(err) : QString());
    }
  }
}

bool QuickClient::clipboardIsolation() const {
  return clipboard_isolation_enabled_;
}

void QuickClient::setClipboardIsolation(bool enabled) {
  clipboard_isolation_enabled_ = enabled;
}

QUrl QuickClient::chatBackground(const QString& chatId) const {
  const QString key = chat_background_key(username_, chatId);
  if (key.isEmpty()) {
    return {};
  }
  const auto it = chat_backgrounds_.constFind(key);
  if (it == chat_backgrounds_.constEnd()) {
    return {};
  }
  const QString path = it.value();
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return {};
  }
  return QUrl::fromLocalFile(path);
}

bool QuickClient::setChatBackground(const QString& chatId,
                                    const QString& imageUrl) {
  const QString key = chat_background_key(username_, chatId);
  if (key.isEmpty()) {
    UpdateLastError(QStringLiteral("聊天对象为空"));
    return false;
  }
  const QString localPath = resolve_local_file_path(imageUrl);
  if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
    UpdateLastError(QStringLiteral("图片不存在"));
    return false;
  }
  const QString dir = chat_backgrounds_dir();
  if (dir.isEmpty()) {
    UpdateLastError(QStringLiteral("背景目录不可用"));
    return false;
  }
  QDir().mkpath(dir);
  const QFileInfo info(localPath);
  const QString ext = info.suffix();
  const QString signature =
      localPath + QStringLiteral("|") +
      QString::number(info.lastModified().toSecsSinceEpoch());
  const QByteArray hash =
      QCryptographicHash::hash(signature.toUtf8(),
                               QCryptographicHash::Sha1)
          .toHex()
          .left(8);
  const QString fileName =
      sanitize_file_stem(key) + QStringLiteral("_") +
      QString::fromLatin1(hash) +
      (ext.isEmpty() ? QString() : QStringLiteral(".") + ext);
  const QString targetPath = QDir(dir).filePath(fileName);
  if (!QFileInfo::exists(targetPath)) {
    if (!QFile::copy(localPath, targetPath)) {
      UpdateLastError(QStringLiteral("背景保存失败"));
      return false;
    }
  }
  chat_backgrounds_.insert(key, targetPath);
  save_chat_backgrounds(chat_backgrounds_);
  UpdateLastError(QString());
  return true;
}

QString QuickClient::renderProtectedText(const QString& protectedTextId) const {
  const auto it = protected_ui_texts_.constFind(protectedTextId);
  if (it == protected_ui_texts_.constEnd()) {
    return {};
  }
  QString rendered;
  (void)with_protected_ui_text(it.value(), [&](std::string_view view) {
    rendered = to_ui_qstring(view);
    return true;
  });
  return rendered;
}

QString QuickClient::StoreProtectedUiText(const QString& text) const {
  QByteArray utf8 = text.toUtf8();
  UiProtectedText protected_text = UiProtectedText::Protect(std::string_view(
      utf8.constData(), static_cast<std::size_t>(utf8.size())));
  if (!utf8.isEmpty()) {
    std::fill_n(utf8.data(), utf8.size(), '\0');
  }
  const QString id =
      QStringLiteral("pt:%1:%2")
          .arg(++protected_ui_text_seq_)
          .arg(QRandomGenerator::global()->generate64(), 16, 16,
               QLatin1Char('0'));
  protected_ui_texts_.insert(id, std::move(protected_text));
  return id;
}

void QuickClient::InsertProtectedUiText(QVariantMap& msg,
                                        const QString& text) const {
  msg.insert(QStringLiteral("protectedTextId"), StoreProtectedUiText(text));
}

bool QuickClient::loggedIn() const {
  return logged_in_;
}

QString QuickClient::username() const {
  return username_;
}

QString QuickClient::lastError() const {
  return last_error_;
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

QString QuickClient::deviceId() const {
  if (c_api_) {
    const char* value = mi_client_device_id(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

QString QuickClient::deviceDisplayId() const {
  if (c_api_) {
    const char* value = mi_client_device_display_id(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

bool QuickClient::remoteOk() const {
  if (c_api_) {
    return mi_client_remote_ok(c_api_) != 0;
  }
  return false;
}

QString QuickClient::remoteError() const {
  if (c_api_) {
    const char* value = mi_client_remote_error(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

bool QuickClient::hasPendingServerTrust() const {
  if (c_api_) {
    return mi_client_has_pending_server_trust(c_api_) != 0;
  }
  return false;
}

QString QuickClient::pendingServerFingerprint() const {
  if (c_api_) {
    const char* value = mi_client_pending_server_fingerprint(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

QString QuickClient::pendingServerPin() const {
  if (c_api_) {
    const char* value = mi_client_pending_server_pin(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

bool QuickClient::hasPendingPeerTrust() const {
  if (c_api_) {
    return mi_client_has_pending_peer_trust(c_api_) != 0;
  }
  return false;
}

QString QuickClient::pendingPeerUsername() const {
  if (c_api_) {
    if (mi_client_has_pending_peer_trust(c_api_) == 0) {
      return {};
    }
    const char* value = mi_client_pending_peer_username(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

QString QuickClient::pendingPeerFingerprint() const {
  if (c_api_) {
    if (mi_client_has_pending_peer_trust(c_api_) == 0) {
      return {};
    }
    const char* value = mi_client_pending_peer_fingerprint(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

QString QuickClient::pendingPeerPin() const {
  if (c_api_) {
    if (mi_client_has_pending_peer_trust(c_api_) == 0) {
      return {};
    }
    const char* value = mi_client_pending_peer_pin(c_api_);
    return value ? QString::fromUtf8(value) : QString();
  }
  return {};
}

QString QuickClient::qrLoginPayload() const {
  return qr_login_payload_;
}

bool QuickClient::qrLoginActive() const {
  return qr_login_active_;
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
  if (!c_api_) {
    return;
  }
  mi::sdk::ChatPollResult poll_result;
  std::vector<mi::sdk::GroupCallEvent> call_events;
  std::string poll_err;
  mi::sdk::PollResult polled;
  (void)mi::sdk::PollEvents(c_api_, 64, 0, polled, poll_err);
  poll_result = std::move(polled.chat);
  call_events = std::move(polled.group_calls);
  const QString poll_error = QString::fromStdString(poll_err);
  if (is_session_invalid_error(poll_error)) {
    HandleSessionInvalid(QStringLiteral("登录已失效，请重新登录"));
    return;
  }
  HandlePollResult(poll_result);
  if (!call_events.empty()) {
    HandleGroupCallEvents(call_events);
  }
  TryActivatePendingGroupCall();
  TryUpdateGroupCallKey();
  UpdateConnectionState(false);
  MaybeEmitTrustSignals();

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - last_friend_sync_ms_ > 2000) {
    std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
    int changed = 0;
    const std::uint32_t count =
        mi_client_sync_friends(c_api_, buffer.data(), kMaxFriendEntries,
                               &changed);
    if (changed) {
      UpdateFriendList(read_friend_entries(buffer.data(), count));
    }
    last_friend_sync_ms_ = now;
  }
  if (now - last_request_sync_ms_ > 4000) {
    std::vector<mi_friend_request_entry_t> req_buffer(
        kMaxFriendRequestEntries);
    const std::uint32_t req_count =
        mi_client_list_friend_requests(c_api_, req_buffer.data(),
                                       kMaxFriendRequestEntries);
    UpdateFriendRequests(read_friend_request_entries(req_buffer.data(),
                                                  req_count));
    last_request_sync_ms_ = now;
  }
  if (now - last_heartbeat_ms_ > 5000) {
    mi_client_heartbeat(c_api_);
    last_heartbeat_ms_ = now;
  }

  if (group_call_session_ && active_group_call_key_id_ != 0 &&
      now - last_group_call_ping_ms_ > 3000) {
    (void)mi_client_send_group_call_signal(
        c_api_, kGroupCallOpPing,
        active_group_call_group_.toStdString().c_str(),
        active_group_call_id_bytes_.data(),
        static_cast<std::uint32_t>(active_group_call_id_bytes_.size()),
        active_group_call_video_ ? 1 : 0, active_group_call_key_id_, 0, 0,
        nullptr, 0, nullptr, 0, nullptr, nullptr, 0, nullptr);
    last_group_call_ping_ms_ = now;
  }

  if (media_session_ && !media_timer_.isActive()) {
    mi_media_config_t media_cfg{};
    QString cfg_err;
    if (LoadMediaConfig(media_cfg, cfg_err)) {
      std::string err;
      media_session_->PollIncoming(media_cfg.pull_max_packets,
                                   media_cfg.pull_wait_ms, err);
    }
  }
}

void QuickClient::EmitMessage(const QVariantMap& message) {
  emit messageEvent(message);
}

void QuickClient::UpdateFriendList(
    const std::vector<mi::sdk::FriendEntry>& friends) {
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
    const std::vector<mi::sdk::FriendRequestEntry>& requests) {
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

bool QuickClient::AddGroupIfMissing(const QString& groupId) {
  for (const auto& entry : groups_) {
    const auto map = entry.toMap();
    if (map.value(QStringLiteral("id")).toString() == groupId) {
      return false;
    }
  }
  QVariantMap map;
  map.insert(QStringLiteral("id"), groupId);
  map.insert(QStringLiteral("name"), groupId);
  map.insert(QStringLiteral("unread"), 0);
  groups_.push_back(map);
  return true;
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

QVariantMap QuickClient::BuildHistoryMessageFromC(
    const mi_history_entry_t& entry) const {
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"),
             entry.conv_id ? QString::fromUtf8(entry.conv_id) : QString());
  msg.insert(QStringLiteral("sender"),
             entry.sender ? QString::fromUtf8(entry.sender) : QString());
  msg.insert(QStringLiteral("outgoing"), entry.outgoing != 0);
  msg.insert(QStringLiteral("isGroup"), entry.is_group != 0);
  const QString messageId =
      entry.message_id ? QString::fromUtf8(entry.message_id) : QString();
  msg.insert(QStringLiteral("messageId"), messageId);
  msg.insert(QStringLiteral("time"),
             QDateTime::fromSecsSinceEpoch(
                 static_cast<qint64>(entry.timestamp_sec))
                 .toString(QStringLiteral("HH:mm:ss")));
  msg.insert(QStringLiteral("timestampSec"),
             static_cast<qint64>(entry.timestamp_sec));
  switch (static_cast<mi::sdk::HistoryStatus>(entry.status)) {
    case mi::sdk::HistoryStatus::kSent:
      msg.insert(QStringLiteral("status"), QStringLiteral("sent"));
      break;
    case mi::sdk::HistoryStatus::kDelivered:
      msg.insert(QStringLiteral("status"), QStringLiteral("delivered"));
      break;
    case mi::sdk::HistoryStatus::kRead:
      msg.insert(QStringLiteral("status"), QStringLiteral("read"));
      break;
    case mi::sdk::HistoryStatus::kFailed:
      msg.insert(QStringLiteral("status"), QStringLiteral("failed"));
      break;
    default:
      msg.insert(QStringLiteral("status"), QStringLiteral("sent"));
      break;
  }

  switch (static_cast<mi::sdk::HistoryKind>(entry.kind)) {
    case mi::sdk::HistoryKind::kText:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      InsertProtectedUiText(
          msg, entry.text != nullptr ? QString::fromUtf8(entry.text)
                                     : QString());
      break;
    case mi::sdk::HistoryKind::kFile: {
      msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
      msg.insert(QStringLiteral("fileName"),
                 entry.file_name ? QString::fromUtf8(entry.file_name) : QString());
      msg.insert(QStringLiteral("fileSize"),
                 static_cast<qint64>(entry.file_size));
      msg.insert(QStringLiteral("fileId"),
                 entry.file_id ? QString::fromUtf8(entry.file_id) : QString());
      std::array<std::uint8_t, 32> key{};
      if (entry.file_key && entry.file_key_len == key.size()) {
        std::memcpy(key.data(), entry.file_key, key.size());
      }
      msg.insert(QStringLiteral("fileKey"), BytesToHex32(key));
      if (!messageId.isEmpty()) {
        const QString enhancedPath = enhanced_image_path_if_exists(messageId);
        if (!enhancedPath.isEmpty()) {
          const QString ext =
              QFileInfo(entry.file_name ? QString::fromUtf8(entry.file_name)
                                        : QString())
                  .suffix();
          if (is_image_ext(ext)) {
            msg.insert(QStringLiteral("fileUrl"),
                       QUrl::fromLocalFile(enhancedPath));
            msg.insert(QStringLiteral("imageEnhanced"), true);
          }
        }
      }
      break;
    }
    case mi::sdk::HistoryKind::kSticker: {
      msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
      const QString sid =
          entry.sticker_id ? QString::fromUtf8(entry.sticker_id) : QString();
      msg.insert(QStringLiteral("stickerId"), sid);
      const auto meta = BuildStickerMeta(sid);
      msg.insert(QStringLiteral("stickerUrl"),
                 meta.value(QStringLiteral("stickerUrl")));
      msg.insert(QStringLiteral("stickerAnimated"),
                 meta.value(QStringLiteral("stickerAnimated")));
      break;
    }
    case mi::sdk::HistoryKind::kSystem:
      msg.insert(QStringLiteral("kind"), QStringLiteral("system"));
      InsertProtectedUiText(
          msg, entry.text != nullptr ? QString::fromUtf8(entry.text)
                                     : QString());
      break;
    case mi::sdk::HistoryKind::kUnknown:
      msg.insert(QStringLiteral("kind"), QStringLiteral("unknown"));
      InsertProtectedUiText(
          msg, entry.text != nullptr ? QString::fromUtf8(entry.text)
                                     : QStringLiteral("Unknown message"));
      break;
    default:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      break;
  }
  return msg;
}

void QuickClient::HandlePollResult(const mi::sdk::ChatPollResult& result) {
  const QString now = now_time_string();
  const qint64 nowSec = QDateTime::currentSecsSinceEpoch();

  for (const auto& t : result.texts) {
    const QString text = QString::fromStdString(t.text_utf8);
    const QString callEndId = parse_call_end_id(text);
    if (!callEndId.isEmpty()) {
      if (callEndId == active_call_id_) {
        EndCallInternal(false);
      }
      QVariantMap msg;
      msg.insert(QStringLiteral("convId"),
                 QString::fromStdString(t.from_username));
      msg.insert(QStringLiteral("sender"),
                 QString::fromStdString(t.from_username));
      msg.insert(QStringLiteral("outgoing"), false);
      msg.insert(QStringLiteral("isGroup"), false);
      msg.insert(QStringLiteral("kind"), QStringLiteral("call_end"));
      msg.insert(QStringLiteral("callId"), callEndId);
      msg.insert(QStringLiteral("time"), now);
      msg.insert(QStringLiteral("timestampSec"), nowSec);
      EmitMessage(msg);
      continue;
    }
    const QString recallId = parse_recall_target_id(text);
    if (!recallId.isEmpty()) {
      QVariantMap msg;
      msg.insert(QStringLiteral("convId"),
                 QString::fromStdString(t.from_username));
      msg.insert(QStringLiteral("sender"),
                 QString::fromStdString(t.from_username));
      msg.insert(QStringLiteral("outgoing"), false);
      msg.insert(QStringLiteral("isGroup"), false);
      msg.insert(QStringLiteral("kind"), QStringLiteral("recall"));
      msg.insert(QStringLiteral("targetMessageId"), recallId);
      msg.insert(QStringLiteral("time"), now);
      msg.insert(QStringLiteral("timestampSec"), nowSec);
      EmitMessage(msg);
      continue;
    }
    const auto invite = parse_call_invite(text);
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
    msg.insert(QStringLiteral("timestampSec"), nowSec);
    if (invite.ok) {
      msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
      msg.insert(QStringLiteral("callId"), invite.callId);
      msg.insert(QStringLiteral("video"), invite.video);
    } else {
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      InsertProtectedUiText(msg, text);
    }
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_texts) {
    const QString text = QString::fromStdString(t.text_utf8);
    if (!parse_call_end_id(text).isEmpty() ||
        !parse_recall_target_id(text).isEmpty()) {
      continue;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    InsertProtectedUiText(msg, text);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    msg.insert(QStringLiteral("timestampSec"), nowSec);
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
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
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
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& t : result.group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    const QString text = QString::fromStdString(t.text_utf8);
    const QString callEndId = parse_call_end_id(text);
    if (!callEndId.isEmpty()) {
      continue;
    }
    const QString recallId = parse_recall_target_id(text);
    if (!recallId.isEmpty()) {
      QVariantMap msg;
      msg.insert(QStringLiteral("convId"), group_id);
      msg.insert(QStringLiteral("sender"),
                 QString::fromStdString(t.from_username));
      msg.insert(QStringLiteral("outgoing"), false);
      msg.insert(QStringLiteral("isGroup"), true);
      msg.insert(QStringLiteral("kind"), QStringLiteral("recall"));
      msg.insert(QStringLiteral("targetMessageId"), recallId);
      msg.insert(QStringLiteral("time"), now);
      msg.insert(QStringLiteral("timestampSec"), nowSec);
      EmitMessage(msg);
      continue;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    InsertProtectedUiText(msg, text);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    msg.insert(QStringLiteral("timestampSec"), nowSec);
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    const QString text = QString::fromStdString(t.text_utf8);
    if (!parse_call_end_id(text).isEmpty() ||
        !parse_recall_target_id(text).isEmpty()) {
      continue;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    InsertProtectedUiText(msg, text);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    msg.insert(QStringLiteral("timestampSec"), nowSec);
    EmitMessage(msg);
  }

  for (const auto& f : result.group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
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
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.outgoing_group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
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
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
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
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    const QString actor = QString::fromStdString(n.actor_username);
    const QString target = QString::fromStdString(n.target_username);
    QString text;
    switch (n.kind) {
      case 1:
        text = QStringLiteral("%1 加入群聊").arg(target);
        break;
      case 2:
        text = QStringLiteral("%1 离开群聊").arg(target);
        break;
      case 3:
        text = QStringLiteral("%1 被移出群聊").arg(target);
        break;
      case 4:
        text = QStringLiteral("%1 权限变更").arg(target);
        break;
      default:
        text = QStringLiteral("群通知更新");
        break;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), actor);
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("notice"));
    InsertProtectedUiText(msg, text);
    msg.insert(QStringLiteral("noticeKind"), static_cast<int>(n.kind));
    msg.insert(QStringLiteral("noticeActor"), actor);
    msg.insert(QStringLiteral("noticeTarget"), target);
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& d : result.deliveries) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(d.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("delivery"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(d.message_id_hex));
    EmitMessage(msg);
  }

  for (const auto& r : result.read_receipts) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(r.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("read"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(r.message_id_hex));
    EmitMessage(msg);
  }

  for (const auto& t : result.typing_events) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("typing"));
    msg.insert(QStringLiteral("typing"), t.typing);
    EmitMessage(msg);
  }

  for (const auto& p : result.presence_events) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(p.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("presence"));
    msg.insert(QStringLiteral("online"), p.online);
    EmitMessage(msg);
  }
}

void QuickClient::HandleGroupCallEvents(
    const std::vector<mi::sdk::GroupCallEvent>& events) {
  bool rooms_changed = false;
  for (const auto& ev : events) {
    if (ev.group_id.empty()) {
      continue;
    }
    const auto op = ev.op;
    const bool ended = (op == kGroupCallOpEnd);
    if (ended) {
      group_call_rooms_map_.erase(ev.group_id);
      group_call_media_flags_.erase(ev.group_id);
      rooms_changed = true;
    } else {
      group_call_rooms_map_[ev.group_id] = ev.call_id;
      group_call_media_flags_[ev.group_id] = ev.media_flags;
      rooms_changed = true;
    }

    if (group_call_session_ &&
        active_group_call_group_ == QString::fromStdString(ev.group_id)) {
      if (op == kGroupCallOpCreate || op == kGroupCallOpJoin) {
        group_call_member_media_flags_[ev.sender] = ev.media_flags;
        AddGroupCallParticipant(ev.sender);
      } else if (op == kGroupCallOpLeave) {
        group_call_member_media_flags_.erase(ev.sender);
        RemoveGroupCallParticipant(ev.sender);
      } else if (op == kGroupCallOpUpdate) {
        group_call_member_media_flags_[ev.sender] = ev.media_flags;
      } else if (op == kGroupCallOpEnd) {
        StopMedia();
        emit groupCallStateChanged();
        emit groupCallParticipantsChanged();
        emit status(QStringLiteral("群通话已结束"));
        continue;
      }

      if (ev.key_id > active_group_call_key_id_ &&
          active_group_call_key_id_ != 0) {
        std::vector<std::string> members;
        bool snap_ok = false;
        std::uint32_t snap_key_id = 0;
        std::vector<mi_group_call_member_t> buffer(kMaxGroupCallMembers);
        std::uint32_t count = 0;
        std::array<std::uint8_t, 16> snap_call{};
        snap_ok = mi_client_send_group_call_signal(
                      c_api_, kGroupCallOpPing, ev.group_id.c_str(),
                      ev.call_id.data(),
                      static_cast<std::uint32_t>(ev.call_id.size()),
                      active_group_call_video_ ? 1 : 0, ev.key_id, 0, 0,
                      nullptr, 0, snap_call.data(),
                      static_cast<std::uint32_t>(snap_call.size()),
                      &snap_key_id, buffer.data(),
                      static_cast<std::uint32_t>(buffer.size()), &count) != 0;
        if (snap_ok) {
          members = read_group_call_members(buffer.data(), count);
        }
        if (snap_ok) {
          if (active_group_call_owner_) {
            std::vector<const char*> member_ptrs;
            member_ptrs.reserve(members.size());
            for (const auto& m : members) {
              member_ptrs.push_back(m.c_str());
            }
            const bool rotated =
                mi_client_rotate_group_call_key(
                    c_api_, ev.group_id.c_str(), ev.call_id.data(),
                    static_cast<std::uint32_t>(ev.call_id.size()), snap_key_id,
                    member_ptrs.data(),
                    static_cast<std::uint32_t>(member_ptrs.size())) != 0;
            if (rotated) {
              std::string err;
              group_call_session_->SetActiveKey(snap_key_id, err);
              active_group_call_key_id_ = snap_key_id;
              pending_group_call_key_id_ = 0;
            }
          } else {
            std::array<std::uint8_t, 32> call_key{};
            const bool has_key =
                mi_client_get_group_call_key(
                    c_api_, ev.group_id.c_str(), ev.call_id.data(),
                    static_cast<std::uint32_t>(ev.call_id.size()), snap_key_id,
                    call_key.data(),
                    static_cast<std::uint32_t>(call_key.size())) != 0;
            if (!has_key) {
              std::vector<const char*> member_ptrs;
              member_ptrs.reserve(members.size());
              for (const auto& m : members) {
                member_ptrs.push_back(m.c_str());
              }
              (void)mi_client_request_group_call_key(
                  c_api_, ev.group_id.c_str(), ev.call_id.data(),
                  static_cast<std::uint32_t>(ev.call_id.size()), snap_key_id,
                  member_ptrs.data(),
                  static_cast<std::uint32_t>(member_ptrs.size()));
              pending_group_call_key_id_ = snap_key_id;
            } else {
              std::string err;
              group_call_session_->SetActiveKey(snap_key_id, err);
              active_group_call_key_id_ = snap_key_id;
              pending_group_call_key_id_ = 0;
            }
          }
        }
      }
    }
  }
  if (rooms_changed) {
    UpdateGroupCallRooms();
  }
}

void QuickClient::UpdateGroupCallRooms() {
  QVariantList rooms;
  rooms.reserve(static_cast<int>(group_call_rooms_map_.size()));
  for (const auto& kv : group_call_rooms_map_) {
    QVariantMap entry;
    entry.insert(QStringLiteral("groupId"),
                 QString::fromStdString(kv.first));
    entry.insert(QStringLiteral("callId"), BytesToHex(kv.second));
    const std::uint8_t flags =
        group_call_media_flags_.count(kv.first) != 0
            ? group_call_media_flags_[kv.first]
            : kGroupCallMediaAudio;
    entry.insert(QStringLiteral("video"),
                 (flags & kGroupCallMediaVideo) != 0);
    rooms.push_back(entry);
  }
  group_call_rooms_ = std::move(rooms);
  emit groupCallRoomsChanged();
}

void QuickClient::UpdateGroupCallParticipants(
    const std::vector<std::string>& members) {
  group_call_member_ids_.clear();
  group_call_member_ids_.reserve(members.size());
  std::unordered_set<std::string> present;
  for (const auto& m : members) {
    if (m.empty()) {
      continue;
    }
    if (std::find(group_call_member_ids_.begin(),
                  group_call_member_ids_.end(), m) ==
        group_call_member_ids_.end()) {
      group_call_member_ids_.push_back(m);
      present.insert(m);
    }
  }
  for (auto it = group_call_member_media_flags_.begin();
       it != group_call_member_media_flags_.end();) {
    if (present.find(it->first) == present.end()) {
      it = group_call_member_media_flags_.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& member : group_call_member_ids_) {
    if (group_call_member_media_flags_.find(member) ==
        group_call_member_media_flags_.end()) {
      group_call_member_media_flags_[member] = kGroupCallMediaAudio;
    }
  }
  SyncGroupCallRemotes();
}

void QuickClient::AddGroupCallParticipant(const std::string& username) {
  if (username.empty()) {
    return;
  }
  if (std::find(group_call_member_ids_.begin(),
                group_call_member_ids_.end(), username) !=
      group_call_member_ids_.end()) {
    return;
  }
  group_call_member_ids_.push_back(username);
  if (group_call_member_media_flags_.find(username) ==
      group_call_member_media_flags_.end()) {
    group_call_member_media_flags_[username] = kGroupCallMediaAudio;
  }
  SyncGroupCallRemotes();
}

void QuickClient::RemoveGroupCallParticipant(const std::string& username) {
  if (username.empty()) {
    return;
  }
  auto it = std::remove(group_call_member_ids_.begin(),
                        group_call_member_ids_.end(), username);
  if (it == group_call_member_ids_.end()) {
    return;
  }
  group_call_member_ids_.erase(it, group_call_member_ids_.end());
  group_call_member_media_flags_.erase(username);
  SyncGroupCallRemotes();
}

void QuickClient::SyncGroupCallRemotes() {
  QVariantList participants;
  participants.reserve(static_cast<int>(group_call_member_ids_.size()));
  std::unordered_set<std::string> desired;
  const std::string self = username_.toStdString();
  for (const auto& m : group_call_member_ids_) {
    QVariantMap entry;
    entry.insert(QStringLiteral("username"), QString::fromStdString(m));
    participants.push_back(entry);
    if (!self.empty() && m == self) {
      continue;
    }
    desired.insert(m);
  }

  for (auto it = group_call_remotes_.begin();
       it != group_call_remotes_.end();) {
    if (desired.find(it->first) == desired.end()) {
      it = group_call_remotes_.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& name : desired) {
    EnsureGroupCallRemote(name);
  }

  group_call_participants_ = std::move(participants);
  emit groupCallParticipantsChanged();
  UpdateGroupCallSubscriptions();
}

void QuickClient::BuildGroupCallSubscriptionPayload(
    std::vector<std::uint8_t>& out) const {
  struct Entry {
    std::string sender;
    std::uint8_t flags{0};
  };
  std::vector<Entry> subs;
  subs.reserve(group_call_member_ids_.size());
  const std::string self = username_.toStdString();
  for (const auto& member : group_call_member_ids_) {
    if (member.empty()) {
      continue;
    }
    if (!self.empty() && member == self) {
      continue;
    }
    std::uint8_t flags = kGroupCallMediaAudio;
    if (active_group_call_video_) {
      const auto it = group_call_member_media_flags_.find(member);
      if (it == group_call_member_media_flags_.end() ||
          (it->second & kGroupCallMediaVideo) != 0) {
        flags = static_cast<std::uint8_t>(flags | kGroupCallMediaVideo);
      }
    }
    if ((flags & (kGroupCallMediaAudio | kGroupCallMediaVideo)) == 0) {
      continue;
    }
    Entry entry;
    entry.sender = member;
    entry.flags = flags;
    subs.push_back(std::move(entry));
  }

  out.clear();
  out.reserve(4 + subs.size() * 12);
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(subs.size()), out);
  for (const auto& sub : subs) {
    mi::server::proto::WriteString(sub.sender, out);
    out.push_back(sub.flags);
  }
}

void QuickClient::UpdateGroupCallSubscriptions() {
  if (!group_call_session_ || active_group_call_group_.isEmpty()) {
    return;
  }
  if (!c_api_) {
    return;
  }
  std::vector<std::uint8_t> payload;
  BuildGroupCallSubscriptionPayload(payload);
  if (payload == group_call_subscribe_payload_) {
    return;
  }
  bool ok = false;
  const std::uint8_t* ext_ptr =
      payload.empty() ? nullptr : payload.data();
  const std::uint32_t ext_len =
      static_cast<std::uint32_t>(payload.size());
  ok = mi_client_send_group_call_signal(
           c_api_, kGroupCallOpUpdate,
           active_group_call_group_.toStdString().c_str(),
           active_group_call_id_bytes_.data(),
           static_cast<std::uint32_t>(active_group_call_id_bytes_.size()),
           active_group_call_video_ ? 1 : 0, active_group_call_key_id_, 0, 0,
           ext_ptr, ext_len, nullptr, 0, nullptr, nullptr, 0, nullptr) != 0;
  if (ok) {
    group_call_subscribe_payload_ = std::move(payload);
  }
}

void QuickClient::EnsureGroupCallRemote(const std::string& username) {
  if (!group_call_session_ || username.empty()) {
    return;
  }
  if (!username_.isEmpty() && username == username_.toStdString()) {
    return;
  }
  auto it = group_call_remotes_.find(username);
  if (it != group_call_remotes_.end()) {
    if (active_group_call_video_ && !it->second.video) {
      std::string err;
      it->second.video =
          std::make_unique<mi::client::media::VideoPipeline>(
              *it->second.adapter, video_config_);
      if (!it->second.video->Init(err)) {
        it->second.video.reset();
      }
    }
    return;
  }

  GroupCallRemote remote;
  remote.adapter =
      std::make_unique<mi::client::media::GroupCallMediaAdapter>(
          *group_call_session_);
  std::string err;
  remote.audio = std::make_unique<mi::client::media::AudioPipeline>(
      *remote.adapter, audio_config_);
  if (!remote.audio->Init(err)) {
    remote.audio.reset();
  }
  if (active_group_call_video_) {
    remote.video = std::make_unique<mi::client::media::VideoPipeline>(
        *remote.adapter, video_config_);
    if (!remote.video->Init(err)) {
      remote.video.reset();
    }
  }
  group_call_remotes_.emplace(username, std::move(remote));
}

void QuickClient::ClearGroupCallRemotes() {
  group_call_remotes_.clear();
}

void QuickClient::TryActivatePendingGroupCall() {
  if (group_call_session_ || pending_group_call_key_id_ == 0 ||
      pending_group_call_group_.isEmpty()) {
    return;
  }
  if (!c_api_) {
    return;
  }
  const std::array<std::uint8_t, 16> call_id = pending_group_call_id_bytes_;
  const QString group = pending_group_call_group_;
  const bool video = pending_group_call_video_;
  const bool owner = pending_group_call_owner_;
  const std::uint32_t key_id = pending_group_call_key_id_;
  std::array<std::uint8_t, 32> call_key{};
  const bool has_key = mi_client_get_group_call_key(
                           c_api_, group.toStdString().c_str(), call_id.data(),
                           static_cast<std::uint32_t>(call_id.size()), key_id,
                           call_key.data(),
                           static_cast<std::uint32_t>(call_key.size())) != 0;
  if (!has_key) {
    return;
  }
  QString err;
  if (!InitGroupCallSession(group, call_id, key_id, video, owner, err)) {
    emit status(err.isEmpty() ? QStringLiteral("加入群通话失败") : err);
    return;
  }
  std::vector<std::string> members;
  bool snap_ok = false;
  std::vector<mi_group_call_member_t> buffer(kMaxGroupCallMembers);
  std::uint32_t count = 0;
  std::array<std::uint8_t, 16> snap_call{};
  std::uint32_t snap_key_id = 0;
  snap_ok = mi_client_send_group_call_signal(
                c_api_, kGroupCallOpPing, group.toStdString().c_str(),
                call_id.data(), static_cast<std::uint32_t>(call_id.size()),
                video ? 1 : 0, key_id, 0, 0, nullptr, 0, snap_call.data(),
                static_cast<std::uint32_t>(snap_call.size()), &snap_key_id,
                buffer.data(),
                static_cast<std::uint32_t>(buffer.size()), &count) != 0;
  if (snap_ok) {
    members = read_group_call_members(buffer.data(), count);
  }
  if (snap_ok) {
    UpdateGroupCallParticipants(members);
  } else {
    std::vector<std::string> members;
    std::vector<mi_group_member_entry_t> member_buffer(kMaxGroupMemberEntries);
    const std::uint32_t member_count =
        mi_client_list_group_members_info(
            c_api_, pending_group_call_group_.toStdString().c_str(),
            member_buffer.data(), kMaxGroupMemberEntries);
    members.reserve(member_count);
    for (std::uint32_t i = 0; i < member_count; ++i) {
      if (member_buffer[i].username) {
        members.emplace_back(member_buffer[i].username);
      }
    }
    UpdateGroupCallParticipants(members);
  }
}

void QuickClient::TryUpdateGroupCallKey() {
  if (!group_call_session_ || pending_group_call_key_id_ == 0 ||
      active_group_call_group_.isEmpty()) {
    return;
  }
  if (!c_api_) {
    return;
  }
  std::array<std::uint8_t, 32> call_key{};
  const bool has_key =
      mi_client_get_group_call_key(
          c_api_, active_group_call_group_.toStdString().c_str(),
          active_group_call_id_bytes_.data(),
          static_cast<std::uint32_t>(active_group_call_id_bytes_.size()),
          pending_group_call_key_id_, call_key.data(),
          static_cast<std::uint32_t>(call_key.size())) != 0;
  if (!has_key) {
    return;
  }
  std::string err;
  if (group_call_session_->SetActiveKey(pending_group_call_key_id_, err)) {
    active_group_call_key_id_ = pending_group_call_key_id_;
    pending_group_call_key_id_ = 0;
  }
}

void QuickClient::ClearGroupCallState(bool notify) {
  group_call_session_.reset();
  group_call_adapter_.reset();
  ClearGroupCallRemotes();
  group_call_member_ids_.clear();
  group_call_member_media_flags_.clear();
  group_call_subscribe_payload_.clear();
  group_call_participants_.clear();
  active_group_call_id_.clear();
  active_group_call_group_.clear();
  active_group_call_video_ = false;
  active_group_call_owner_ = false;
  active_group_call_key_id_ = 0;
  pending_group_call_key_id_ = 0;
  pending_group_call_group_.clear();
  pending_group_call_video_ = false;
  pending_group_call_owner_ = false;
  active_group_call_id_bytes_.fill(0);
  pending_group_call_id_bytes_.fill(0);
  last_group_call_ping_ms_ = 0;
  group_mix_buffer_.clear();
  if (notify) {
    emit groupCallStateChanged();
    emit groupCallParticipantsChanged();
  }
}

void QuickClient::HandleSessionInvalid(const QString& message) {
  const QString hint = message.trimmed().isEmpty()
                           ? QStringLiteral("登录已失效，请重新登录")
                           : message.trimmed();
  const bool was_logged_in = logged_in_ || !username_.isEmpty();

  StopPolling();
  StopMedia();
  if (c_api_) {
    mi_client_logout(c_api_);
  }
  logged_in_ = false;
  username_.clear();
  friends_.clear();
  groups_.clear();
  friend_requests_.clear();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();

  if (last_error_ != hint) {
    last_error_ = hint;
    emit errorChanged();
  }
  if (was_logged_in) {
    emit authStateChanged();
    emit userChanged();
    emit friendsChanged();
    emit groupsChanged();
    emit friendRequestsChanged();
    emit callStateChanged();
    emit groupCallStateChanged();
    emit groupCallParticipantsChanged();
    emit groupCallRoomsChanged();
  }
  emit status(hint);
}

void QuickClient::HandleLoginSuccess(const QString& username) {
  logged_in_ = true;
  username_ = username.trimmed();
  QString historyErr;
  if (!history_save_enabled_) {
    if (!mi_client_clear_all_history(c_api_, 1, 0)) {
      const char* err = mi_client_last_error(c_api_);
      historyErr = err ? QString::fromUtf8(err) : QString();
    }
    mi_client_set_history_enabled(c_api_, 0);
  }
  emit status(QStringLiteral("登录成功"));
  UpdateLastError(historyErr);
  StartPolling();
  std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
  int changed = 0;
  std::uint32_t count =
      mi_client_sync_friends(c_api_, buffer.data(), kMaxFriendEntries,
                             &changed);
  const char* sync_err = mi_client_last_error(c_api_);
  if (sync_err && *sync_err) {
    count = mi_client_list_friends(c_api_, buffer.data(), kMaxFriendEntries);
  }
  UpdateFriendList(read_friend_entries(buffer.data(), count));

  std::vector<mi_friend_request_entry_t> req_buffer(
      kMaxFriendRequestEntries);
  const std::uint32_t req_count =
      mi_client_list_friend_requests(c_api_, req_buffer.data(),
                                     kMaxFriendRequestEntries);
  UpdateFriendRequests(read_friend_request_entries(req_buffer.data(),
                                                req_count));
  emit deviceChanged();
}

void QuickClient::ClearQrLoginCache() {
  qr_login_image_cache_.clear();
}

void QuickClient::UpdateLastError(const QString& message) {
  const QString trimmed = message.trimmed();
  if (is_session_invalid_error(trimmed)) {
    HandleSessionInvalid(QStringLiteral("登录已失效，请重新登录"));
    return;
  }
  if (trimmed == last_error_) {
    return;
  }
  last_error_ = trimmed;
  emit errorChanged();
}

void QuickClient::UpdateConnectionState(bool force_emit) {
  bool ok = false;
  QString err;
  if (c_api_) {
    ok = mi_client_remote_ok(c_api_) != 0;
    const char* value = mi_client_remote_error(c_api_);
    err = value ? QString::fromUtf8(value) : QString();
  }
  if (!force_emit && ok == last_remote_ok_ && err == last_remote_error_) {
    return;
  }
  last_remote_ok_ = ok;
  last_remote_error_ = err;
  emit connectionChanged();
}

void QuickClient::MaybeEmitTrustSignals() {
  bool changed = false;
  if (hasPendingServerTrust()) {
    const QString fp = pendingServerFingerprint();
    if (fp != last_pending_server_fingerprint_) {
      last_pending_server_fingerprint_ = fp;
      emit serverTrustRequired(fp, pendingServerPin());
      changed = true;
    }
  } else if (!last_pending_server_fingerprint_.isEmpty()) {
    last_pending_server_fingerprint_.clear();
    changed = true;
  }

  if (hasPendingPeerTrust()) {
    const QString fp = pendingPeerFingerprint();
    if (fp != last_pending_peer_fingerprint_) {
      last_pending_peer_fingerprint_ = fp;
      emit peerTrustRequired(pendingPeerUsername(), fp, pendingPeerPin());
      changed = true;
    }
  } else if (!last_pending_peer_fingerprint_.isEmpty()) {
    last_pending_peer_fingerprint_.clear();
    changed = true;
  }

  if (changed) {
    emit trustStateChanged();
  }
}

void QuickClient::EmitDownloadProgress(const QString& fileId,
                                       const QString& savePath,
                                       double progress) {
  const double base = download_progress_base_.value(fileId, 0.0);
  const double span = download_progress_span_.value(fileId, 1.0);
  double clamped = std::max(0.0, std::min(1.0, progress));
  double scaled = base + clamped * span;
  scaled = std::max(0.0, std::min(1.0, scaled));
  emit attachmentDownloadProgress(fileId, savePath, scaled);
}

void QuickClient::ResetMediaTransport() {
  if (media_session_ || group_call_session_) {
    return;
  }
  if (!c_api_) {
    media_transport_.reset();
    return;
  }
  media_transport_ = std::make_unique<CapiMediaTransport>(c_api_);
}

bool QuickClient::LoadMediaConfig(mi_media_config_t& out_config,
                                  QString& out_error) {
  out_error.clear();
  if (c_api_) {
    if (mi_client_get_media_config(c_api_, &out_config) == 0) {
      const char* err = mi_client_last_error(c_api_);
      out_error = err ? QString::fromUtf8(err) : QString();
      if (out_error.isEmpty()) {
        out_error = QStringLiteral("媒体配置读取失败");
      }
      return false;
    }
    return true;
  }
  out_error = QStringLiteral("媒体通道不可用");
  return false;
}

#include "quick_client_media_methods.inc"


}  // namespace mi::client::ui
