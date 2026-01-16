#pragma once

#include <QObject>
#include <QByteArray>
#include <QTimer>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "../../include/media_pipeline.h"
#include "../../include/media_session.h"

class QAudioSink;
class QAudioSource;
class QCamera;
class QIODevice;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;
struct mi_client_handle;

class CallController : public QObject {
    Q_OBJECT

public:
    explicit CallController(mi_client_handle* handle, QObject *parent = nullptr);
    ~CallController() override;

    bool Start(const QString &peerUsername,
               const QString &callIdHex,
               bool initiator,
               bool video,
               QString &outError);
    void Stop();

    bool isActive() const { return !active_call_id_.isEmpty(); }
    QString activeCallId() const { return active_call_id_; }
    QString activeCallPeer() const { return active_call_peer_; }
    bool activeCallVideo() const { return active_call_video_; }

    void setLocalVideoSink(QVideoSink *sink);
    void setRemoteVideoSink(QVideoSink *sink);

signals:
    void callStateChanged();

private:
    void StartMedia();
    void StopMedia();
    void PumpMedia();
    void DrainAudioInput();
    void FlushAudioOutput();
    bool SetupAudio(QString &outError);
    bool SetupVideo(QString &outError);
    void ShutdownAudio();
    void ShutdownVideo();
    void HandleAudioReady();
    void HandleLocalVideoFrame(const QVideoFrame &frame);
    bool ConvertVideoFrameToNv12(const QVideoFrame &frame,
                                 std::vector<std::uint8_t> &out,
                                 std::uint32_t &width,
                                 std::uint32_t &height,
                                 std::size_t &stride) const;
    bool SelectCameraFormat();
    QMediaCaptureSession *EnsureCaptureSession();

    mi_client_handle* handle_{nullptr};
    QTimer media_timer_;
    std::unique_ptr<mi::client::media::MediaTransport> media_transport_;
    std::unique_ptr<mi::client::media::MediaSession> media_session_;
    std::unique_ptr<mi::client::media::AudioPipeline> audio_pipeline_;
    std::unique_ptr<mi::client::media::VideoPipeline> video_pipeline_;
    mi::client::media::AudioPipelineConfig audio_config_{};
    mi::client::media::VideoPipelineConfig video_config_{};
    std::unique_ptr<QAudioSource> audio_source_;
    std::unique_ptr<QAudioSink> audio_sink_;
    QIODevice *audio_in_device_{nullptr};
    QIODevice *audio_out_device_{nullptr};
    QByteArray audio_in_buffer_;
    qsizetype audio_in_offset_{0};
    QByteArray audio_out_pending_;
    std::vector<std::int16_t> audio_frame_tmp_;
    std::unique_ptr<QCamera> camera_;
    std::unique_ptr<QMediaCaptureSession> capture_session_;
    QVideoSink *local_video_sink_{nullptr};
    QVideoSink *remote_video_sink_{nullptr};
    std::unique_ptr<QVideoSink> owned_local_sink_;
    std::unique_ptr<QVideoSink> owned_remote_sink_;
    std::vector<std::uint8_t> video_send_buffer_;
    QString active_call_id_;
    QString active_call_peer_;
    bool active_call_video_{false};
};
