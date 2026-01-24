#include "platform_media.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>

#include <cstring>
#include <mutex>
#include <vector>

namespace mi::platform::media {

class OpusCodecWin final : public OpusCodec {
 public:
  ~OpusCodecWin() override { Shutdown(); }

  bool Init(int sample_rate,
            int channels,
            int bitrate,
            bool enable_fec,
            bool enable_dtx,
            int loss_pct,
            std::string& error) override {
    error.clear();
    if (!LoadLibraryHandles(error)) {
      return false;
    }
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

  void Shutdown() override {
    if (destroy_encoder_ && enc_) {
      destroy_encoder_(enc_);
    }
    if (destroy_decoder_ && dec_) {
      destroy_decoder_(dec_);
    }
    enc_ = nullptr;
    dec_ = nullptr;
    if (lib_) {
      FreeLibrary(lib_);
      lib_ = nullptr;
    }
  }

  bool EncodeInto(const std::int16_t* pcm,
                  int frame_samples,
                  std::uint8_t* out,
                  int max_len,
                  std::size_t& out_len) override {
    out_len = 0;
    if (!enc_ || !pcm || !out || max_len <= 0) {
      return false;
    }
    const int n = encode_(enc_, pcm, frame_samples, out, max_len);
    if (n < 0) {
      return false;
    }
    out_len = static_cast<std::size_t>(n);
    return true;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              int frame_samples,
              std::vector<std::int16_t>& out) override {
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

  bool SetBitrate(int bitrate) override {
    if (!enc_) {
      return false;
    }
    return encoder_ctl_(enc_, kOpusSetBitrate, bitrate) == 0;
  }

 private:
  static int ClampInt(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
  }

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

  HMODULE lib_{nullptr};
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

namespace {

constexpr std::uint8_t kAnnexBPrefix[4] = {0, 0, 0, 1};

bool HasAnnexBStartCode(const std::uint8_t* data, std::size_t len) {
  if (!data || len < 3) {
    return false;
  }
  for (std::size_t i = 0; i + 3 < len; ++i) {
    if (data[i] == 0 && data[i + 1] == 0) {
      if (data[i + 2] == 1) {
        return true;
      }
      if (i + 3 < len && data[i + 2] == 0 && data[i + 3] == 1) {
        return true;
      }
    }
  }
  return false;
}

bool AnnexBHasParameterSets(const std::vector<std::uint8_t>& data) {
  if (data.size() < 4) {
    return false;
  }
  for (std::size_t i = 0; i + 3 < data.size(); ++i) {
    if (data[i] == 0 && data[i + 1] == 0 &&
        ((data[i + 2] == 1) ||
         (i + 3 < data.size() && data[i + 2] == 0 && data[i + 3] == 1))) {
      const std::size_t start = (data[i + 2] == 1) ? (i + 3) : (i + 4);
      if (start < data.size()) {
        const std::uint8_t type = data[start] & 0x1F;
        if (type == 7 || type == 8) {
          return true;
        }
      }
    }
  }
  return false;
}

bool ConvertAvccToAnnexB(const std::uint8_t* data,
                         std::size_t len,
                         std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len < 4) {
    return false;
  }
  std::size_t off = 0;
  while (off + 4 <= len) {
    const std::uint32_t nalu_len =
        (static_cast<std::uint32_t>(data[off]) << 24) |
        (static_cast<std::uint32_t>(data[off + 1]) << 16) |
        (static_cast<std::uint32_t>(data[off + 2]) << 8) |
        (static_cast<std::uint32_t>(data[off + 3]));
    off += 4;
    if (nalu_len == 0 || off + nalu_len > len) {
      out.clear();
      return false;
    }
    out.insert(out.end(), kAnnexBPrefix, kAnnexBPrefix + sizeof(kAnnexBPrefix));
    out.insert(out.end(), data + off, data + off + nalu_len);
    off += nalu_len;
  }
  return off == len && !out.empty();
}

bool AvccExtradataToAnnexB(const std::uint8_t* data,
                           std::size_t len,
                           std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len < 7) {
    return false;
  }
  std::size_t off = 5;
  const std::uint8_t num_sps = data[off] & 0x1F;
  off += 1;
  for (std::uint8_t i = 0; i < num_sps; ++i) {
    if (off + 2 > len) {
      out.clear();
      return false;
    }
    const std::uint16_t sps_len =
        static_cast<std::uint16_t>(data[off] << 8 | data[off + 1]);
    off += 2;
    if (sps_len == 0 || off + sps_len > len) {
      out.clear();
      return false;
    }
    out.insert(out.end(), kAnnexBPrefix, kAnnexBPrefix + sizeof(kAnnexBPrefix));
    out.insert(out.end(), data + off, data + off + sps_len);
    off += sps_len;
  }
  if (off >= len) {
    return !out.empty();
  }
  const std::uint8_t num_pps = data[off++];
  for (std::uint8_t i = 0; i < num_pps; ++i) {
    if (off + 2 > len) {
      out.clear();
      return false;
    }
    const std::uint16_t pps_len =
        static_cast<std::uint16_t>(data[off] << 8 | data[off + 1]);
    off += 2;
    if (pps_len == 0 || off + pps_len > len) {
      out.clear();
      return false;
    }
    out.insert(out.end(), kAnnexBPrefix, kAnnexBPrefix + sizeof(kAnnexBPrefix));
    out.insert(out.end(), data + off, data + off + pps_len);
    off += pps_len;
  }
  return !out.empty();
}

bool AppendSampleBytes(IMFSample* sample, std::vector<std::uint8_t>& out) {
  out.clear();
  if (!sample) {
    return false;
  }
  DWORD count = 0;
  if (FAILED(sample->GetBufferCount(&count)) || count == 0) {
    return false;
  }
  for (DWORD i = 0; i < count; ++i) {
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->GetBufferByIndex(i, &buffer))) {
      out.clear();
      return false;
    }
    BYTE* data = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(buffer->Lock(&data, &max_len, &cur_len))) {
      out.clear();
      return false;
    }
    if (cur_len > 0) {
      out.insert(out.end(), data, data + cur_len);
    }
    buffer->Unlock();
  }
  return !out.empty();
}

bool SampleIsKeyframe(IMFSample* sample) {
  if (!sample) {
    return false;
  }
  UINT32 clean = 0;
  if (SUCCEEDED(sample->GetUINT32(MFSampleExtension_CleanPoint, &clean))) {
    return clean != 0;
  }
  return false;
}

}  // namespace

class H264CodecWin final : public H264Codec {
 public:
  bool Init(std::uint32_t width,
            std::uint32_t height,
            std::uint32_t fps,
            std::uint32_t bitrate,
            std::string& error) override {
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
              std::uint64_t timestamp_ms) override {
    if (!encoder_ || !nv12) {
      return false;
    }
    if (stride < width_) {
      return false;
    }
    if (keyframe) {
      ForceKeyframe();
    }
    const std::size_t y_bytes = stride * height_;
    const std::size_t uv_bytes = stride * height_ / 2;
    const std::size_t total = y_bytes + uv_bytes;
    if (!enc_in_buf_ || enc_in_capacity_ < total) {
      enc_in_sample_.Reset();
      enc_in_buf_.Reset();
      if (FAILED(MFCreateSample(&enc_in_sample_)) ||
          FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(total),
                                      &enc_in_buf_))) {
        return false;
      }
      enc_in_capacity_ = static_cast<DWORD>(total);
      enc_in_sample_->AddBuffer(enc_in_buf_.Get());
    }
    std::uint8_t* dst = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(enc_in_buf_->Lock(reinterpret_cast<BYTE**>(&dst), &max_len,
                                 &cur_len))) {
      return false;
    }
    std::memcpy(dst, nv12, total);
    enc_in_buf_->Unlock();
    enc_in_buf_->SetCurrentLength(static_cast<DWORD>(total));
    const LONGLONG ts = static_cast<LONGLONG>(timestamp_ms) * 10000;
    enc_in_sample_->SetSampleTime(ts);
    enc_in_sample_->SetSampleDuration(frame_duration_100ns_);
    HRESULT input_hr = encoder_->ProcessInput(0, enc_in_sample_.Get(), 0);
    if (input_hr == MF_E_NOTACCEPTING) {
      if (!DrainEncoder(out)) {
        return false;
      }
      input_hr = encoder_->ProcessInput(0, enc_in_sample_.Get(), 0);
    }
    if (FAILED(input_hr)) {
      return false;
    }
    const std::size_t start = out.size();
    if (!DrainEncoder(out)) {
      return false;
    }
    return out.size() > start;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              std::vector<std::uint8_t>& out,
              std::uint64_t timestamp_ms) override {
    if (!decoder_ || (!data && len != 0)) {
      return false;
    }
    const std::uint8_t* input_data = data;
    std::size_t input_len = len;
    std::vector<std::uint8_t> annexb;
    if (data && len > 0 && !HasAnnexBStartCode(data, len)) {
      if (!ConvertAvccToAnnexB(data, len, annexb)) {
        return false;
      }
      input_data = annexb.data();
      input_len = annexb.size();
    }
    if (!dec_in_buf_ || dec_in_capacity_ < input_len) {
      dec_in_sample_.Reset();
      dec_in_buf_.Reset();
      if (FAILED(MFCreateSample(&dec_in_sample_)) ||
          FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(input_len),
                                      &dec_in_buf_))) {
        return false;
      }
      dec_in_capacity_ = static_cast<DWORD>(input_len);
      dec_in_sample_->AddBuffer(dec_in_buf_.Get());
    }
    std::uint8_t* dst = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(dec_in_buf_->Lock(reinterpret_cast<BYTE**>(&dst), &max_len,
                                 &cur_len))) {
      return false;
    }
    if (input_len > 0) {
      std::memcpy(dst, input_data, input_len);
    }
    dec_in_buf_->Unlock();
    dec_in_buf_->SetCurrentLength(static_cast<DWORD>(input_len));
    const LONGLONG ts = static_cast<LONGLONG>(timestamp_ms) * 10000;
    dec_in_sample_->SetSampleTime(ts);
    dec_in_sample_->SetSampleDuration(frame_duration_100ns_);
    HRESULT dec_hr = decoder_->ProcessInput(0, dec_in_sample_.Get(), 0);
    if (dec_hr == MF_E_NOTACCEPTING) {
      if (!DrainDecoder(out)) {
        return false;
      }
      dec_hr = decoder_->ProcessInput(0, dec_in_sample_.Get(), 0);
    }
    if (FAILED(dec_hr)) {
      return false;
    }
    out.clear();
    if (!DrainDecoder(out)) {
      return false;
    }
    return !out.empty();
  }

  bool SetBitrate(std::uint32_t bitrate) override {
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

  void RefreshAnnexBHeader() {
    annexb_header_.clear();
    if (!encoder_) {
      return;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    if (FAILED(encoder_->GetOutputCurrentType(0, &type)) || !type) {
      return;
    }
    UINT32 blob_size = 0;
    if (FAILED(type->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &blob_size)) ||
        blob_size == 0) {
      return;
    }
    std::vector<std::uint8_t> blob(blob_size);
    if (FAILED(type->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, blob.data(),
                             blob_size, nullptr))) {
      return;
    }
    std::vector<std::uint8_t> header;
    if (AvccExtradataToAnnexB(blob.data(), blob.size(), header)) {
      annexb_header_.swap(header);
    }
  }

  bool EnsureEncoderOutputType() {
    if (!encoder_) {
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    if (FAILED(encoder_->GetOutputAvailableType(0, 0, &type)) || !type) {
      return false;
    }
    if (FAILED(encoder_->SetOutputType(0, type.Get(), 0))) {
      return false;
    }
    RefreshAnnexBHeader();
    return true;
  }

  bool EnsureDecoderOutputType() {
    if (!decoder_) {
      return false;
    }
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    if (FAILED(decoder_->GetOutputAvailableType(0, 0, &type)) || !type) {
      return false;
    }
    if (FAILED(decoder_->SetOutputType(0, type.Get(), 0))) {
      return false;
    }
    return true;
  }

  bool DrainEncoder(std::vector<std::uint8_t>& out) {
    if (!encoder_) {
      return false;
    }
    for (;;) {
      MFT_OUTPUT_STREAM_INFO info{};
      if (FAILED(encoder_->GetOutputStreamInfo(0, &info))) {
        return false;
      }
      Microsoft::WRL::ComPtr<IMFSample> sample;
      if ((info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
        if (!enc_out_buf_ || enc_out_capacity_ < info.cbSize) {
          enc_out_sample_.Reset();
          enc_out_buf_.Reset();
          if (FAILED(MFCreateSample(&enc_out_sample_)) ||
              FAILED(MFCreateMemoryBuffer(info.cbSize, &enc_out_buf_))) {
            return false;
          }
          enc_out_capacity_ = info.cbSize;
          enc_out_sample_->AddBuffer(enc_out_buf_.Get());
        }
        enc_out_buf_->SetCurrentLength(0);
        sample = enc_out_sample_;
      }
      MFT_OUTPUT_DATA_BUFFER output{};
      output.pSample = sample.Get();
      DWORD status = 0;
      const HRESULT hr = encoder_->ProcessOutput(0, 1, &output, &status);
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        break;
      }
      if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        if (!EnsureEncoderOutputType()) {
          return false;
        }
        continue;
      }
      if (FAILED(hr)) {
        return false;
      }
      IMFSample* out_sample = output.pSample ? output.pSample : sample.Get();
      if (!out_sample) {
        if (output.pEvents) {
          output.pEvents->Release();
        }
        return false;
      }
      std::vector<std::uint8_t> packet;
      if (!AppendSampleBytes(out_sample, packet)) {
        if (output.pEvents) {
          output.pEvents->Release();
        }
        if (output.pSample && output.pSample != sample.Get()) {
          output.pSample->Release();
        }
        return false;
      }
      if (!HasAnnexBStartCode(packet.data(), packet.size())) {
        std::vector<std::uint8_t> annexb;
        if (!ConvertAvccToAnnexB(packet.data(), packet.size(), annexb)) {
          if (output.pEvents) {
            output.pEvents->Release();
          }
          if (output.pSample && output.pSample != sample.Get()) {
            output.pSample->Release();
          }
          return false;
        }
        packet.swap(annexb);
      }
      const bool keyframe = SampleIsKeyframe(out_sample);
      if (keyframe && !annexb_header_.empty() &&
          !AnnexBHasParameterSets(packet)) {
        out.insert(out.end(), annexb_header_.begin(), annexb_header_.end());
      }
      out.insert(out.end(), packet.begin(), packet.end());
      if (output.pEvents) {
        output.pEvents->Release();
      }
      if (output.pSample && output.pSample != sample.Get()) {
        output.pSample->Release();
      }
    }
    return true;
  }

  bool DrainDecoder(std::vector<std::uint8_t>& out) {
    if (!decoder_) {
      return false;
    }
    for (;;) {
      MFT_OUTPUT_STREAM_INFO info{};
      if (FAILED(decoder_->GetOutputStreamInfo(0, &info))) {
        return false;
      }
      Microsoft::WRL::ComPtr<IMFSample> sample;
      if ((info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
        if (!dec_out_buf_ || dec_out_capacity_ < info.cbSize) {
          dec_out_sample_.Reset();
          dec_out_buf_.Reset();
          if (FAILED(MFCreateSample(&dec_out_sample_)) ||
              FAILED(MFCreateMemoryBuffer(info.cbSize, &dec_out_buf_))) {
            return false;
          }
          dec_out_capacity_ = info.cbSize;
          dec_out_sample_->AddBuffer(dec_out_buf_.Get());
        }
        dec_out_buf_->SetCurrentLength(0);
        sample = dec_out_sample_;
      }
      MFT_OUTPUT_DATA_BUFFER output{};
      output.pSample = sample.Get();
      DWORD status = 0;
      const HRESULT hr = decoder_->ProcessOutput(0, 1, &output, &status);
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        break;
      }
      if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        if (!EnsureDecoderOutputType()) {
          return false;
        }
        continue;
      }
      if (FAILED(hr)) {
        return false;
      }
      IMFSample* out_sample = output.pSample ? output.pSample : sample.Get();
      if (!out_sample) {
        if (output.pEvents) {
          output.pEvents->Release();
        }
        return false;
      }
      std::vector<std::uint8_t> packet;
      if (!AppendSampleBytes(out_sample, packet)) {
        if (output.pEvents) {
          output.pEvents->Release();
        }
        if (output.pSample && output.pSample != sample.Get()) {
          output.pSample->Release();
        }
        return false;
      }
      out.insert(out.end(), packet.begin(), packet.end());
      if (output.pEvents) {
        output.pEvents->Release();
      }
      if (output.pSample && output.pSample != sample.Get()) {
        output.pSample->Release();
      }
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
#if defined(__ICodecAPI_INTERFACE_DEFINED__)
    {
      Microsoft::WRL::ComPtr<ICodecAPI> api;
      if (SUCCEEDED(encoder_.As(&api))) {
        VARIANT v{};
        v.vt = VT_BOOL;
        v.boolVal = VARIANT_TRUE;
        api->SetValue(&CODECAPI_AVLowLatencyMode, &v);
      }
    }
#endif
    RefreshAnnexBHeader();
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
  Microsoft::WRL::ComPtr<IMFSample> enc_in_sample_;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> enc_in_buf_;
  DWORD enc_in_capacity_{0};
  Microsoft::WRL::ComPtr<IMFSample> enc_out_sample_;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> enc_out_buf_;
  DWORD enc_out_capacity_{0};
  Microsoft::WRL::ComPtr<IMFSample> dec_in_sample_;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> dec_in_buf_;
  DWORD dec_in_capacity_{0};
  Microsoft::WRL::ComPtr<IMFSample> dec_out_sample_;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> dec_out_buf_;
  DWORD dec_out_capacity_{0};
  std::vector<std::uint8_t> annexb_header_;
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::uint32_t fps_{0};
  std::uint32_t bitrate_{0};
  LONGLONG frame_duration_100ns_{0};
};

std::unique_ptr<OpusCodec> CreateOpusCodec() {
  return std::make_unique<OpusCodecWin>();
}

std::unique_ptr<H264Codec> CreateH264Codec() {
  return std::make_unique<H264CodecWin>();
}

}  // namespace mi::platform::media
