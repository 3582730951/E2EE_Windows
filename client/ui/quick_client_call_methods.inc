QString QuickClient::startVoiceCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return {};
  }
  if (group_call_session_) {
    emit status(QStringLiteral("当前已有群通话进行中"));
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
  char* out_id = nullptr;
  const bool ok =
      mi_client_send_private_text(c_api_, peer.toStdString().c_str(),
                                  invite.toStdString().c_str(), &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    StopMedia();
    active_call_id_.clear();
    active_call_peer_.clear();
    active_call_video_ = false;
    emit callStateChanged();
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(QStringLiteral("语音通话邀请发送失败"));
    return {};
  }
  UpdateLastError(QString());

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), false);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("timestampSec"),
             QDateTime::currentSecsSinceEpoch());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  emit status(QStringLiteral("语音通话已发起"));
  return call_hex;
}

QString QuickClient::startVideoCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return {};
  }
  if (group_call_session_) {
    emit status(QStringLiteral("当前已有群通话进行中"));
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
  char* out_id = nullptr;
  const bool ok =
      mi_client_send_private_text(c_api_, peer.toStdString().c_str(),
                                  invite.toStdString().c_str(), &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    StopMedia();
    active_call_id_.clear();
    active_call_peer_.clear();
    active_call_video_ = false;
    emit callStateChanged();
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(QStringLiteral("视频通话邀请发送失败"));
    return {};
  }
  UpdateLastError(QString());

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), true);
  msg.insert(QStringLiteral("time"), now_time_string());
  msg.insert(QStringLiteral("timestampSec"),
             QDateTime::currentSecsSinceEpoch());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  emit status(QStringLiteral("视频通话已发起"));
  return call_hex;
}

bool QuickClient::joinCall(const QString& peerUsername,
                           const QString& callIdHex,
                           bool video) {
  if (group_call_session_) {
    emit status(QStringLiteral("当前已有群通话进行中"));
    return false;
  }
  QString err;
  if (!InitMediaSession(peerUsername, callIdHex, false, video, err)) {
    emit status(err.isEmpty() ? QStringLiteral("加入通话失败") : err);
    return false;
  }
  emit status(QStringLiteral("已加入通话"));
  return true;
}

void QuickClient::endCall() {
  EndCallInternal(true);
}

bool QuickClient::sendCallEnd(const QString& peerUsername,
                              const QString& callIdHex) {
  const QString peer = peerUsername.trimmed();
  const QString callId = callIdHex.trimmed();
  if (peer.isEmpty() || callId.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    return false;
  }
  const QString payload =
      QString::fromLatin1(kCallEndPrefix) + callId;
  std::string msg_id;
  char* out_id = nullptr;
  const bool ok =
      mi_client_send_private_text(c_api_, peer.toStdString().c_str(),
                                  payload.toStdString().c_str(),
                                  &out_id) != 0;
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }
  if (!ok) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
  } else {
    UpdateLastError(QString());
  }
  return ok;
}

QString QuickClient::startGroupCall(const QString& groupId, bool video) {
  const QString group = groupId.trimmed();
  if (group.isEmpty()) {
    return {};
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(lastError());
    return {};
  }
  if (!active_call_id_.isEmpty() || group_call_session_) {
    emit status(QStringLiteral("当前已有通话进行中"));
    return {};
  }
  std::array<std::uint8_t, 16> call_id{};
  std::uint32_t key_id = 0;
  if (mi_client_start_group_call(c_api_, group.toStdString().c_str(),
                                 video ? 1 : 0, call_id.data(),
                                 static_cast<std::uint32_t>(call_id.size()),
                                 &key_id) == 0) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(lastError());
    return {};
  }
  QString err;
  if (!InitGroupCallSession(group, call_id, key_id, video, true, err)) {
    emit status(err.isEmpty() ? QStringLiteral("群通话初始化失败") : err);
    return {};
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
        mi_client_list_group_members_info(c_api_, group.toStdString().c_str(),
                                          member_buffer.data(),
                                          kMaxGroupMemberEntries);
    members.reserve(member_count);
    for (std::uint32_t i = 0; i < member_count; ++i) {
      if (member_buffer[i].username) {
        members.emplace_back(member_buffer[i].username);
      }
    }
    UpdateGroupCallParticipants(members);
  }

  const std::string notify_text =
      video ? "[groupcall] video started" : "[groupcall] voice started";
  std::string msg_id;
  char* out_id = nullptr;
  (void)mi_client_send_group_text(c_api_, group.toStdString().c_str(),
                                  notify_text.c_str(), &out_id);
  if (out_id) {
    msg_id.assign(out_id);
    mi_client_free(out_id);
  }

  group_call_rooms_map_[group.toStdString()] = call_id;
  group_call_media_flags_[group.toStdString()] =
      static_cast<std::uint8_t>(kGroupCallMediaAudio |
                                (video ? kGroupCallMediaVideo : 0));
  UpdateGroupCallRooms();
  return BytesToHex(call_id);
}

bool QuickClient::joinGroupCall(const QString& groupId,
                                const QString& callIdHex,
                                bool video) {
  const QString group = groupId.trimmed();
  if (group.isEmpty()) {
    return false;
  }
  if (!c_api_) {
    UpdateLastError(QStringLiteral("未初始化"));
    emit status(lastError());
    return false;
  }
  if (!active_call_id_.isEmpty() || group_call_session_) {
    emit status(QStringLiteral("当前已有通话进行中"));
    return false;
  }
  std::array<std::uint8_t, 16> call_id{};
  if (!HexToBytes16(callIdHex, call_id)) {
    UpdateLastError(QStringLiteral("群通话 ID 格式错误"));
    return false;
  }
  std::uint32_t key_id = 0;
  if (mi_client_join_group_call(
          c_api_, group.toStdString().c_str(), call_id.data(),
          static_cast<std::uint32_t>(call_id.size()), video ? 1 : 0,
          &key_id) == 0) {
    const char* err = mi_client_last_error(c_api_);
    UpdateLastError(err ? QString::fromUtf8(err) : QString());
    emit status(lastError());
    return false;
  }

  std::array<std::uint8_t, 32> call_key{};
  const bool has_key =
      mi_client_get_group_call_key(
          c_api_, group.toStdString().c_str(), call_id.data(),
          static_cast<std::uint32_t>(call_id.size()), key_id, call_key.data(),
          static_cast<std::uint32_t>(call_key.size())) != 0;
  if (has_key) {
    QString err;
    if (!InitGroupCallSession(group, call_id, key_id, video, false, err)) {
      emit status(err.isEmpty() ? QStringLiteral("加入群通话失败") : err);
      return false;
    }
    std::vector<std::string> members;
    bool snap_ok = false;
    std::vector<mi_group_call_member_t> buffer(kMaxGroupCallMembers);
    std::uint32_t count = 0;
    std::array<std::uint8_t, 16> snap_call{};
    std::uint32_t snap_key_id = 0;
    snap_ok = mi_client_send_group_call_signal(
                  c_api_, kGroupCallOpPing, group.toStdString().c_str(),
                  call_id.data(),
                  static_cast<std::uint32_t>(call_id.size()), video ? 1 : 0,
                  key_id, 0, 0, nullptr, 0, snap_call.data(),
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
              c_api_, group.toStdString().c_str(), member_buffer.data(),
              kMaxGroupMemberEntries);
      members.reserve(member_count);
      for (std::uint32_t i = 0; i < member_count; ++i) {
        if (member_buffer[i].username) {
          members.emplace_back(member_buffer[i].username);
        }
      }
      UpdateGroupCallParticipants(members);
    }
  } else {
    pending_group_call_id_bytes_ = call_id;
    pending_group_call_group_ = group;
    pending_group_call_video_ = video;
    pending_group_call_owner_ = false;
    pending_group_call_key_id_ = key_id;
    emit status(QStringLiteral("正在等待群通话密钥"));
  }

  group_call_rooms_map_[group.toStdString()] = call_id;
  group_call_media_flags_[group.toStdString()] =
      static_cast<std::uint8_t>(kGroupCallMediaAudio |
                                (video ? kGroupCallMediaVideo : 0));
  UpdateGroupCallRooms();
  return true;
}

void QuickClient::leaveGroupCall() {
  if (!group_call_session_) {
    return;
  }
  if (!active_group_call_group_.isEmpty()) {
    if (active_group_call_owner_) {
      const std::string notify_text = "[groupcall] ended";
      std::string msg_id;
      if (c_api_) {
        char* out_id = nullptr;
        (void)mi_client_send_group_text(
            c_api_, active_group_call_group_.toStdString().c_str(),
            notify_text.c_str(), &out_id);
        if (out_id) {
          msg_id.assign(out_id);
          mi_client_free(out_id);
        }
      }
    }
    if (c_api_) {
      (void)mi_client_leave_group_call(
          c_api_, active_group_call_group_.toStdString().c_str(),
          active_group_call_id_bytes_.data(),
          static_cast<std::uint32_t>(active_group_call_id_bytes_.size()));
    }
  }
  StopMedia();
  emit groupCallStateChanged();
  emit groupCallParticipantsChanged();
  emit status(QStringLiteral("已退出群通话"));
}

void QuickClient::endGroupCall() {
  if (!group_call_session_) {
    return;
  }
  if (!active_group_call_group_.isEmpty()) {
    if (active_group_call_owner_) {
      if (c_api_) {
        (void)mi_client_send_group_call_signal(
            c_api_, kGroupCallOpEnd,
            active_group_call_group_.toStdString().c_str(),
            active_group_call_id_bytes_.data(),
            static_cast<std::uint32_t>(active_group_call_id_bytes_.size()),
            active_group_call_video_ ? 1 : 0, 0, 0, 0, nullptr, 0, nullptr, 0,
            nullptr, nullptr, 0, nullptr);
      }
      const std::string notify_text = "[groupcall] ended";
      std::string msg_id;
      if (c_api_) {
        char* out_id = nullptr;
        (void)mi_client_send_group_text(
            c_api_, active_group_call_group_.toStdString().c_str(),
            notify_text.c_str(), &out_id);
        if (out_id) {
          msg_id.assign(out_id);
          mi_client_free(out_id);
        }
      }
    } else {
      if (c_api_) {
        (void)mi_client_leave_group_call(
            c_api_, active_group_call_group_.toStdString().c_str(),
            active_group_call_id_bytes_.data(),
            static_cast<std::uint32_t>(active_group_call_id_bytes_.size()));
      }
    }
  }
  StopMedia();
  emit groupCallStateChanged();
  emit groupCallParticipantsChanged();
  emit status(QStringLiteral("群通话已结束"));
}

bool QuickClient::callMicEnabled() const {
  return call_mic_enabled_;
}

void QuickClient::setCallMicEnabled(bool enabled) {
  call_mic_enabled_ = enabled;
  if (!call_mic_enabled_) {
    audio_in_buffer_.clear();
    audio_in_offset_ = 0;
  }
}

bool QuickClient::callCameraEnabled() const {
  return call_camera_enabled_;
}

void QuickClient::setCallCameraEnabled(bool enabled) {
  call_camera_enabled_ = enabled;
  if (!camera_) {
    return;
  }
  if (call_camera_enabled_) {
    if (!camera_->isActive()) {
      camera_->start();
    }
  } else {
    camera_->stop();
    if (local_video_sink_) {
      local_video_sink_->setVideoFrame(QVideoFrame());
    }
  }
}

void QuickClient::EndCallInternal(bool notify_peer) {
  const QString peer = active_call_peer_;
  const QString callId = active_call_id_;
  if (notify_peer && !peer.isEmpty() && !callId.isEmpty()) {
    const QString payload =
        QString::fromLatin1(kCallEndPrefix) + callId;
    std::string msg_id;
    if (c_api_) {
      char* out_id = nullptr;
      (void)mi_client_send_private_text(c_api_, peer.toStdString().c_str(),
                                        payload.toStdString().c_str(), &out_id);
      if (out_id) {
        msg_id.assign(out_id);
        mi_client_free(out_id);
      }
    }
  }
  StopMedia();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  emit callStateChanged();
  emit status(QStringLiteral("通话已结束"));
}

void QuickClient::bindRemoteVideoSink(QObject* sink) {
  auto* casted = qobject_cast<QVideoSink*>(sink);
  if (!casted || casted == remote_video_sink_) {
    return;
  }
  remote_video_sink_ = casted;
}

void QuickClient::bindLocalVideoSink(QObject* sink) {
  auto* casted = qobject_cast<QVideoSink*>(sink);
  if (!casted || casted == local_video_sink_) {
    return;
  }
  if (local_video_sink_) {
    disconnect(local_video_sink_, nullptr, this, nullptr);
  }
  local_video_sink_ = casted;
  if (auto* session = EnsureCaptureSession()) {
    session->setVideoSink(local_video_sink_);
  }
  connect(local_video_sink_, &QVideoSink::videoFrameChanged, this,
          &QuickClient::HandleLocalVideoFrame);
}

void QuickClient::bindGroupCallVideoSink(const QString& username,
                                         QObject* sink) {
  const QString user = username.trimmed();
  auto* casted = qobject_cast<QVideoSink*>(sink);
  if (user.isEmpty() || !casted) {
    return;
  }
  EnsureGroupCallRemote(user.toStdString());
  auto it = group_call_remotes_.find(user.toStdString());
  if (it == group_call_remotes_.end()) {
    return;
  }
  it->second.sink = casted;
}

