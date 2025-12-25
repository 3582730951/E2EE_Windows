#include "media_pipeline.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>
#endif

namespace mi::client::media {

namespace {
constexpr std::uint8_t kAudioPayloadVersion = 1;
constexpr std::uint8_t kVideoPayloadVersion = 1;
constexpr std::uint8_t kVideoFlagKeyframe = 0x01;
constexpr std::size_t kVideoHeaderSize = 8;

std::uint64_t NowMs() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

void WriteUint16Le(std::uint16_t v, std::uint8_t* out) {
  out[0] = static_cast<std::uint8_t>(v & 0xFFu);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

std::uint16_t ReadUint16Le(const std::uint8_t* in) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[0]) |
                                    (static_cast<std::uint16_t>(in[1]) << 8));
}

bool EncodeAudioPayload(AudioCodec codec,
                        const std::uint8_t* data,
                        std::size_t len,
                        std::vector<std::uint8_t>& out) {
  if (!data && len != 0) {
    return false;
  }
  out.resize(2 + len);
  out[0] = kAudioPayloadVersion;
  out[1] = static_cast<std::uint8_t>(codec);
  if (len > 0) {
    std::memcpy(out.data() + 2, data, len);
  }
  return true;
}

bool DecodeAudioPayload(const std::vector<std::uint8_t>& payload,
                        AudioCodec& codec,
                        const std::uint8_t*& data,
                        std::size_t& len) {
  if (payload.size() < 2) {
    return false;
  }
  if (payload[0] != kAudioPayloadVersion) {
    return false;
  }
  codec = static_cast<AudioCodec>(payload[1]);
  data = payload.data() + 2;
  len = payload.size() - 2;
  return true;
}

bool EncodeVideoPayload(VideoCodec codec,
                        bool keyframe,
                        std::uint32_t width,
                        std::uint32_t height,
                        const std::uint8_t* data,
                        std::size_t len,
                        std::vector<std::uint8_t>& out) {
  if (!data && len != 0) {
    return false;
  }
  out.resize(kVideoHeaderSize + len);
  out[0] = kVideoPayloadVersion;
  out[1] = static_cast<std::uint8_t>(codec);
  out[2] = keyframe ? kVideoFlagKeyframe : 0;
  out[3] = 0;
  WriteUint16Le(static_cast<std::uint16_t>(width), out.data() + 4);
  WriteUint16Le(static_cast<std::uint16_t>(height), out.data() + 6);
  if (len > 0) {
    std::memcpy(out.data() + kVideoHeaderSize, data, len);
  }
  return true;
}

bool DecodeVideoPayload(const std::vector<std::uint8_t>& payload,
                        VideoCodec& codec,
                        bool& keyframe,
                        std::uint32_t& width,
                        std::uint32_t& height,
                        const std::uint8_t*& data,
                        std::size_t& len) {
  if (payload.size() < kVideoHeaderSize) {
    return false;
  }
  if (payload[0] != kVideoPayloadVersion) {
    return false;
  }
  codec = static_cast<VideoCodec>(payload[1]);
  keyframe = (payload[2] & kVideoFlagKeyframe) != 0;
  width = ReadUint16Le(payload.data() + 4);
  height = ReadUint16Le(payload.data() + 6);
  data = payload.data() + kVideoHeaderSize;
  len = payload.size() - kVideoHeaderSize;
  return true;
}

int ClampInt(int v, int lo, int hi) {
  return std::max(lo, std::min(hi, v));
}
}  // namespace

class AudioPipeline::OpusCodecImpl {
 public:
  bool Init(int sample_rate,
            int channels,
            int bitrate,
            bool enable_fec,
            bool enable_dtx,
            int loss_pct,
            std::string& error) {
    error.clear();
#ifdef _WIN32
    if (!LoadLibraryHandles(error)) {
      return false;
    }
#else
    error = "opus not available";
    return false;
#endif
    if (!create_encoder_ || !create_decoder_ || !encode_ || !decode_ ||
        !destroy_encoder_ || !destroy_decoder_ || !encoder_ctl_) {
      error = "opus symbols missing";
      return false;
    }
    int err = 0;
    enc_ = create_encoder_(sample_rate, channels, kOpusAppVoip, &err);
    if (!enc_ || err != 0) {
      error = "opus encoder init failed";
      return false;
    }
    dec_ = create_decoder_(sample_rate, channels, &err);
    if (!dec_ || err != 0) {
      error = "opus decoder init failed";
      return false;
    }
    channels_ = channels;
    frame_samples_ = sample_rate / 1000 * 20;
    SetBitrate(bitrate);
    encoder_ctl_(enc_, kOpusSetInbandFec, enable_fec ? 1 : 0);
    encoder_ctl_(enc_, kOpusSetPacketLossPerc, ClampInt(loss_pct, 0, 20));
    encoder_ctl_(enc_, kOpusSetDtx, enable_dtx ? 1 : 0);
    return true;
  }

  void Shutdown() {
    if (destroy_encoder_ && enc_) {
      destroy_encoder_(enc_);
    }
    if (destroy_decoder_ && dec_) {
      destroy_decoder_(dec_);
    }
    enc_ = nullptr;
    dec_ = nullptr;
#ifdef _WIN32
    if (lib_) {
      FreeLibrary(lib_);
      lib_ = nullptr;
    }
#endif
  }

  bool Encode(const std::int16_t* pcm,
              int frame_samples,
              std::vector<std::uint8_t>& out) {
    if (!enc_ || !pcm) {
      return false;
    }
    const int max_packet = 4000;
    out.resize(static_cast<std::size_t>(max_packet));
    const int n = encode_(enc_, pcm, frame_samples, out.data(), max_packet);
    if (n < 0) {
      return false;
    }
    out.resize(static_cast<std::size_t>(n));
    return true;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              int frame_samples,
              std::vector<std::int16_t>& out) {
    if (!dec_ || (!data && len != 0)) {
      return false;
    }
    out.resize(static_cast<std::size_t>(frame_samples * channels_));
    const int n = decode_(dec_, data, static_cast<int>(len), out.data(),
                          frame_samples, 0);
    if (n < 0) {
      return false;
    }
    out.resize(static_cast<std::size_t>(n * channels_));
    return true;
  }

  bool SetBitrate(int bitrate) {
    if (!enc_) {
      return false;
    }
    return encoder_ctl_(enc_, kOpusSetBitrate, bitrate) == 0;
  }

 private:
#ifdef _WIN32
  bool LoadLibraryHandles(std::string& error) {
    const wchar_t* names[] = {L"opus.dll", L"libopus-0.dll", L"libopus.dll"};
    for (const auto* name : names) {
      lib_ = LoadLibraryW(name);
      if (lib_) {
        break;
      }
    }
    if (!lib_) {
      error = "opus dll not found";
      return false;
    }
    create_encoder_ = reinterpret_cast<OpusEncoderCreate>(
        GetProcAddress(lib_, "opus_encoder_create"));
    create_decoder_ = reinterpret_cast<OpusDecoderCreate>(
        GetProcAddress(lib_, "opus_decoder_create"));
    destroy_encoder_ = reinterpret_cast<OpusEncoderDestroy>(
        GetProcAddress(lib_, "opus_encoder_destroy"));
    destroy_decoder_ = reinterpret_cast<OpusDecoderDestroy>(
        GetProcAddress(lib_, "opus_decoder_destroy"));
    encode_ = reinterpret_cast<OpusEncode>(
        GetProcAddress(lib_, "opus_encode"));
    decode_ = reinterpret_cast<OpusDecode>(
        GetProcAddress(lib_, "opus_decode"));
    encoder_ctl_ = reinterpret_cast<OpusEncoderCtl>(
        GetProcAddress(lib_, "opus_encoder_ctl"));
    return true;
  }
#endif

  struct OpusEncoder;
  struct OpusDecoder;
  using OpusEncoderCreate = OpusEncoder* (*)(int, int, int, int*);
  using OpusDecoderCreate = OpusDecoder* (*)(int, int, int*);
  using OpusEncoderDestroy = void (*)(OpusEncoder*);
  using OpusDecoderDestroy = void (*)(OpusDecoder*);
  using OpusEncode = int (*)(OpusEncoder*, const std::int16_t*, int,
                             std::uint8_t*, int);
  using OpusDecode = int (*)(OpusDecoder*, const std::uint8_t*, int,
                             std::int16_t*, int, int);
  using OpusEncoderCtl = int (*)(OpusEncoder*, int, ...);

  static constexpr int kOpusAppVoip = 2048;
  static constexpr int kOpusSetBitrate = 4002;
  static constexpr int kOpusSetInbandFec = 4012;
  static constexpr int kOpusSetPacketLossPerc = 4014;
  static constexpr int kOpusSetDtx = 4016;

#ifdef _WIN32
  HMODULE lib_{nullptr};
#endif
  OpusEncoder* enc_{nullptr};
  OpusDecoder* dec_{nullptr};
  int channels_{1};
  int frame_samples_{0};
  OpusEncoderCreate create_encoder_{nullptr};
  OpusDecoderCreate create_decoder_{nullptr};
  OpusEncoderDestroy destroy_encoder_{nullptr};
  OpusDecoderDestroy destroy_decoder_{nullptr};
  OpusEncode encode_{nullptr};
  OpusDecode decode_{nullptr};
  OpusEncoderCtl encoder_ctl_{nullptr};
};

AudioPipeline::AudioPipeline(MediaSession& session, AudioPipelineConfig config)
    : session_(session), config_(std::move(config)) {}

AudioPipeline::~AudioPipeline() = default;

bool AudioPipeline::Init(std::string& error) {
  error.clear();
  ready_ = false;
  if (config_.sample_rate <= 0 || config_.channels <= 0 ||
      config_.frame_ms <= 0) {
    error = "audio config invalid";
    return false;
  }
  frame_samples_ =
      config_.sample_rate * config_.frame_ms / 1000 * config_.channels;
  if (frame_samples_ <= 0) {
    error = "audio frame samples invalid";
    return false;
  }
  current_bitrate_bps_ =
      ClampInt(config_.target_bitrate_bps, config_.min_bitrate_bps,
               config_.max_bitrate_bps);
  opus_ = std::make_unique<OpusCodecImpl>();
  if (opus_->Init(config_.sample_rate, config_.channels, current_bitrate_bps_,
                  config_.enable_fec, config_.enable_dtx,
                  config_.max_packet_loss, error)) {
    codec_ = AudioCodec::kOpus;
    ready_ = true;
    return true;
  }
  if (!config_.allow_pcm_fallback) {
    return false;
  }
  opus_.reset();
  codec_ = AudioCodec::kPcm16;
  ready_ = true;
  return true;
}

bool AudioPipeline::SendPcmFrame(const std::int16_t* samples,
                                 std::size_t sample_count) {
  if (!ready_ || !samples) {
    return false;
  }
  if (sample_count != static_cast<std::size_t>(frame_samples_)) {
    return false;
  }
  std::vector<std::uint8_t> encoded;
  if (codec_ == AudioCodec::kOpus) {
    if (!opus_ || !opus_->Encode(samples, frame_samples_, encoded)) {
      return false;
    }
  } else {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(samples);
    encoded.assign(ptr, ptr + sample_count * sizeof(std::int16_t));
  }
  std::vector<std::uint8_t> payload;
  if (!EncodeAudioPayload(codec_, encoded.data(), encoded.size(), payload)) {
    return false;
  }
  return session_.SendAudioFrame(payload, NowMs(), 0);
}

void AudioPipeline::PumpIncoming() {
  if (!ready_) {
    return;
  }
  const auto now_ms = NowMs();
  mi::media::MediaFrame frame;
  while (session_.PopAudioFrame(now_ms, frame)) {
    AudioCodec codec;
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    if (!DecodeAudioPayload(frame.payload, codec, data, len)) {
      continue;
    }
    PcmFrame decoded;
    decoded.timestamp_ms = frame.timestamp_ms;
    if (codec == AudioCodec::kOpus) {
      if (!opus_ ||
          !opus_->Decode(data, len, frame_samples_, decoded.samples)) {
        continue;
      }
    } else if (codec == AudioCodec::kPcm16) {
      const std::size_t samples = len / sizeof(std::int16_t);
      decoded.samples.resize(samples);
      if (!decoded.samples.empty()) {
        std::memcpy(decoded.samples.data(), data,
                    samples * sizeof(std::int16_t));
      }
    } else {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      decoded_.push_back(std::move(decoded));
      while (decoded_.size() > config_.max_decoded_frames) {
        decoded_.pop_front();
      }
    }
  }
  AdaptBitrate(now_ms);
}

bool AudioPipeline::PopDecodedFrame(PcmFrame& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (decoded_.empty()) {
    return false;
  }
  out = std::move(decoded_.front());
  decoded_.pop_front();
  return true;
}

void AudioPipeline::AdaptBitrate(std::uint64_t now_ms) {
  if (codec_ != AudioCodec::kOpus || !opus_) {
    return;
  }
  if (now_ms - last_adapt_ms_ < 1000) {
    return;
  }
  const auto stats = session_.stats();
  const auto jitter = session_.audio_jitter_stats();
  const auto recv_delta =
      stats.audio.frames_recv - last_stats_.audio.frames_recv;
  const auto drop_delta =
      stats.audio.frames_drop - last_stats_.audio.frames_drop +
      jitter.dropped - last_jitter_.dropped +
      jitter.late - last_jitter_.late;
  double drop_ratio = 0.0;
  if (recv_delta > 0) {
    drop_ratio = static_cast<double>(drop_delta) /
                 static_cast<double>(recv_delta);
  }
  int bitrate = current_bitrate_bps_;
  if (drop_ratio > 0.10) {
    bitrate = ClampInt(bitrate * 8 / 10, config_.min_bitrate_bps,
                       config_.max_bitrate_bps);
  } else if (drop_ratio < 0.02 && recv_delta >= 30) {
    bitrate = ClampInt(bitrate * 11 / 10, config_.min_bitrate_bps,
                       config_.max_bitrate_bps);
  }
  if (bitrate != current_bitrate_bps_) {
    if (opus_->SetBitrate(bitrate)) {
      current_bitrate_bps_ = bitrate;
    }
  }
  last_stats_ = stats;
  last_jitter_ = jitter;
  last_adapt_ms_ = now_ms;
}

#ifdef _WIN32
class VideoPipeline::MfVideoCodecImpl {
 public:
  bool Init(std::uint32_t width,
            std::uint32_t height,
            std::uint32_t fps,
            std::uint32_t bitrate,
            std::string& error) {
    error.clear();
    if (!EnsureStartup(error)) {
      return false;
    }
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    frame_duration_100ns_ =
        fps_ == 0 ? 0 : static_cast<LONGLONG>(10000000ull / fps_);
    if (!CreateEncoder(error)) {
      return false;
    }
    if (!CreateDecoder(error)) {
      return false;
    }
    return true;
  }

  bool Encode(const std::uint8_t* nv12,
              std::size_t stride,
              bool keyframe,
              std::vector<std::uint8_t>& out,
              std::uint64_t timestamp_ms) {
    if (!encoder_ || !nv12) {
      return false;
    }
    if (keyframe) {
      ForceKeyframe();
    }
    const std::size_t y_bytes = stride * height_;
    const std::size_t uv_bytes = stride * height_ / 2;
    const std::size_t total = y_bytes + uv_bytes;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(MFCreateSample(&sample)) ||
        FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(total), &buffer))) {
      return false;
    }
    std::uint8_t* dst = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(buffer->Lock(reinterpret_cast<BYTE**>(&dst), &max_len,
                            &cur_len))) {
      return false;
    }
    std::memcpy(dst, nv12, total);
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(total));
    sample->AddBuffer(buffer.Get());
    const LONGLONG ts = static_cast<LONGLONG>(timestamp_ms) * 10000;
    sample->SetSampleTime(ts);
    sample->SetSampleDuration(frame_duration_100ns_);
    if (FAILED(encoder_->ProcessInput(0, sample.Get(), 0))) {
      return false;
    }
    out.clear();
    for (;;) {
      MFT_OUTPUT_STREAM_INFO info{};
      if (FAILED(encoder_->GetOutputStreamInfo(0, &info))) {
        return false;
      }
      Microsoft::WRL::ComPtr<IMFSample> out_sample;
      Microsoft::WRL::ComPtr<IMFMediaBuffer> out_buf;
      if (FAILED(MFCreateSample(&out_sample)) ||
          FAILED(MFCreateMemoryBuffer(info.cbSize, &out_buf))) {
        return false;
      }
      out_sample->AddBuffer(out_buf.Get());
      MFT_OUTPUT_DATA_BUFFER output{};
      output.pSample = out_sample.Get();
      DWORD status = 0;
      const HRESULT hr = encoder_->ProcessOutput(0, 1, &output, &status);
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        break;
      }
      if (FAILED(hr)) {
        return false;
      }
      std::uint8_t* data = nullptr;
      DWORD max_len2 = 0;
      DWORD cur_len2 = 0;
      if (FAILED(out_buf->Lock(reinterpret_cast<BYTE**>(&data), &max_len2,
                               &cur_len2))) {
        return false;
      }
      out.insert(out.end(), data, data + cur_len2);
      out_buf->Unlock();
      if (output.pEvents) {
        output.pEvents->Release();
      }
    }
    return !out.empty();
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              std::vector<std::uint8_t>& out,
              std::uint64_t timestamp_ms) {
    if (!decoder_ || (!data && len != 0)) {
      return false;
    }
    Microsoft::WRL::ComPtr<IMFSample> sample;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(MFCreateSample(&sample)) ||
        FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(len), &buffer))) {
      return false;
    }
    std::uint8_t* dst = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(buffer->Lock(reinterpret_cast<BYTE**>(&dst), &max_len,
                            &cur_len))) {
      return false;
    }
    if (len > 0) {
      std::memcpy(dst, data, len);
    }
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(len));
    sample->AddBuffer(buffer.Get());
    const LONGLONG ts = static_cast<LONGLONG>(timestamp_ms) * 10000;
    sample->SetSampleTime(ts);
    sample->SetSampleDuration(frame_duration_100ns_);
    if (FAILED(decoder_->ProcessInput(0, sample.Get(), 0))) {
      return false;
    }
    out.clear();
    for (;;) {
      MFT_OUTPUT_STREAM_INFO info{};
      if (FAILED(decoder_->GetOutputStreamInfo(0, &info))) {
        return false;
      }
      Microsoft::WRL::ComPtr<IMFSample> out_sample;
      Microsoft::WRL::ComPtr<IMFMediaBuffer> out_buf;
      if (FAILED(MFCreateSample(&out_sample)) ||
          FAILED(MFCreateMemoryBuffer(info.cbSize, &out_buf))) {
        return false;
      }
      out_sample->AddBuffer(out_buf.Get());
      MFT_OUTPUT_DATA_BUFFER output{};
      output.pSample = out_sample.Get();
      DWORD status = 0;
      const HRESULT hr = decoder_->ProcessOutput(0, 1, &output, &status);
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        break;
      }
      if (FAILED(hr)) {
        return false;
      }
      std::uint8_t* data_ptr = nullptr;
      DWORD max_len2 = 0;
      DWORD cur_len2 = 0;
      if (FAILED(out_buf->Lock(reinterpret_cast<BYTE**>(&data_ptr), &max_len2,
                               &cur_len2))) {
        return false;
      }
      out.insert(out.end(), data_ptr, data_ptr + cur_len2);
      out_buf->Unlock();
      if (output.pEvents) {
        output.pEvents->Release();
      }
    }
    return !out.empty();
  }

  bool SetBitrate(std::uint32_t bitrate) {
    if (!encoder_) {
      return false;
    }
#if defined(__ICodecAPI_INTERFACE_DEFINED__)
    Microsoft::WRL::ComPtr<ICodecAPI> api;
    if (FAILED(encoder_.As(&api))) {
      return false;
    }
    VARIANT v{};
    v.vt = VT_UI4;
    v.ulVal = bitrate;
    if (FAILED(api->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &v))) {
      return false;
    }
    bitrate_ = bitrate;
    return true;
#else
    (void)bitrate;
    return false;
#endif
  }

 private:
  bool EnsureStartup(std::string& error) {
    static std::once_flag once;
    static HRESULT hr = S_OK;
    std::call_once(once, [] { hr = MFStartup(MF_VERSION); });
    if (FAILED(hr)) {
      error = "MFStartup failed";
      return false;
    }
    return true;
  }

  bool CreateEncoder(std::string& error) {
    Microsoft::WRL::ComPtr<IMFActivate> activate;
    UINT32 count = 0;
    MFT_REGISTER_TYPE_INFO input_info{MFMediaType_Video, MFVideoFormat_NV12};
    MFT_REGISTER_TYPE_INFO output_info{MFMediaType_Video, MFVideoFormat_H264};
    IMFActivate** activates = nullptr;
    HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
                           MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                           &input_info, &output_info, &activates, &count);
    if (FAILED(hr) || count == 0) {
      hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_SORTANDFILTER,
                     &input_info, &output_info, &activates, &count);
    }
    if (FAILED(hr) || count == 0) {
      error = "h264 encoder not found";
      return false;
    }
    activate = activates[0];
    for (UINT32 i = 0; i < count; ++i) {
      if (activates[i] != activate.Get()) {
        activates[i]->Release();
      }
    }
    CoTaskMemFree(activates);
    if (FAILED(activate->ActivateObject(IID_PPV_ARGS(&encoder_)))) {
      error = "encoder activate failed";
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> input_type;
    MFCreateMediaType(&input_type);
    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    input_type->SetUINT32(MF_MT_INTERLACE_MODE,
                          MFVideoInterlace_Progressive);
    if (FAILED(encoder_->SetInputType(0, input_type.Get(), 0))) {
      error = "encoder input type failed";
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> output_type;
    MFCreateMediaType(&output_type);
    output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    MFSetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    output_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate_);
    output_type->SetUINT32(MF_MT_INTERLACE_MODE,
                           MFVideoInterlace_Progressive);
    output_type->SetUINT32(MF_MT_MPEG2_PROFILE,
                           eAVEncH264VProfile_Base);
    if (FAILED(encoder_->SetOutputType(0, output_type.Get(), 0))) {
      error = "encoder output type failed";
      return false;
    }
    encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    return true;
  }

  bool CreateDecoder(std::string& error) {
    Microsoft::WRL::ComPtr<IMFActivate> activate;
    UINT32 count = 0;
    MFT_REGISTER_TYPE_INFO input_info{MFMediaType_Video, MFVideoFormat_H264};
    MFT_REGISTER_TYPE_INFO output_info{MFMediaType_Video, MFVideoFormat_NV12};
    IMFActivate** activates = nullptr;
    HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                           MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                           &input_info, &output_info, &activates, &count);
    if (FAILED(hr) || count == 0) {
      hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SORTANDFILTER,
                     &input_info, &output_info, &activates, &count);
    }
    if (FAILED(hr) || count == 0) {
      error = "h264 decoder not found";
      return false;
    }
    activate = activates[0];
    for (UINT32 i = 0; i < count; ++i) {
      if (activates[i] != activate.Get()) {
        activates[i]->Release();
      }
    }
    CoTaskMemFree(activates);
    if (FAILED(activate->ActivateObject(IID_PPV_ARGS(&decoder_)))) {
      error = "decoder activate failed";
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> input_type;
    MFCreateMediaType(&input_type);
    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    input_type->SetUINT32(MF_MT_INTERLACE_MODE,
                          MFVideoInterlace_Progressive);
    if (FAILED(decoder_->SetInputType(0, input_type.Get(), 0))) {
      error = "decoder input type failed";
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> output_type;
    MFCreateMediaType(&output_type);
    output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    output_type->SetUINT32(MF_MT_INTERLACE_MODE,
                           MFVideoInterlace_Progressive);
    if (FAILED(decoder_->SetOutputType(0, output_type.Get(), 0))) {
      error = "decoder output type failed";
      return false;
    }
    decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    return true;
  }

  void ForceKeyframe() {
#if defined(__ICodecAPI_INTERFACE_DEFINED__)
    Microsoft::WRL::ComPtr<ICodecAPI> api;
    if (FAILED(encoder_.As(&api))) {
      return;
    }
    VARIANT v{};
    v.vt = VT_UI4;
    v.ulVal = 1;
    api->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &v);
#endif
  }

  Microsoft::WRL::ComPtr<IMFTransform> encoder_;
  Microsoft::WRL::ComPtr<IMFTransform> decoder_;
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::uint32_t fps_{0};
  std::uint32_t bitrate_{0};
  LONGLONG frame_duration_100ns_{0};
};
#else
class VideoPipeline::MfVideoCodecImpl {
 public:
  bool Init(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
            std::string& error) {
    error = "media foundation not available";
    return false;
  }
  bool Encode(const std::uint8_t*, std::size_t, bool,
              std::vector<std::uint8_t>&, std::uint64_t) {
    return false;
  }
  bool Decode(const std::uint8_t*, std::size_t,
              std::vector<std::uint8_t>&, std::uint64_t) {
    return false;
  }
  bool SetBitrate(std::uint32_t) { return false; }
};
#endif

VideoPipeline::VideoPipeline(MediaSession& session, VideoPipelineConfig config)
    : session_(session), config_(std::move(config)) {}

VideoPipeline::~VideoPipeline() = default;

bool VideoPipeline::Init(std::string& error) {
  error.clear();
  ready_ = false;
  if (config_.width == 0 || config_.height == 0 || config_.fps == 0) {
    error = "video config invalid";
    return false;
  }
  current_bitrate_bps_ =
      std::max(config_.min_bitrate_bps,
               std::min(config_.target_bitrate_bps,
                        config_.max_bitrate_bps));
  mf_ = std::make_unique<MfVideoCodecImpl>();
  if (mf_->Init(config_.width, config_.height, config_.fps,
                current_bitrate_bps_, error)) {
    codec_ = VideoCodec::kH264;
    ready_ = true;
    return true;
  }
  if (!config_.allow_raw_fallback) {
    return false;
  }
  mf_.reset();
  codec_ = VideoCodec::kRawNv12;
  ready_ = true;
  return true;
}

bool VideoPipeline::SendNv12Frame(const std::uint8_t* data,
                                  std::size_t stride,
                                  std::uint32_t width,
                                  std::uint32_t height) {
  if (!ready_ || !data || width == 0 || height == 0) {
    return false;
  }
  if (stride == 0) {
    stride = width;
  }
  if (stride < width) {
    return false;
  }
  const auto now_ms = NowMs();
  const std::uint64_t interval_ms =
      config_.fps == 0 ? 0 : (1000ull / config_.fps);
  if (interval_ms > 0 && now_ms - last_send_ms_ < interval_ms) {
    return false;
  }
  last_send_ms_ = now_ms;
  const bool keyframe =
      config_.keyframe_interval_ms > 0 &&
      (now_ms - last_keyframe_ms_ >= config_.keyframe_interval_ms);
  if (keyframe) {
    last_keyframe_ms_ = now_ms;
  }
  const std::uint8_t* src = data;
  std::size_t src_stride = stride;
  if (stride != width) {
    const std::size_t y_bytes = static_cast<std::size_t>(width) * height;
    const std::size_t uv_bytes = y_bytes / 2;
    encode_scratch_.resize(y_bytes + uv_bytes);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(encode_scratch_.data() + row * width,
                  data + row * stride, width);
    }
    const std::uint8_t* uv_src = data + stride * height;
    std::uint8_t* uv_dst = encode_scratch_.data() + y_bytes;
    const std::uint32_t uv_height = height / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      std::memcpy(uv_dst + row * width, uv_src + row * stride, width);
    }
    src = encode_scratch_.data();
    src_stride = width;
  }
  std::vector<std::uint8_t> encoded;
  if (codec_ == VideoCodec::kH264) {
    if (!EncodeFrame(src, src_stride, width, height, keyframe, encoded)) {
      return false;
    }
  } else {
    const std::size_t y_bytes = src_stride * height;
    const std::size_t uv_bytes = y_bytes / 2;
    encoded.assign(src, src + y_bytes + uv_bytes);
  }
  std::vector<std::uint8_t> payload;
  if (!EncodeVideoPayload(codec_, keyframe, width, height, encoded.data(),
                          encoded.size(), payload)) {
    return false;
  }
  const std::uint8_t flags = keyframe ? mi::media::kFrameKey : 0;
  return session_.SendVideoFrame(payload, now_ms, flags);
}

void VideoPipeline::PumpIncoming() {
  if (!ready_) {
    return;
  }
  const auto now_ms = NowMs();
  mi::media::MediaFrame frame;
  while (session_.PopVideoFrame(now_ms, frame)) {
    VideoCodec codec;
    bool keyframe = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const std::uint8_t* data = nullptr;
    std::size_t len = 0;
    if (!DecodeVideoPayload(frame.payload, codec, keyframe, width, height, data,
                            len)) {
      continue;
    }
    VideoFrameData decoded;
    decoded.timestamp_ms = frame.timestamp_ms;
    decoded.keyframe = keyframe;
    decoded.width = width;
    decoded.height = height;
    if (codec == VideoCodec::kH264) {
      if (!DecodeFrame(data, len, width, height, decoded.nv12)) {
        continue;
      }
    } else if (codec == VideoCodec::kRawNv12) {
      decoded.nv12.assign(data, data + len);
    } else {
      continue;
    }
    if (width > 0 && height > 0 && !decoded.nv12.empty()) {
      const std::size_t expected =
          static_cast<std::size_t>(width) * height * 3 / 2;
      if (decoded.nv12.size() == expected) {
        decoded.stride = width;
      } else {
        const std::size_t denom = static_cast<std::size_t>(height) * 3;
        const std::size_t maybe_stride =
            denom == 0 ? 0 : decoded.nv12.size() * 2 / denom;
        decoded.stride = maybe_stride >= width
                             ? static_cast<std::uint32_t>(maybe_stride)
                             : width;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      decoded_.push_back(std::move(decoded));
      while (decoded_.size() > config_.max_decoded_frames) {
        decoded_.pop_front();
      }
    }
  }
  AdaptBitrate(now_ms);
}

bool VideoPipeline::PopDecodedFrame(VideoFrameData& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (decoded_.empty()) {
    return false;
  }
  out = std::move(decoded_.front());
  decoded_.pop_front();
  return true;
}

void VideoPipeline::AdaptBitrate(std::uint64_t now_ms) {
  if (codec_ != VideoCodec::kH264 || !mf_) {
    return;
  }
  if (now_ms - last_adapt_ms_ < 1000) {
    return;
  }
  const auto stats = session_.stats();
  const auto jitter = session_.video_jitter_stats();
  const auto recv_delta =
      stats.video.frames_recv - last_stats_.video.frames_recv;
  const auto drop_delta =
      stats.video.frames_drop - last_stats_.video.frames_drop +
      jitter.dropped - last_jitter_.dropped +
      jitter.late - last_jitter_.late;
  double drop_ratio = 0.0;
  if (recv_delta > 0) {
    drop_ratio = static_cast<double>(drop_delta) /
                 static_cast<double>(recv_delta);
  }
  std::uint32_t bitrate = current_bitrate_bps_;
  if (drop_ratio > 0.10) {
    bitrate = std::max(config_.min_bitrate_bps, bitrate * 8 / 10);
  } else if (drop_ratio < 0.02 && recv_delta >= 10) {
    bitrate = std::min(config_.max_bitrate_bps, bitrate * 11 / 10);
  }
  if (bitrate != current_bitrate_bps_) {
    if (mf_->SetBitrate(bitrate)) {
      current_bitrate_bps_ = bitrate;
    }
  }
  last_stats_ = stats;
  last_jitter_ = jitter;
  last_adapt_ms_ = now_ms;
}

bool VideoPipeline::EncodeFrame(const std::uint8_t* data,
                                std::size_t stride,
                                std::uint32_t width,
                                std::uint32_t height,
                                bool keyframe,
                                std::vector<std::uint8_t>& out) {
  if (!mf_) {
    return false;
  }
  if (width != config_.width || height != config_.height) {
    std::string err;
    mf_->Init(width, height, config_.fps, current_bitrate_bps_, err);
    config_.width = width;
    config_.height = height;
  }
  return mf_->Encode(data, stride, keyframe, out, NowMs());
}

bool VideoPipeline::DecodeFrame(const std::uint8_t* data,
                                std::size_t len,
                                std::uint32_t width,
                                std::uint32_t height,
                                std::vector<std::uint8_t>& out) {
  if (!mf_) {
    return false;
  }
  if (width != config_.width || height != config_.height) {
    std::string err;
    mf_->Init(width, height, config_.fps, current_bitrate_bps_, err);
    config_.width = width;
    config_.height = height;
  }
  return mf_->Decode(data, len, out, NowMs());
}

}  // namespace mi::client::media
