#include "CallController.h"

#include <QAbstractVideoBuffer>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>

#include <cmath>
#include <cstring>
#include <limits>

namespace {

class Nv12VideoBuffer final : public QAbstractVideoBuffer {
public:
    Nv12VideoBuffer(std::vector<std::uint8_t> &&data,
                    std::uint32_t width,
                    std::uint32_t height,
                    std::uint32_t stride)
        : format_(QSize(static_cast<int>(width), static_cast<int>(height)),
                  QVideoFrameFormat::Format_NV12),
          data_(std::move(data)),
          stride_(static_cast<int>(stride)),
          height_(static_cast<int>(height)) {}

    MapData map(QVideoFrame::MapMode) override {
        MapData out;
        if (data_.empty() || stride_ <= 0 || height_ <= 0) {
            return out;
        }
        const std::size_t y_bytes =
            static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_);
        if (data_.size() < y_bytes) {
            return out;
        }
        out.planeCount = 2;
        out.bytesPerLine[0] = stride_;
        out.bytesPerLine[1] = stride_;
        out.data[0] = data_.data();
        out.data[1] = data_.data() + y_bytes;
        out.dataSize[0] = static_cast<int>(y_bytes);
        out.dataSize[1] = static_cast<int>(data_.size() - y_bytes);
        return out;
    }

    void unmap() override {}

    QVideoFrameFormat format() const override { return format_; }

private:
    QVideoFrameFormat format_;
    std::vector<std::uint8_t> data_;
    int stride_{0};
    int height_{0};
};

bool HexToBytes16(const QString &hex, std::array<std::uint8_t, 16> &out) {
    const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
    if (raw.size() != static_cast<int>(out.size())) {
        return false;
    }
    std::memcpy(out.data(), raw.data(), out.size());
    return true;
}

bool IsAudioFormatSupported(const QAudioDevice &device,
                            int sampleRate,
                            int channels) {
    if (device.isNull() || sampleRate <= 0 || channels <= 0) {
        return false;
    }
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    return device.isFormatSupported(format);
}

bool PickPreferredAudioFormat(const QAudioDevice &device,
                              int &sampleRate,
                              int &channels) {
    if (device.isNull()) {
        return false;
    }
    const QAudioFormat preferred = device.preferredFormat();
    if (preferred.sampleFormat() != QAudioFormat::Int16) {
        return false;
    }
    const int rate = preferred.sampleRate();
    const int ch = preferred.channelCount();
    if (rate <= 0 || ch <= 0) {
        return false;
    }
    if (!device.isFormatSupported(preferred)) {
        return false;
    }
    sampleRate = rate;
    channels = ch;
    return true;
}

bool FindCandidateAudioFormat(const QAudioDevice &inDevice,
                              const QAudioDevice &outDevice,
                              bool checkIn,
                              bool checkOut,
                              int &sampleRate,
                              int &channels) {
    const std::array<int, 5> rates = {48000, 44100, 32000, 24000, 16000};
    const std::array<int, 2> chans = {1, 2};
    for (const int rate : rates) {
        for (const int ch : chans) {
            if (checkIn && !IsAudioFormatSupported(inDevice, rate, ch)) {
                continue;
            }
            if (checkOut && !IsAudioFormatSupported(outDevice, rate, ch)) {
                continue;
            }
            sampleRate = rate;
            channels = ch;
            return true;
        }
    }
    return false;
}

void AdjustAudioConfigForDevices(
    const QAudioDevice &inDevice,
    const QAudioDevice &outDevice,
    mi::client::media::AudioPipelineConfig &config) {
    const bool haveIn = !inDevice.isNull();
    const bool haveOut = !outDevice.isNull();
    if (!haveIn && !haveOut) {
        return;
    }
    const bool inOk =
        !haveIn || IsAudioFormatSupported(inDevice, config.sample_rate,
                                          config.channels);
    const bool outOk =
        !haveOut || IsAudioFormatSupported(outDevice, config.sample_rate,
                                           config.channels);
    if (inOk && outOk) {
        return;
    }

    int rate = config.sample_rate;
    int ch = config.channels;
    if (haveIn && haveOut) {
        if (FindCandidateAudioFormat(inDevice, outDevice, true, true, rate, ch)) {
            config.sample_rate = rate;
            config.channels = ch;
            return;
        }
        int prefRate = 0;
        int prefCh = 0;
        if (PickPreferredAudioFormat(inDevice, prefRate, prefCh) &&
            IsAudioFormatSupported(outDevice, prefRate, prefCh)) {
            config.sample_rate = prefRate;
            config.channels = prefCh;
            return;
        }
        if (PickPreferredAudioFormat(outDevice, prefRate, prefCh) &&
            IsAudioFormatSupported(inDevice, prefRate, prefCh)) {
            config.sample_rate = prefRate;
            config.channels = prefCh;
            return;
        }
    }
    if (haveIn) {
        int prefRate = 0;
        int prefCh = 0;
        if (PickPreferredAudioFormat(inDevice, prefRate, prefCh) ||
            FindCandidateAudioFormat(inDevice, outDevice, true, false,
                                     prefRate, prefCh)) {
            config.sample_rate = prefRate;
            config.channels = prefCh;
            return;
        }
    }
    if (haveOut) {
        int prefRate = 0;
        int prefCh = 0;
        if (PickPreferredAudioFormat(outDevice, prefRate, prefCh) ||
            FindCandidateAudioFormat(inDevice, outDevice, false, true,
                                     prefRate, prefCh)) {
            config.sample_rate = prefRate;
            config.channels = prefCh;
            return;
        }
    }
}

}  // namespace

CallController::CallController(mi::client::ClientCore &core, QObject *parent)
    : QObject(parent), core_(core) {
    media_timer_.setInterval(20);
    media_timer_.setTimerType(Qt::PreciseTimer);
    connect(&media_timer_, &QTimer::timeout, this, &CallController::PumpMedia);
}

CallController::~CallController() {
    Stop();
}

bool CallController::Start(const QString &peerUsername,
                           const QString &callIdHex,
                           bool initiator,
                           bool video,
                           QString &outError) {
    outError.clear();
    if (isActive()) {
        outError = QStringLiteral("已有通话进行中");
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

    auto session = std::make_unique<mi::client::media::MediaSession>(core_, cfg);
    std::string err;
    if (!session->Init(err)) {
        outError = err.empty() ? QStringLiteral("通话初始化失败")
                               : QString::fromStdString(err);
        return false;
    }

    media_session_ = std::move(session);
    audio_config_ = mi::client::media::AudioPipelineConfig{};
    const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
    const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
    AdjustAudioConfigForDevices(in_device, out_device, audio_config_);
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

void CallController::Stop() {
    StopMedia();
    if (!active_call_id_.isEmpty()) {
        active_call_id_.clear();
        active_call_peer_.clear();
        active_call_video_ = false;
        emit callStateChanged();
    }
}

void CallController::setLocalVideoSink(QVideoSink *sink) {
    if (sink == local_video_sink_) {
        return;
    }
    if (local_video_sink_) {
        disconnect(local_video_sink_, nullptr, this, nullptr);
    }
    if (!sink) {
        if (!owned_local_sink_) {
            owned_local_sink_ = std::make_unique<QVideoSink>(this);
        }
        local_video_sink_ = owned_local_sink_.get();
    } else {
        owned_local_sink_.reset();
        local_video_sink_ = sink;
    }
    if (auto *session = EnsureCaptureSession()) {
        session->setVideoSink(local_video_sink_);
    }
    if (local_video_sink_) {
        connect(local_video_sink_, &QVideoSink::videoFrameChanged, this,
                &CallController::HandleLocalVideoFrame);
    }
}

void CallController::setRemoteVideoSink(QVideoSink *sink) {
    if (sink == remote_video_sink_) {
        return;
    }
    if (!sink) {
        if (!owned_remote_sink_) {
            owned_remote_sink_ = std::make_unique<QVideoSink>(this);
        }
        remote_video_sink_ = owned_remote_sink_.get();
    } else {
        owned_remote_sink_.reset();
        remote_video_sink_ = sink;
    }
}

void CallController::StartMedia() {
    if (!media_timer_.isActive()) {
        media_timer_.start();
    }
    if (camera_ && !camera_->isActive()) {
        camera_->start();
    }
}

void CallController::StopMedia() {
    if (media_timer_.isActive()) {
        media_timer_.stop();
    }
    ShutdownAudio();
    ShutdownVideo();
    audio_pipeline_.reset();
    video_pipeline_.reset();
    media_session_.reset();
    audio_in_buffer_.clear();
    audio_out_pending_.clear();
    audio_in_offset_ = 0;
    audio_frame_tmp_.clear();
    video_send_buffer_.clear();
    if (remote_video_sink_) {
        remote_video_sink_->setVideoFrame(QVideoFrame());
    }
}

void CallController::PumpMedia() {
    if (!media_session_) {
        return;
    }
    std::string err;
    media_session_->PollIncoming(32, 0, err);

    if (audio_pipeline_) {
        audio_pipeline_->PumpIncoming();
        DrainAudioInput();
        mi::client::media::PcmFrame decoded;
        const int frame_samples = audio_pipeline_->frame_samples();
        const int frame_bytes = frame_samples * static_cast<int>(sizeof(std::int16_t));
        const int max_pending = frame_bytes * 10;
        while (audio_pipeline_->PopDecodedFrame(decoded)) {
            if (!decoded.samples.empty()) {
                const char *ptr =
                    reinterpret_cast<const char *>(decoded.samples.data());
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
            auto buffer = std::make_unique<Nv12VideoBuffer>(
                std::move(latest.nv12), latest.width, latest.height, stride);
            QVideoFrame frame(std::move(buffer));
            frame.setStartTime(static_cast<qint64>(latest.timestamp_ms));
            remote_video_sink_->setVideoFrame(frame);
        }
    }
}

void CallController::DrainAudioInput() {
    if (!audio_pipeline_ || !audio_in_device_) {
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
        const char *src = audio_in_buffer_.constData() + audio_in_offset_;
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

void CallController::FlushAudioOutput() {
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

bool CallController::SetupAudio(QString &outError) {
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
                &CallController::HandleAudioReady);
    }
    return true;
}

bool CallController::SetupVideo(QString &outError) {
    outError.clear();
    const QCameraDevice device = QMediaDevices::defaultVideoInput();
    if (device.isNull()) {
        return true;
    }
    if (!local_video_sink_) {
        owned_local_sink_ = std::make_unique<QVideoSink>(this);
        local_video_sink_ = owned_local_sink_.get();
    }
    QMediaCaptureSession *session = EnsureCaptureSession();
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
                &CallController::HandleLocalVideoFrame);
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

void CallController::ShutdownAudio() {
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

void CallController::ShutdownVideo() {
    if (camera_) {
        camera_->stop();
    }
    if (capture_session_) {
        capture_session_->setVideoSink(nullptr);
        capture_session_->setCamera(nullptr);
    }
    camera_.reset();
}

QMediaCaptureSession *CallController::EnsureCaptureSession() {
    if (!capture_session_) {
        capture_session_ = std::make_unique<QMediaCaptureSession>(this);
    }
    return capture_session_.get();
}

void CallController::HandleAudioReady() {
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

void CallController::HandleLocalVideoFrame(const QVideoFrame &frame) {
    if (!video_pipeline_ || !media_session_) {
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

bool CallController::ConvertVideoFrameToNv12(const QVideoFrame &frame,
                                             std::vector<std::uint8_t> &out,
                                             std::uint32_t &width,
                                             std::uint32_t &height,
                                             std::size_t &stride) const {
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
    std::uint8_t *y_out = out.data();
    std::uint8_t *uv_out = out.data() + y_bytes;
    const auto fmt = mapped.pixelFormat();

    if (fmt == QVideoFrameFormat::Format_NV12 ||
        fmt == QVideoFrameFormat::Format_NV21) {
        const int y_stride = mapped.bytesPerLine(0);
        const int uv_stride = mapped.bytesPerLine(1);
        const std::uint8_t *y_src = mapped.bits(0);
        const std::uint8_t *uv_src = mapped.bits(1);
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
                const std::uint8_t *src = uv_src + row * uv_stride;
                std::uint8_t *dst = uv_out + row * width;
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
        const std::uint8_t *y_src = mapped.bits(0);
        const std::uint8_t *u_src =
            mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 1 : 2);
        const std::uint8_t *v_src =
            mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 2 : 1);
        for (std::uint32_t row = 0; row < height; ++row) {
            std::memcpy(y_out + row * width, y_src + row * y_stride, width);
        }
        const std::uint32_t uv_height = height / 2;
        for (std::uint32_t row = 0; row < uv_height; ++row) {
            const std::uint8_t *u_line = u_src + row * u_stride;
            const std::uint8_t *v_line = v_src + row * v_stride;
            std::uint8_t *dst = uv_out + row * width;
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
        const std::uint8_t *src = mapped.bits(0);
        const std::uint32_t width_even = width & ~1u;
        for (std::uint32_t row = 0; row < height; ++row) {
            const std::uint8_t *line = src + row * src_stride;
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
                    std::uint8_t *dst = uv_out + (row / 2) * width;
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

bool CallController::SelectCameraFormat() {
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
    for (const auto &fmt : formats) {
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
