bool QuickClient::InitMediaSession(const QString& peerUsername,
                                   const QString& callIdHex,
                                   bool initiator,
                                   bool video,
                                   QString& outError) {
  outError.clear();
  StopMedia();
  ResetMediaTransport();
  if (!media_transport_) {
    outError = QStringLiteral("媒体通道不可用");
    return false;
  }
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
  mi_media_config_t media_cfg{};
  if (!LoadMediaConfig(media_cfg, outError)) {
    return false;
  }
  cfg.audio_delay_ms = media_cfg.audio_delay_ms;
  cfg.video_delay_ms = media_cfg.video_delay_ms;
  cfg.audio_max_frames = media_cfg.audio_max_frames;
  cfg.video_max_frames = media_cfg.video_max_frames;

  auto session =
      std::make_unique<mi::client::media::MediaSession>(*media_transport_, cfg);
  std::string err;
  if (!session->Init(err)) {
    outError = err.empty() ? QStringLiteral("通话初始化失败")
                           : QString::fromStdString(err);
    return false;
  }
  media_session_ = std::move(session);
  call_mic_enabled_ = true;
  call_camera_enabled_ = true;
  audio_config_ = mi::client::media::AudioPipelineConfig{};
  const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
  const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
  adjust_audio_config_for_devices(in_device, out_device, audio_config_);
  audio_pipeline_ = std::make_unique<mi::client::media::AudioPipeline>(
      *media_session_, audio_config_);
  if (!audio_pipeline_->Init(err)) {
    outError = err.empty() ? QStringLiteral("音频编码初始化失败")
                           : QString::fromStdString(err);
    StopMedia();
    return false;
  }
  if (video) {
    video_config_ = mi::client::media::VideoPipelineConfig{};
    if (!SetupVideo(outError)) {
      StopMedia();
      return false;
    }
    video_pipeline_ = std::make_unique<mi::client::media::VideoPipeline>(
        *media_session_, video_config_);
    if (!video_pipeline_->Init(err)) {
      outError = err.empty() ? QStringLiteral("视频编码初始化失败")
                             : QString::fromStdString(err);
      StopMedia();
      return false;
    }
  }
  if (!SetupAudio(outError)) {
    StopMedia();
    return false;
  }
  StartMedia();
  active_call_id_ = callIdHex.trimmed();
  active_call_peer_ = peer;
  active_call_video_ = video;
  emit callStateChanged();
  return true;
}

bool QuickClient::InitGroupCallSession(
    const QString& groupId,
    const std::array<std::uint8_t, 16>& callId,
    std::uint32_t keyId,
    bool video,
    bool owner,
    QString& outError) {
  outError.clear();
  StopMedia();
  ResetMediaTransport();
  if (!media_transport_) {
    outError = QStringLiteral("媒体通道不可用");
    return false;
  }
  const QString group = groupId.trimmed();
  if (group.isEmpty()) {
    outError = QStringLiteral("群 ID 不能为空");
    return false;
  }
  if (callId == std::array<std::uint8_t, 16>{}) {
    outError = QStringLiteral("群通话 ID 不能为空");
    return false;
  }
  mi::client::media::GroupCallSessionConfig cfg;
  cfg.group_id = group.toStdString();
  cfg.call_id = callId;
  cfg.key_id = keyId;
  cfg.enable_audio = true;
  cfg.enable_video = video;
  mi_media_config_t media_cfg{};
  if (!LoadMediaConfig(media_cfg, outError)) {
    return false;
  }
  cfg.audio_delay_ms = media_cfg.audio_delay_ms;
  cfg.video_delay_ms = media_cfg.video_delay_ms;
  cfg.audio_max_frames = media_cfg.audio_max_frames;
  cfg.video_max_frames = media_cfg.video_max_frames;

  auto session = std::make_unique<mi::client::media::GroupCallSession>(
      *media_transport_, cfg);
  std::string err;
  if (!session->Init(err)) {
    outError = err.empty() ? QStringLiteral("群通话初始化失败")
                           : QString::fromStdString(err);
    return false;
  }
  group_call_session_ = std::move(session);
  group_call_adapter_ =
      std::make_unique<mi::client::media::GroupCallMediaAdapter>(
          *group_call_session_);

  call_mic_enabled_ = true;
  call_camera_enabled_ = true;
  audio_config_ = mi::client::media::AudioPipelineConfig{};
  const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
  const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
  adjust_audio_config_for_devices(in_device, out_device, audio_config_);
  audio_pipeline_ = std::make_unique<mi::client::media::AudioPipeline>(
      *group_call_adapter_, audio_config_);
  if (!audio_pipeline_->Init(err)) {
    outError = err.empty() ? QStringLiteral("群通话音频初始化失败")
                           : QString::fromStdString(err);
    StopMedia();
    return false;
  }
  if (video) {
    video_config_ = mi::client::media::VideoPipelineConfig{};
    if (!SetupVideo(outError)) {
      StopMedia();
      return false;
    }
    video_pipeline_ = std::make_unique<mi::client::media::VideoPipeline>(
        *group_call_adapter_, video_config_);
    if (!video_pipeline_->Init(err)) {
      outError = err.empty() ? QStringLiteral("群通话视频初始化失败")
                             : QString::fromStdString(err);
      StopMedia();
      return false;
    }
  }
  if (!SetupAudio(outError)) {
    StopMedia();
    return false;
  }
  StartMedia();

  active_group_call_id_bytes_ = callId;
  active_group_call_id_ = BytesToHex(callId);
  active_group_call_group_ = group;
  active_group_call_video_ = video;
  active_group_call_owner_ = owner;
  active_group_call_key_id_ = keyId;
  pending_group_call_key_id_ = 0;
  pending_group_call_group_.clear();
  pending_group_call_video_ = false;
  pending_group_call_owner_ = false;
  group_call_participants_.clear();
  ClearGroupCallRemotes();
  emit groupCallStateChanged();
  return true;
}

void QuickClient::StartMedia() {
  if (!media_timer_.isActive()) {
    media_timer_.start();
  }
  if (camera_ && !camera_->isActive()) {
    camera_->start();
  }
}

void QuickClient::StopMedia() {
  if (media_timer_.isActive()) {
    media_timer_.stop();
  }
  ShutdownAudio();
  ShutdownVideo();
  audio_pipeline_.reset();
  video_pipeline_.reset();
  media_session_.reset();
  group_call_adapter_.reset();
  group_call_session_.reset();
  audio_in_buffer_.clear();
  audio_out_pending_.clear();
  audio_in_offset_ = 0;
  audio_frame_tmp_.clear();
  video_send_buffer_.clear();
  call_mic_enabled_ = true;
  call_camera_enabled_ = true;
  if (remote_video_sink_) {
    remote_video_sink_->setVideoFrame(QVideoFrame());
  }
  ClearGroupCallState(false);
}

void QuickClient::PumpMedia() {
  if (media_session_) {
    mi_media_config_t media_cfg{};
    QString cfg_err;
    if (LoadMediaConfig(media_cfg, cfg_err)) {
      std::string err;
      media_session_->PollIncoming(media_cfg.pull_max_packets,
                                   media_cfg.pull_wait_ms, err);
    }

    if (audio_pipeline_) {
      audio_pipeline_->PumpIncoming();
      DrainAudioInput();
      mi::client::media::PcmFrame decoded;
      const int frame_samples = audio_pipeline_->frame_samples();
      const int frame_bytes =
          frame_samples * static_cast<int>(sizeof(std::int16_t));
      const int max_pending = frame_bytes * 10;
      while (audio_pipeline_->PopDecodedFrame(decoded)) {
        if (!decoded.samples.empty()) {
          const char* ptr =
              reinterpret_cast<const char*>(decoded.samples.data());
          const int bytes =
              static_cast<int>(decoded.samples.size() * sizeof(std::int16_t));
          if (bytes > 0) {
            audio_out_pending_.append(ptr, bytes);
            if (audio_out_pending_.size() > max_pending) {
              const int trim = audio_out_pending_.size() - max_pending;
              audio_out_pending_.remove(0, trim);
            }
          }
        }
      }
      FlushAudioOutput();
    }

    if (video_pipeline_) {
      video_pipeline_->PumpIncoming();
      mi::client::media::VideoFrameData latest;
      bool has_frame = false;
      while (video_pipeline_->PopDecodedFrame(latest)) {
        has_frame = true;
      }
      if (has_frame && remote_video_sink_ && latest.width > 0 &&
          latest.height > 0 && !latest.nv12.empty()) {
        std::uint32_t stride = latest.stride;
        if (stride == 0) {
          const std::size_t denom =
              static_cast<std::size_t>(latest.height) * 3;
          const std::size_t maybe =
              denom == 0 ? 0 : latest.nv12.size() * 2 / denom;
          stride = maybe >= latest.width
                       ? static_cast<std::uint32_t>(maybe)
                       : latest.width;
        }
        QVideoFrame frame = make_nv12_frame(std::move(latest.nv12),
                                          latest.width,
                                          latest.height,
                                          stride,
                                          latest.timestamp_ms);
        remote_video_sink_->setVideoFrame(frame);
      }
    }
    return;
  }

  if (group_call_session_) {
    PumpGroupCall();
  }
}

void QuickClient::PumpGroupCall() {
  if (!group_call_session_) {
    return;
  }
  mi_media_config_t media_cfg{};
  QString cfg_err;
  if (LoadMediaConfig(media_cfg, cfg_err)) {
    std::string err;
    group_call_session_->PollIncoming(media_cfg.group_pull_max_packets,
                                      media_cfg.group_pull_wait_ms, err);
  }

  const std::uint64_t now_ms = now_monotonic_ms();
  mi::client::media::GroupMediaFrame frame;
  while (group_call_session_->PopAudioFrame(now_ms, frame)) {
    EnsureGroupCallRemote(frame.sender);
    auto it = group_call_remotes_.find(frame.sender);
    if (it != group_call_remotes_.end() && it->second.adapter) {
      it->second.adapter->PushIncoming(std::move(frame));
    }
  }
  while (group_call_session_->PopVideoFrame(now_ms, frame)) {
    EnsureGroupCallRemote(frame.sender);
    auto it = group_call_remotes_.find(frame.sender);
    if (it != group_call_remotes_.end() && it->second.adapter) {
      it->second.adapter->PushIncoming(std::move(frame));
    }
  }

  for (auto& kv : group_call_remotes_) {
    if (kv.second.audio) {
      kv.second.audio->PumpIncoming();
    }
    if (kv.second.video) {
      kv.second.video->PumpIncoming();
    }
  }

  PumpGroupAudioOutput();
  PumpGroupVideoOutput();
  DrainAudioInput();
}

void QuickClient::PumpGroupAudioOutput() {
  if (!audio_out_device_) {
    return;
  }
  if (!audio_pipeline_) {
    return;
  }
  const int frame_samples = audio_pipeline_->frame_samples();
  if (frame_samples <= 0) {
    return;
  }
  if (group_mix_buffer_.size() != static_cast<std::size_t>(frame_samples)) {
    group_mix_buffer_.assign(static_cast<std::size_t>(frame_samples), 0);
  } else {
    std::fill(group_mix_buffer_.begin(), group_mix_buffer_.end(), 0);
  }
  bool has_frame = false;
  for (auto& kv : group_call_remotes_) {
    if (!kv.second.audio) {
      continue;
    }
    mi::client::media::PcmFrame decoded;
    if (!kv.second.audio->PopDecodedFrame(decoded)) {
      continue;
    }
    if (decoded.samples.size() != group_mix_buffer_.size()) {
      continue;
    }
    has_frame = true;
    for (std::size_t i = 0; i < decoded.samples.size(); ++i) {
      const int mixed =
          static_cast<int>(group_mix_buffer_[i]) +
          static_cast<int>(decoded.samples[i]);
      group_mix_buffer_[i] = static_cast<std::int16_t>(
          std::max(-32768, std::min(32767, mixed)));
    }
  }
  if (!has_frame) {
    return;
  }
  const char* ptr =
      reinterpret_cast<const char*>(group_mix_buffer_.data());
  const int bytes =
      static_cast<int>(group_mix_buffer_.size() * sizeof(std::int16_t));
  if (bytes > 0) {
    audio_out_pending_.append(ptr, bytes);
  }
  FlushAudioOutput();
}

void QuickClient::PumpGroupVideoOutput() {
  for (auto& kv : group_call_remotes_) {
    if (!kv.second.video || !kv.second.sink) {
      continue;
    }
    mi::client::media::VideoFrameData latest;
    bool has_frame = false;
    while (kv.second.video->PopDecodedFrame(latest)) {
      has_frame = true;
    }
    if (!has_frame || latest.width == 0 || latest.height == 0 ||
        latest.nv12.empty()) {
      continue;
    }
    std::uint32_t stride = latest.stride;
    if (stride == 0) {
      const std::size_t denom =
          static_cast<std::size_t>(latest.height) * 3;
      const std::size_t maybe =
          denom == 0 ? 0 : latest.nv12.size() * 2 / denom;
      stride = maybe >= latest.width
                   ? static_cast<std::uint32_t>(maybe)
                   : latest.width;
    }
    QVideoFrame frame_obj = make_nv12_frame(std::move(latest.nv12),
                                          latest.width,
                                          latest.height,
                                          stride,
                                          latest.timestamp_ms);
    kv.second.sink->setVideoFrame(frame_obj);
  }
}

void QuickClient::DrainAudioInput() {
  if (!audio_pipeline_ || !audio_in_device_) {
    return;
  }
  if (!call_mic_enabled_) {
    audio_in_buffer_.clear();
    audio_in_offset_ = 0;
    return;
  }
  const int frame_samples = audio_pipeline_->frame_samples();
  if (frame_samples <= 0) {
    return;
  }
  const int frame_bytes = frame_samples * static_cast<int>(sizeof(std::int16_t));
  if (frame_bytes <= 0) {
    return;
  }
  if (audio_frame_tmp_.size() != static_cast<std::size_t>(frame_samples)) {
    audio_frame_tmp_.assign(static_cast<std::size_t>(frame_samples), 0);
  }
  while (audio_in_buffer_.size() - audio_in_offset_ >= frame_bytes) {
    const char* src = audio_in_buffer_.constData() + audio_in_offset_;
    std::memcpy(audio_frame_tmp_.data(), src,
                static_cast<std::size_t>(frame_bytes));
    audio_in_offset_ += frame_bytes;
    audio_pipeline_->SendPcmFrame(audio_frame_tmp_.data(),
                                  static_cast<std::size_t>(frame_samples));
  }
  if (audio_in_offset_ > 0 &&
      audio_in_offset_ >= audio_in_buffer_.size() / 2) {
    audio_in_buffer_.remove(0, audio_in_offset_);
    audio_in_offset_ = 0;
  }
}

void QuickClient::FlushAudioOutput() {
  if (!audio_out_device_ || audio_out_pending_.isEmpty()) {
    return;
  }
  for (;;) {
    const qint64 written = audio_out_device_->write(audio_out_pending_);
    if (written <= 0) {
      break;
    }
    audio_out_pending_.remove(0, static_cast<int>(written));
    if (audio_out_pending_.isEmpty()) {
      break;
    }
  }
}

bool QuickClient::SetupAudio(QString& outError) {
  outError.clear();
  if (!audio_pipeline_) {
    return true;
  }
  const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
  const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
  const bool have_in = !in_device.isNull();
  const bool have_out = !out_device.isNull();
  if (!have_in && !have_out) {
    outError = QStringLiteral("未找到音频设备");
    return false;
  }
  QAudioFormat format;
  format.setSampleRate(audio_config_.sample_rate);
  format.setChannelCount(audio_config_.channels);
  format.setSampleFormat(QAudioFormat::Int16);
  const bool in_ok = have_in && in_device.isFormatSupported(format);
  const bool out_ok = have_out && out_device.isFormatSupported(format);
  if (!in_ok && !out_ok) {
    outError = QStringLiteral("音频格式不支持");
    return false;
  }
  if (in_ok) {
    audio_source_ = std::make_unique<QAudioSource>(in_device, format, this);
  }
  if (out_ok) {
    audio_sink_ = std::make_unique<QAudioSink>(out_device, format, this);
  }
  const int frame_bytes =
      audio_pipeline_->frame_samples() * static_cast<int>(sizeof(std::int16_t));
  if (frame_bytes > 0) {
    if (audio_source_) {
      audio_source_->setBufferSize(frame_bytes * 4);
    }
    if (audio_sink_) {
      audio_sink_->setBufferSize(frame_bytes * 8);
    }
  }
  if (audio_source_) {
    audio_in_device_ = audio_source_->start();
    if (!audio_in_device_) {
      audio_source_.reset();
    }
  }
  if (audio_sink_) {
    audio_out_device_ = audio_sink_->start();
    if (!audio_out_device_) {
      audio_sink_.reset();
    }
  }
  if (!audio_in_device_ && !audio_out_device_) {
    outError = QStringLiteral("音频设备启动失败");
    return false;
  }
  if (audio_in_device_) {
    connect(audio_in_device_, &QIODevice::readyRead, this,
            &QuickClient::HandleAudioReady);
  }
  return true;
}

bool QuickClient::SetupVideo(QString& outError) {
  outError.clear();
  const QCameraDevice device = QMediaDevices::defaultVideoInput();
  if (device.isNull()) {
    return true;
  }
  QMediaCaptureSession* session = EnsureCaptureSession();
  if (!session) {
    outError = QStringLiteral("视频模块初始化失败");
    return false;
  }
  camera_ = std::make_unique<QCamera>(device);
  session->setCamera(camera_.get());
  session->setVideoSink(local_video_sink_);
  if (local_video_sink_) {
    disconnect(local_video_sink_, nullptr, this, nullptr);
    connect(local_video_sink_, &QVideoSink::videoFrameChanged, this,
            &QuickClient::HandleLocalVideoFrame);
  }
  if (!SelectCameraFormat()) {
    const QCameraFormat fmt = camera_->cameraFormat();
    if (fmt.isNull()) {
      outError = QStringLiteral("摄像头格式不可用");
      return false;
    }
    const QSize res = fmt.resolution();
    if (res.isValid()) {
      video_config_.width = static_cast<std::uint32_t>(res.width());
      video_config_.height = static_cast<std::uint32_t>(res.height());
    }
    const float max_fps = fmt.maxFrameRate();
    if (max_fps > 1.0f) {
      video_config_.fps =
          static_cast<std::uint32_t>(std::lround(max_fps));
    }
    if (video_config_.fps == 0) {
      video_config_.fps = 24;
    }
  }
  return true;
}

void QuickClient::ShutdownAudio() {
  if (audio_source_) {
    audio_source_->stop();
  }
  if (audio_sink_) {
    audio_sink_->stop();
  }
  audio_in_device_ = nullptr;
  audio_out_device_ = nullptr;
  audio_source_.reset();
  audio_sink_.reset();
  audio_in_buffer_.clear();
  audio_out_pending_.clear();
  audio_in_offset_ = 0;
}

void QuickClient::ShutdownVideo() {
  if (camera_) {
    camera_->stop();
  }
  if (capture_session_) {
    capture_session_->setVideoSink(nullptr);
    capture_session_->setCamera(nullptr);
  }
  camera_.reset();
}

QMediaCaptureSession* QuickClient::EnsureCaptureSession() {
  if (!capture_session_) {
    capture_session_ = std::make_unique<QMediaCaptureSession>(this);
  }
  return capture_session_.get();
}

void* QuickClient::EnsureImeSession() {
  if (ime_session_) {
    return ime_session_;
  }
  ime_session_ = ImePluginLoader::instance().createSession();
  return ime_session_;
}

void QuickClient::HandleAudioReady() {
  if (!audio_in_device_) {
    return;
  }
  const QByteArray data = audio_in_device_->readAll();
  if (data.isEmpty()) {
    return;
  }
  audio_in_buffer_.append(data);
  DrainAudioInput();
}

void QuickClient::HandleLocalVideoFrame(const QVideoFrame& frame) {
  if (!video_pipeline_ || (!media_session_ && !group_call_session_)) {
    return;
  }
  if (!call_camera_enabled_) {
    return;
  }
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t stride = 0;
  if (!ConvertVideoFrameToNv12(frame, video_send_buffer_, width, height,
                               stride)) {
    return;
  }
  if (width == 0 || height == 0 || stride == 0) {
    return;
  }
  video_pipeline_->SendNv12Frame(video_send_buffer_.data(), stride, width,
                                 height);
}

bool QuickClient::ConvertVideoFrameToNv12(const QVideoFrame& frame,
                                          std::vector<std::uint8_t>& out,
                                          std::uint32_t& width,
                                          std::uint32_t& height,
                                          std::size_t& stride) const {
  QVideoFrame mapped(frame);
  if (!mapped.isValid()) {
    return false;
  }
  if (!mapped.map(QVideoFrame::ReadOnly)) {
    return false;
  }
  width = static_cast<std::uint32_t>(mapped.width());
  height = static_cast<std::uint32_t>(mapped.height());
  if (width == 0 || height == 0) {
    mapped.unmap();
    return false;
  }
  stride = width;
  const std::size_t y_bytes =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const std::size_t uv_bytes = y_bytes / 2;
  out.resize(y_bytes + uv_bytes);
  std::uint8_t* y_out = out.data();
  std::uint8_t* uv_out = out.data() + y_bytes;
  const auto fmt = mapped.pixelFormat();

  if (fmt == QVideoFrameFormat::Format_NV12 ||
      fmt == QVideoFrameFormat::Format_NV21) {
    const int y_stride = mapped.bytesPerLine(0);
    const int uv_stride = mapped.bytesPerLine(1);
    const std::uint8_t* y_src = mapped.bits(0);
    const std::uint8_t* uv_src = mapped.bits(1);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(y_out + row * width, y_src + row * y_stride, width);
    }
    const std::uint32_t uv_height = height / 2;
    if (fmt == QVideoFrameFormat::Format_NV12) {
      for (std::uint32_t row = 0; row < uv_height; ++row) {
        std::memcpy(uv_out + row * width, uv_src + row * uv_stride, width);
      }
    } else {
      for (std::uint32_t row = 0; row < uv_height; ++row) {
        const std::uint8_t* src = uv_src + row * uv_stride;
        std::uint8_t* dst = uv_out + row * width;
        for (std::uint32_t col = 0; col + 1 < width; col += 2) {
          dst[col] = src[col + 1];
          dst[col + 1] = src[col];
        }
      }
    }
    mapped.unmap();
    return true;
  }

  if (fmt == QVideoFrameFormat::Format_YUV420P ||
      fmt == QVideoFrameFormat::Format_YV12) {
    const int y_stride = mapped.bytesPerLine(0);
    const int u_stride = mapped.bytesPerLine(1);
    const int v_stride = mapped.bytesPerLine(2);
    const std::uint8_t* y_src = mapped.bits(0);
    const std::uint8_t* u_src = mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 1 : 2);
    const std::uint8_t* v_src = mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 2 : 1);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(y_out + row * width, y_src + row * y_stride, width);
    }
    const std::uint32_t uv_height = height / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      const std::uint8_t* u_line = u_src + row * u_stride;
      const std::uint8_t* v_line = v_src + row * v_stride;
      std::uint8_t* dst = uv_out + row * width;
      for (std::uint32_t col = 0; col + 1 < width; col += 2) {
        dst[col] = u_line[col / 2];
        dst[col + 1] = v_line[col / 2];
      }
    }
    mapped.unmap();
    return true;
  }

  if (fmt == QVideoFrameFormat::Format_YUYV ||
      fmt == QVideoFrameFormat::Format_UYVY) {
    const int src_stride = mapped.bytesPerLine(0);
    const std::uint8_t* src = mapped.bits(0);
    const std::uint32_t width_even = width & ~1u;
    for (std::uint32_t row = 0; row < height; ++row) {
      const std::uint8_t* line = src + row * src_stride;
      for (std::uint32_t col = 0; col < width_even; col += 2) {
        std::uint8_t y0 = 0;
        std::uint8_t y1 = 0;
        std::uint8_t u = 0;
        std::uint8_t v = 0;
        if (fmt == QVideoFrameFormat::Format_YUYV) {
          y0 = line[0];
          u = line[1];
          y1 = line[2];
          v = line[3];
        } else {
          u = line[0];
          y0 = line[1];
          v = line[2];
          y1 = line[3];
        }
        y_out[row * width + col] = y0;
        if (col + 1 < width) {
          y_out[row * width + col + 1] = y1;
        }
        if ((row & 1u) == 0) {
          std::uint8_t* dst = uv_out + (row / 2) * width;
          dst[col] = u;
          if (col + 1 < width) {
            dst[col + 1] = v;
          }
        }
        line += 4;
      }
    }
    mapped.unmap();
    return true;
  }

  mapped.unmap();
  return false;
}

bool QuickClient::SelectCameraFormat() {
  if (!camera_) {
    return false;
  }
  const auto formats = camera_->cameraDevice().videoFormats();
  if (formats.isEmpty()) {
    return false;
  }
  const QSize target(static_cast<int>(video_config_.width),
                     static_cast<int>(video_config_.height));
  int best_score = std::numeric_limits<int>::max();
  QCameraFormat best;
  bool found = false;
  for (const auto& fmt : formats) {
    const auto pix = fmt.pixelFormat();
    if (pix != QVideoFrameFormat::Format_NV12 &&
        pix != QVideoFrameFormat::Format_NV21 &&
        pix != QVideoFrameFormat::Format_YUV420P &&
        pix != QVideoFrameFormat::Format_YV12 &&
        pix != QVideoFrameFormat::Format_YUYV &&
        pix != QVideoFrameFormat::Format_UYVY) {
      continue;
    }
    const QSize res = fmt.resolution();
    int score = std::abs(res.width() - target.width()) +
                std::abs(res.height() - target.height());
    if (pix != QVideoFrameFormat::Format_NV12) {
      score += 200;
    }
    const float max_fps = fmt.maxFrameRate();
    if (max_fps > 0.0f) {
      score += static_cast<int>(
          std::abs(max_fps - static_cast<float>(video_config_.fps)) * 10.0f);
    }
    if (!found || score < best_score) {
      best = fmt;
      best_score = score;
      found = true;
    }
  }
  if (!found || best.isNull()) {
    return false;
  }
  camera_->setCameraFormat(best);
  const QSize res = best.resolution();
  if (res.isValid()) {
    video_config_.width = static_cast<std::uint32_t>(res.width());
    video_config_.height = static_cast<std::uint32_t>(res.height());
  }
  const float max_fps = best.maxFrameRate();
  if (max_fps > 1.0f) {
    video_config_.fps = static_cast<std::uint32_t>(std::lround(max_fps));
  }
  if (video_config_.fps == 0) {
    video_config_.fps = 24;
  }
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

QString QuickClient::BytesToHex32(const std::array<std::uint8_t, 32>& bytes) {
  const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<int>(bytes.size()));
  return QString::fromLatin1(raw.toHex());
}

bool QuickClient::HexToBytes32(const QString& hex,
                               std::array<std::uint8_t, 32>& out) {
  const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
  if (raw.size() != static_cast<int>(out.size())) {
    return false;
  }
  std::memcpy(out.data(), raw.data(), out.size());
  return true;
}
