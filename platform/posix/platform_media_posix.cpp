#include "platform_media.h"

#include <dlfcn.h>

#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#endif

#if defined(MI_E2EE_WITH_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#endif

namespace mi::platform::media {

class OpusCodecPosix final : public OpusCodec {
 public:
  ~OpusCodecPosix() override { Shutdown(); }

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
      dlclose(lib_);
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
    if (lib_) {
      return true;
    }
    const char* names[] = {"libopus.so.0", "libopus.so"};
    for (const auto* name : names) {
      lib_ = dlopen(name, RTLD_LAZY);
      if (lib_) {
        break;
      }
    }
    if (!lib_) {
      error = "opus library not found";
      return false;
    }
    create_encoder_ = reinterpret_cast<OpusEncoderCreate>(
        dlsym(lib_, "opus_encoder_create"));
    create_decoder_ = reinterpret_cast<OpusDecoderCreate>(
        dlsym(lib_, "opus_decoder_create"));
    destroy_encoder_ = reinterpret_cast<OpusEncoderDestroy>(
        dlsym(lib_, "opus_encoder_destroy"));
    destroy_decoder_ = reinterpret_cast<OpusDecoderDestroy>(
        dlsym(lib_, "opus_decoder_destroy"));
    encode_ = reinterpret_cast<OpusEncode>(dlsym(lib_, "opus_encode"));
    decode_ = reinterpret_cast<OpusDecode>(dlsym(lib_, "opus_decode"));
    encoder_ctl_ =
        reinterpret_cast<OpusEncoderCtl>(dlsym(lib_, "opus_encoder_ctl"));
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

  void* lib_{nullptr};
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

#if defined(__APPLE__)
namespace {

constexpr std::uint8_t kAnnexBPrefix[4] = {0, 0, 0, 1};

struct NaluSpan {
  const std::uint8_t* data{nullptr};
  std::size_t len{0};
  std::uint8_t type{0};
};

struct FrameToken {
  std::mutex mu;
  std::condition_variable cv;
  bool done{false};
  bool ok{false};
  std::vector<std::uint8_t> data;
};

bool IsKeyframe(CMSampleBufferRef sample) {
  if (!sample) {
    return false;
  }
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
  if (!attachments || CFArrayGetCount(attachments) == 0) {
    return true;
  }
  CFDictionaryRef dict =
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
  if (!dict) {
    return true;
  }
  const void* value =
      CFDictionaryGetValue(dict, kCMSampleAttachmentKey_NotSync);
  if (!value) {
    return true;
  }
  if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
    return CFBooleanGetValue(static_cast<CFBooleanRef>(value)) == false;
  }
  return false;
}

bool CopyBlockBuffer(CMBlockBufferRef block,
                     std::vector<std::uint8_t>& out) {
  out.clear();
  if (!block) {
    return false;
  }
  size_t total_len = CMBlockBufferGetDataLength(block);
  if (total_len == 0) {
    return false;
  }
  size_t length_at_offset = 0;
  char* data_ptr = nullptr;
  OSStatus rc =
      CMBlockBufferGetDataPointer(block, 0, &length_at_offset, &total_len,
                                  &data_ptr);
  if (rc != kCMBlockBufferNoErr) {
    return false;
  }
  if (length_at_offset == total_len && data_ptr) {
    out.assign(reinterpret_cast<std::uint8_t*>(data_ptr),
               reinterpret_cast<std::uint8_t*>(data_ptr) + total_len);
    return true;
  }
  out.resize(total_len);
  rc = CMBlockBufferCopyDataBytes(block, 0, total_len, out.data());
  if (rc != kCMBlockBufferNoErr) {
    out.clear();
    return false;
  }
  return true;
}

bool AppendAvccToAnnexB(const std::uint8_t* data,
                        std::size_t len,
                        std::vector<std::uint8_t>& out) {
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
      return false;
    }
    out.insert(out.end(), kAnnexBPrefix,
               kAnnexBPrefix + sizeof(kAnnexBPrefix));
    out.insert(out.end(), data + off, data + off + nalu_len);
    off += nalu_len;
  }
  return off == len;
}

bool ExtractParameterSets(CMFormatDescriptionRef format,
                          std::vector<std::uint8_t>& sps,
                          std::vector<std::uint8_t>& pps) {
  sps.clear();
  pps.clear();
  if (!format) {
    return false;
  }
  const std::uint8_t* sps_ptr = nullptr;
  size_t sps_len = 0;
  const std::uint8_t* pps_ptr = nullptr;
  size_t pps_len = 0;
  if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          format, 0, &sps_ptr, &sps_len, nullptr, nullptr) != noErr) {
    return false;
  }
  if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          format, 1, &pps_ptr, &pps_len, nullptr, nullptr) != noErr) {
    return false;
  }
  if (!sps_ptr || !pps_ptr || sps_len == 0 || pps_len == 0) {
    return false;
  }
  sps.assign(sps_ptr, sps_ptr + sps_len);
  pps.assign(pps_ptr, pps_ptr + pps_len);
  return true;
}

bool FindStartCode(const std::uint8_t* data,
                   std::size_t len,
                   std::size_t offset,
                   std::size_t& out_pos,
                   std::size_t& out_size) {
  for (std::size_t i = offset; i + 3 < len; ++i) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      out_pos = i;
      out_size = 3;
      return true;
    }
    if (i + 4 < len && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 &&
        data[i + 3] == 1) {
      out_pos = i;
      out_size = 4;
      return true;
    }
  }
  return false;
}

void ParseAnnexBNalus(const std::uint8_t* data,
                      std::size_t len,
                      std::vector<NaluSpan>& out) {
  out.clear();
  if (!data || len < 4) {
    return;
  }
  std::size_t pos = 0;
  std::size_t sc_pos = 0;
  std::size_t sc_size = 0;
  while (FindStartCode(data, len, pos, sc_pos, sc_size)) {
    const std::size_t nal_start = sc_pos + sc_size;
    std::size_t next_pos = len;
    std::size_t next_size = 0;
    if (FindStartCode(data, len, nal_start, next_pos, next_size)) {
      // next_pos set
    }
    const std::size_t nal_len =
        next_pos > nal_start ? (next_pos - nal_start) : 0;
    if (nal_len > 0) {
      const std::uint8_t type = data[nal_start] & 0x1F;
      out.push_back({data + nal_start, nal_len, type});
    }
    pos = next_pos;
  }
}

void SignalToken(FrameToken* token, bool ok,
                 std::vector<std::uint8_t>&& data) {
  if (!token) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(token->mu);
    token->ok = ok;
    token->data = std::move(data);
    token->done = true;
  }
  token->cv.notify_one();
}

void WaitToken(FrameToken* token) {
  if (!token) {
    return;
  }
  std::unique_lock<std::mutex> lock(token->mu);
  token->cv.wait(lock, [&] { return token->done; });
}

}  // namespace

class H264CodecApple final : public H264Codec {
 public:
  ~H264CodecApple() override { Shutdown(); }

  bool Init(std::uint32_t width,
            std::uint32_t height,
            std::uint32_t fps,
            std::uint32_t bitrate,
            std::string& error) override {
    error.clear();
    Shutdown();
    if (width == 0 || height == 0 || fps == 0 || bitrate == 0) {
      error = "video config invalid";
      return false;
    }
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    pts_ = 0;
    if (!CreateEncoder(error)) {
      Shutdown();
      return false;
    }
    return true;
  }

  bool Encode(const std::uint8_t* nv12,
              std::size_t stride,
              bool keyframe,
              std::vector<std::uint8_t>& out,
              std::uint64_t /*timestamp_ms*/) override {
    if (!enc_session_ || !nv12) {
      return false;
    }
    if (stride < width_) {
      return false;
    }
    CVPixelBufferRef pixel = nullptr;
    if (!CreatePixelBuffer(nv12, stride, pixel)) {
      return false;
    }
    CFDictionaryRef options = nullptr;
    if (keyframe) {
      const void* keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
      const void* values[] = {kCFBooleanTrue};
      options = CFDictionaryCreate(nullptr, keys, values, 1,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
    }
    auto token = std::make_unique<FrameToken>();
    const CMTime pts = CMTimeMake(static_cast<int64_t>(pts_++),
                                  static_cast<int32_t>(fps_));
    const OSStatus rc = VTCompressionSessionEncodeFrame(
        enc_session_, pixel, pts, kCMTimeInvalid, options, token.get(), nullptr);
    if (options) {
      CFRelease(options);
    }
    CVPixelBufferRelease(pixel);
    if (rc != noErr) {
      return false;
    }
    WaitToken(token.get());
    if (!token->ok) {
      return false;
    }
    out.insert(out.end(), token->data.begin(), token->data.end());
    return true;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              std::vector<std::uint8_t>& out,
              std::uint64_t /*timestamp_ms*/) override {
    out.clear();
    if (!data || len == 0) {
      return false;
    }
    std::vector<NaluSpan> nalus;
    ParseAnnexBNalus(data, len, nalus);
    if (nalus.empty()) {
      return false;
    }
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
    for (const auto& nalu : nalus) {
      if (nalu.type == 7) {
        sps.assign(nalu.data, nalu.data + nalu.len);
      } else if (nalu.type == 8) {
        pps.assign(nalu.data, nalu.data + nalu.len);
      }
    }
    if (!sps.empty() && !pps.empty()) {
      if (sps != sps_ || pps != pps_) {
        if (!UpdateDecoder(sps, pps)) {
          return false;
        }
      }
    }
    if (!dec_session_) {
      return false;
    }

    std::vector<std::uint8_t> avcc;
    avcc.reserve(len + 4);
    for (const auto& nalu : nalus) {
      if (nalu.type == 7 || nalu.type == 8) {
        continue;
      }
      const std::uint32_t nalu_len = static_cast<std::uint32_t>(nalu.len);
      avcc.push_back(static_cast<std::uint8_t>((nalu_len >> 24) & 0xFF));
      avcc.push_back(static_cast<std::uint8_t>((nalu_len >> 16) & 0xFF));
      avcc.push_back(static_cast<std::uint8_t>((nalu_len >> 8) & 0xFF));
      avcc.push_back(static_cast<std::uint8_t>(nalu_len & 0xFF));
      avcc.insert(avcc.end(), nalu.data, nalu.data + nalu.len);
    }
    if (avcc.empty()) {
      return false;
    }

    CMBlockBufferRef block = nullptr;
    const OSStatus block_rc = CMBlockBufferCreateWithMemoryBlock(
        kCFAllocatorDefault, nullptr, avcc.size(), kCFAllocatorDefault, nullptr,
        0, avcc.size(), 0, &block);
    if (block_rc != kCMBlockBufferNoErr || !block) {
      return false;
    }
    if (CMBlockBufferReplaceDataBytes(avcc.data(), block, 0, avcc.size()) !=
        kCMBlockBufferNoErr) {
      CFRelease(block);
      return false;
    }

    CMSampleBufferRef sample = nullptr;
    const size_t sample_sizes[] = {avcc.size()};
    const OSStatus sample_rc = CMSampleBufferCreateReady(
        kCFAllocatorDefault, block, dec_format_, 1, 0, nullptr, 1, sample_sizes,
        &sample);
    CFRelease(block);
    if (sample_rc != noErr || !sample) {
      return false;
    }

    auto token = std::make_unique<FrameToken>();
    VTDecodeFrameFlags flags = 0;
    VTDecodeInfoFlags info = 0;
    const OSStatus decode_rc = VTDecompressionSessionDecodeFrame(
        dec_session_, sample, flags, token.get(), &info);
    CFRelease(sample);
    if (decode_rc != noErr) {
      return false;
    }
    WaitToken(token.get());
    if (!token->ok) {
      return false;
    }
    out = std::move(token->data);
    return !out.empty();
  }

  bool SetBitrate(std::uint32_t bitrate) override {
    bitrate_ = bitrate;
    if (!enc_session_) {
      return false;
    }
    const int32_t bps = static_cast<int32_t>(bitrate);
    CFNumberRef bps_num =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &bps);
    if (bps_num) {
      VTSessionSetProperty(enc_session_,
                           kVTCompressionPropertyKey_AverageBitRate, bps_num);
      CFRelease(bps_num);
    }
    const int32_t bytes = static_cast<int32_t>(bitrate / 8);
    const int32_t secs = 1;
    CFNumberRef bytes_num =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &bytes);
    CFNumberRef secs_num =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &secs);
    if (bytes_num && secs_num) {
      const void* vals[] = {bytes_num, secs_num};
      CFArrayRef arr = CFArrayCreate(nullptr, vals, 2,
                                     &kCFTypeArrayCallBacks);
      if (arr) {
        VTSessionSetProperty(enc_session_,
                             kVTCompressionPropertyKey_DataRateLimits, arr);
        CFRelease(arr);
      }
    }
    if (bytes_num) {
      CFRelease(bytes_num);
    }
    if (secs_num) {
      CFRelease(secs_num);
    }
    return true;
  }

  void Shutdown() override {
    if (enc_session_) {
      VTCompressionSessionInvalidate(enc_session_);
      CFRelease(enc_session_);
      enc_session_ = nullptr;
    }
    if (dec_session_) {
      VTDecompressionSessionInvalidate(dec_session_);
      CFRelease(dec_session_);
      dec_session_ = nullptr;
    }
    if (dec_format_) {
      CFRelease(dec_format_);
      dec_format_ = nullptr;
    }
    sps_.clear();
    pps_.clear();
    width_ = 0;
    height_ = 0;
    fps_ = 0;
    bitrate_ = 0;
    pts_ = 0;
  }

 private:
  static void CompressionCallback(void* outputCallbackRefCon,
                                  void* sourceFrameRefCon,
                                  OSStatus status,
                                  VTEncodeInfoFlags /*infoFlags*/,
                                  CMSampleBufferRef sampleBuffer) {
    auto* self = static_cast<H264CodecApple*>(outputCallbackRefCon);
    auto* token = static_cast<FrameToken*>(sourceFrameRefCon);
    if (status != noErr || !sampleBuffer) {
      SignalToken(token, false, {});
      return;
    }
    std::vector<std::uint8_t> block_data;
    if (!CopyBlockBuffer(CMSampleBufferGetDataBuffer(sampleBuffer),
                         block_data)) {
      SignalToken(token, false, {});
      return;
    }
    std::vector<std::uint8_t> out;
    out.reserve(block_data.size() + 64);
    if (IsKeyframe(sampleBuffer)) {
      std::vector<std::uint8_t> sps;
      std::vector<std::uint8_t> pps;
      if (ExtractParameterSets(CMSampleBufferGetFormatDescription(sampleBuffer),
                               sps, pps)) {
        if (self) {
          self->sps_ = sps;
          self->pps_ = pps;
        }
        out.insert(out.end(), kAnnexBPrefix,
                   kAnnexBPrefix + sizeof(kAnnexBPrefix));
        out.insert(out.end(), sps.begin(), sps.end());
        out.insert(out.end(), kAnnexBPrefix,
                   kAnnexBPrefix + sizeof(kAnnexBPrefix));
        out.insert(out.end(), pps.begin(), pps.end());
      }
    }
    if (!AppendAvccToAnnexB(block_data.data(), block_data.size(), out)) {
      SignalToken(token, false, {});
      return;
    }
    SignalToken(token, true, std::move(out));
  }

  static void DecompressionCallback(void* /*decompressionOutputRefCon*/,
                                    void* sourceFrameRefCon,
                                    OSStatus status,
                                    VTDecodeInfoFlags /*infoFlags*/,
                                    CVImageBufferRef imageBuffer,
                                    CMTime /*presentationTimeStamp*/,
                                    CMTime /*presentationDuration*/) {
    auto* token = static_cast<FrameToken*>(sourceFrameRefCon);
    if (status != noErr || !imageBuffer) {
      SignalToken(token, false, {});
      return;
    }
    CVPixelBufferRef pb = static_cast<CVPixelBufferRef>(imageBuffer);
    const OSType fmt = CVPixelBufferGetPixelFormatType(pb);
    if (fmt != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
        fmt != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
      SignalToken(token, false, {});
      return;
    }
    CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    const std::size_t width = CVPixelBufferGetWidthOfPlane(pb, 0);
    const std::size_t height = CVPixelBufferGetHeightOfPlane(pb, 0);
    const std::size_t stride_y = CVPixelBufferGetBytesPerRowOfPlane(pb, 0);
    const std::size_t stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pb, 1);
    const std::uint8_t* src_y =
        static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pb, 0));
    const std::uint8_t* src_uv =
        static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pb, 1));
    std::vector<std::uint8_t> out;
    if (width > 0 && height > 0 && src_y && src_uv) {
      const std::size_t y_bytes = width * height;
      out.resize(y_bytes + y_bytes / 2);
      for (std::size_t row = 0; row < height; ++row) {
        std::memcpy(out.data() + row * width, src_y + row * stride_y, width);
      }
      const std::size_t uv_height = height / 2;
      std::uint8_t* dst_uv = out.data() + y_bytes;
      for (std::size_t row = 0; row < uv_height; ++row) {
        std::memcpy(dst_uv + row * width, src_uv + row * stride_uv, width);
      }
    }
    CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    if (out.empty()) {
      SignalToken(token, false, {});
      return;
    }
    SignalToken(token, true, std::move(out));
  }

  bool CreateEncoder(std::string& error) {
    const int w = static_cast<int>(width_);
    const int h = static_cast<int>(height_);
    const int32_t pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    const void* keys[] = {kCVPixelBufferPixelFormatTypeKey};
    const void* values[] = {CFNumberCreate(nullptr, kCFNumberSInt32Type,
                                           &pixel_format)};
    CFDictionaryRef attrs = CFDictionaryCreate(
        nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (values[0]) {
      CFRelease(values[0]);
    }
    const OSStatus rc = VTCompressionSessionCreate(
        kCFAllocatorDefault, w, h, kCMVideoCodecType_H264, nullptr, attrs,
        nullptr, &CompressionCallback, this, &enc_session_);
    if (attrs) {
      CFRelease(attrs);
    }
    if (rc != noErr || !enc_session_) {
      error = "h264 encoder init failed";
      return false;
    }
    VTSessionSetProperty(enc_session_, kVTCompressionPropertyKey_RealTime,
                         kCFBooleanTrue);
    VTSessionSetProperty(enc_session_,
                         kVTCompressionPropertyKey_AllowFrameReordering,
                         kCFBooleanFalse);
    const int32_t fps = static_cast<int32_t>(fps_);
    CFNumberRef fps_num = CFNumberCreate(nullptr, kCFNumberSInt32Type, &fps);
    if (fps_num) {
      VTSessionSetProperty(enc_session_,
                           kVTCompressionPropertyKey_ExpectedFrameRate,
                           fps_num);
      VTSessionSetProperty(enc_session_,
                           kVTCompressionPropertyKey_MaxKeyFrameInterval,
                           fps_num);
      CFRelease(fps_num);
    }
    const int32_t bps = static_cast<int32_t>(bitrate_);
    CFNumberRef bps_num = CFNumberCreate(nullptr, kCFNumberSInt32Type, &bps);
    if (bps_num) {
      VTSessionSetProperty(enc_session_,
                           kVTCompressionPropertyKey_AverageBitRate, bps_num);
      CFRelease(bps_num);
    }
    CFStringRef profile = kVTProfileLevel_H264_Baseline_AutoLevel;
    VTSessionSetProperty(enc_session_, kVTCompressionPropertyKey_ProfileLevel,
                         profile);
    VTCompressionSessionPrepareToEncodeFrames(enc_session_);
    return true;
  }

  bool CreatePixelBuffer(const std::uint8_t* nv12,
                          std::size_t stride,
                          CVPixelBufferRef& out) {
    out = nullptr;
    if (!nv12 || width_ == 0 || height_ == 0) {
      return false;
    }
    const OSType fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    const OSStatus rc = CVPixelBufferCreate(
        kCFAllocatorDefault, width_, height_, fmt, nullptr, &out);
    if (rc != kCVReturnSuccess || !out) {
      return false;
    }
    CVPixelBufferLockBaseAddress(out, 0);
    std::uint8_t* dst_y =
        static_cast<std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(out, 0));
    std::uint8_t* dst_uv =
        static_cast<std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(out, 1));
    const std::size_t dst_stride_y =
        CVPixelBufferGetBytesPerRowOfPlane(out, 0);
    const std::size_t dst_stride_uv =
        CVPixelBufferGetBytesPerRowOfPlane(out, 1);
    for (std::uint32_t row = 0; row < height_; ++row) {
      std::memcpy(dst_y + row * dst_stride_y,
                  nv12 + row * stride, width_);
    }
    const std::uint8_t* src_uv = nv12 + stride * height_;
    const std::uint32_t uv_height = height_ / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      std::memcpy(dst_uv + row * dst_stride_uv,
                  src_uv + row * stride, width_);
    }
    CVPixelBufferUnlockBaseAddress(out, 0);
    return true;
  }

  bool UpdateDecoder(const std::vector<std::uint8_t>& sps,
                     const std::vector<std::uint8_t>& pps) {
    if (sps.empty() || pps.empty()) {
      return false;
    }
    if (dec_session_) {
      VTDecompressionSessionInvalidate(dec_session_);
      CFRelease(dec_session_);
      dec_session_ = nullptr;
    }
    if (dec_format_) {
      CFRelease(dec_format_);
      dec_format_ = nullptr;
    }
    const std::uint8_t* params[] = {sps.data(), pps.data()};
    const size_t sizes[] = {sps.size(), pps.size()};
    const OSStatus fmt_rc = CMVideoFormatDescriptionCreateFromH264ParameterSets(
        kCFAllocatorDefault, 2, params, sizes, 4, &dec_format_);
    if (fmt_rc != noErr || !dec_format_) {
      return false;
    }
    const int32_t pixel_format =
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    const void* keys[] = {kCVPixelBufferPixelFormatTypeKey};
    const void* values[] = {CFNumberCreate(nullptr, kCFNumberSInt32Type,
                                           &pixel_format)};
    CFDictionaryRef attrs = CFDictionaryCreate(
        nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (values[0]) {
      CFRelease(values[0]);
    }
    VTDecompressionOutputCallbackRecord cb{};
    cb.decompressionOutputCallback = DecompressionCallback;
    cb.decompressionOutputRefCon = this;
    const OSStatus dec_rc = VTDecompressionSessionCreate(
        kCFAllocatorDefault, dec_format_, nullptr, attrs, &cb, &dec_session_);
    if (attrs) {
      CFRelease(attrs);
    }
    if (dec_rc != noErr || !dec_session_) {
      return false;
    }
    sps_ = sps;
    pps_ = pps;
    return true;
  }

  VTCompressionSessionRef enc_session_{nullptr};
  VTDecompressionSessionRef dec_session_{nullptr};
  CMVideoFormatDescriptionRef dec_format_{nullptr};
  std::vector<std::uint8_t> sps_;
  std::vector<std::uint8_t> pps_;
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::uint32_t fps_{0};
  std::uint32_t bitrate_{0};
  std::int64_t pts_{0};
};
#endif

#if defined(MI_E2EE_WITH_FFMPEG)
namespace {

std::string AvErrorToString(int err) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {};
  if (av_strerror(err, buf, sizeof(buf)) == 0) {
    return std::string(buf);
  }
  return "ffmpeg error";
}

}  // namespace

class H264CodecFfmpeg final : public H264Codec {
 public:
  ~H264CodecFfmpeg() override { Shutdown(); }

  bool Init(std::uint32_t width,
            std::uint32_t height,
            std::uint32_t fps,
            std::uint32_t bitrate,
            std::string& error) override {
    error.clear();
    Shutdown();
    if (width == 0 || height == 0 || fps == 0 || bitrate == 0) {
      error = "video config invalid";
      return false;
    }
    av_log_set_level(AV_LOG_QUIET);
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    pts_ = 0;
    if (!InitEncoder(error)) {
      Shutdown();
      return false;
    }
    if (!InitDecoder(error)) {
      Shutdown();
      return false;
    }
    return true;
  }

  bool Encode(const std::uint8_t* nv12,
              std::size_t stride,
              bool keyframe,
              std::vector<std::uint8_t>& out,
              std::uint64_t /*timestamp_ms*/) override {
    if (!enc_ctx_ || !enc_frame_ || !enc_pkt_ || !nv12) {
      return false;
    }
    if (stride < width_) {
      return false;
    }
    if (av_frame_make_writable(enc_frame_) < 0) {
      return false;
    }
    if (!CopyNv12ToFrame(nv12, stride)) {
      return false;
    }
    enc_frame_->pts = pts_++;
    enc_frame_->pict_type = keyframe ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
    enc_frame_->key_frame = keyframe ? 1 : 0;

    const std::size_t base = out.size();
    const int send_rc = avcodec_send_frame(enc_ctx_, enc_frame_);
    if (send_rc < 0) {
      return false;
    }
    bool wrote = false;
    for (;;) {
      const int rc = avcodec_receive_packet(enc_ctx_, enc_pkt_);
      if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
        break;
      }
      if (rc < 0) {
        out.resize(base);
        return false;
      }
      if (enc_pkt_->data && enc_pkt_->size > 0) {
        out.insert(out.end(), enc_pkt_->data, enc_pkt_->data + enc_pkt_->size);
        wrote = true;
      }
      av_packet_unref(enc_pkt_);
    }
    if (!wrote) {
      out.resize(base);
      return false;
    }
    return true;
  }

  bool Decode(const std::uint8_t* data,
              std::size_t len,
              std::vector<std::uint8_t>& out,
              std::uint64_t /*timestamp_ms*/) override {
    out.clear();
    if (!dec_ctx_ || !dec_frame_ || !dec_pkt_ || !data || len == 0) {
      return false;
    }
    if (len > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
      return false;
    }
    av_packet_unref(dec_pkt_);
    if (av_new_packet(dec_pkt_, static_cast<int>(len)) < 0) {
      return false;
    }
    std::memcpy(dec_pkt_->data, data, len);
    const int send_rc = avcodec_send_packet(dec_ctx_, dec_pkt_);
    av_packet_unref(dec_pkt_);
    if (send_rc < 0) {
      return false;
    }
    const int rc = avcodec_receive_frame(dec_ctx_, dec_frame_);
    if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
      av_frame_unref(dec_frame_);
      return false;
    }
    if (rc < 0) {
      av_frame_unref(dec_frame_);
      return false;
    }
    const bool ok = ConvertFrameToNv12(dec_frame_, out);
    av_frame_unref(dec_frame_);
    return ok;
  }

  bool SetBitrate(std::uint32_t bitrate) override {
    bitrate_ = bitrate;
    if (!enc_ctx_) {
      return false;
    }
    enc_ctx_->bit_rate = bitrate;
    if (enc_ctx_->priv_data) {
      av_opt_set_int(enc_ctx_->priv_data, "b", bitrate, 0);
      av_opt_set_int(enc_ctx_->priv_data, "maxrate", bitrate, 0);
      av_opt_set_int(enc_ctx_->priv_data, "bufsize", bitrate, 0);
    }
    return true;
  }

  void Shutdown() override {
    if (enc_ctx_) {
      avcodec_free_context(&enc_ctx_);
    }
    if (dec_ctx_) {
      avcodec_free_context(&dec_ctx_);
    }
    if (enc_frame_) {
      av_frame_free(&enc_frame_);
    }
    if (dec_frame_) {
      av_frame_free(&dec_frame_);
    }
    if (enc_pkt_) {
      av_packet_free(&enc_pkt_);
    }
    if (dec_pkt_) {
      av_packet_free(&dec_pkt_);
    }
    if (sws_) {
      sws_freeContext(sws_);
      sws_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
    fps_ = 0;
    bitrate_ = 0;
    pts_ = 0;
  }

 private:
  bool InitEncoder(std::string& error) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
      error = "h264 encoder unavailable";
      return false;
    }
    enc_ctx_ = avcodec_alloc_context3(codec);
    if (!enc_ctx_) {
      error = "h264 encoder alloc failed";
      return false;
    }
    enc_ctx_->width = static_cast<int>(width_);
    enc_ctx_->height = static_cast<int>(height_);
    enc_ctx_->pix_fmt = AV_PIX_FMT_NV12;
    enc_ctx_->time_base = AVRational{1, static_cast<int>(fps_)};
    enc_ctx_->framerate = AVRational{static_cast<int>(fps_), 1};
    enc_ctx_->bit_rate = static_cast<int64_t>(bitrate_);
    enc_ctx_->gop_size = static_cast<int>(fps_);
    enc_ctx_->max_b_frames = 0;
    enc_ctx_->thread_count = 1;
    enc_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;

    AVDictionary* opts = nullptr;
    if (codec->name && std::strstr(codec->name, "libx264")) {
      av_dict_set(&opts, "preset", "ultrafast", 0);
      av_dict_set(&opts, "tune", "zerolatency", 0);
      av_dict_set(&opts, "profile", "baseline", 0);
      av_dict_set(&opts, "repeat_headers", "1", 0);
      av_dict_set(&opts, "annexb", "1", 0);
    }
    const int rc = avcodec_open2(enc_ctx_, codec, &opts);
    av_dict_free(&opts);
    if (rc < 0) {
      error = "h264 encoder open failed: " + AvErrorToString(rc);
      return false;
    }

    enc_frame_ = av_frame_alloc();
    if (!enc_frame_) {
      error = "h264 frame alloc failed";
      return false;
    }
    enc_frame_->format = enc_ctx_->pix_fmt;
    enc_frame_->width = enc_ctx_->width;
    enc_frame_->height = enc_ctx_->height;
    if (av_frame_get_buffer(enc_frame_, 32) < 0) {
      error = "h264 frame buffer failed";
      return false;
    }

    enc_pkt_ = av_packet_alloc();
    if (!enc_pkt_) {
      error = "h264 packet alloc failed";
      return false;
    }
    return true;
  }

  bool InitDecoder(std::string& error) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
      error = "h264 decoder unavailable";
      return false;
    }
    dec_ctx_ = avcodec_alloc_context3(codec);
    if (!dec_ctx_) {
      error = "h264 decoder alloc failed";
      return false;
    }
    dec_ctx_->thread_count = 1;
    dec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
    const int rc = avcodec_open2(dec_ctx_, codec, nullptr);
    if (rc < 0) {
      error = "h264 decoder open failed: " + AvErrorToString(rc);
      return false;
    }
    dec_frame_ = av_frame_alloc();
    if (!dec_frame_) {
      error = "h264 frame alloc failed";
      return false;
    }
    dec_pkt_ = av_packet_alloc();
    if (!dec_pkt_) {
      error = "h264 packet alloc failed";
      return false;
    }
    return true;
  }

  bool CopyNv12ToFrame(const std::uint8_t* data, std::size_t stride) {
    if (!enc_frame_ || !data) {
      return false;
    }
    const int dst_stride_y = enc_frame_->linesize[0];
    const int dst_stride_uv = enc_frame_->linesize[1];
    const std::uint8_t* src_y = data;
    const std::uint8_t* src_uv = data + stride * height_;
    std::uint8_t* dst_y = enc_frame_->data[0];
    std::uint8_t* dst_uv = enc_frame_->data[1];
    for (std::uint32_t row = 0; row < height_; ++row) {
      std::memcpy(dst_y + row * dst_stride_y, src_y + row * stride, width_);
    }
    const std::uint32_t uv_height = height_ / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      std::memcpy(dst_uv + row * dst_stride_uv, src_uv + row * stride, width_);
    }
    return true;
  }

  bool ConvertFrameToNv12(const AVFrame* frame,
                          std::vector<std::uint8_t>& out) {
    if (!frame) {
      return false;
    }
    const int width = frame->width;
    const int height = frame->height;
    const int buf_size =
        av_image_get_buffer_size(AV_PIX_FMT_NV12, width, height, 1);
    if (buf_size <= 0) {
      return false;
    }
    out.resize(static_cast<std::size_t>(buf_size));
    std::uint8_t* dst_data[4] = {};
    int dst_linesize[4] = {};
    if (av_image_fill_arrays(dst_data, dst_linesize, out.data(),
                             AV_PIX_FMT_NV12, width, height, 1) < 0) {
      out.clear();
      return false;
    }

    if (frame->format == AV_PIX_FMT_NV12) {
      const int src_stride_y = frame->linesize[0];
      const int src_stride_uv = frame->linesize[1];
      for (int row = 0; row < height; ++row) {
        std::memcpy(dst_data[0] + row * dst_linesize[0],
                    frame->data[0] + row * src_stride_y, width);
      }
      const int uv_height = height / 2;
      for (int row = 0; row < uv_height; ++row) {
        std::memcpy(dst_data[1] + row * dst_linesize[1],
                    frame->data[1] + row * src_stride_uv, width);
      }
      return true;
    }

    sws_ = sws_getCachedContext(
        sws_, width, height, static_cast<AVPixelFormat>(frame->format),
        width, height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_) {
      out.clear();
      return false;
    }
    const int scaled =
        sws_scale(sws_, frame->data, frame->linesize, 0, height, dst_data,
                  dst_linesize);
    if (scaled <= 0) {
      out.clear();
      return false;
    }
    return true;
  }

  AVCodecContext* enc_ctx_{nullptr};
  AVCodecContext* dec_ctx_{nullptr};
  AVFrame* enc_frame_{nullptr};
  AVFrame* dec_frame_{nullptr};
  AVPacket* enc_pkt_{nullptr};
  AVPacket* dec_pkt_{nullptr};
  SwsContext* sws_{nullptr};
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::uint32_t fps_{0};
  std::uint32_t bitrate_{0};
  std::int64_t pts_{0};
};
#endif

class H264CodecStub final : public H264Codec {
 public:
  bool Init(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
            std::string& error) override {
    error = "h264 unavailable";
    return false;
  }
  bool Encode(const std::uint8_t*, std::size_t, bool,
              std::vector<std::uint8_t>&, std::uint64_t) override {
    return false;
  }
  bool Decode(const std::uint8_t*, std::size_t,
              std::vector<std::uint8_t>&, std::uint64_t) override {
    return false;
  }
  bool SetBitrate(std::uint32_t) override { return false; }
};

std::unique_ptr<OpusCodec> CreateOpusCodec() {
  return std::make_unique<OpusCodecPosix>();
}

std::unique_ptr<H264Codec> CreateH264Codec() {
#if defined(__APPLE__)
  return std::make_unique<H264CodecApple>();
#elif defined(MI_E2EE_WITH_FFMPEG)
  return std::make_unique<H264CodecFfmpeg>();
#else
  return std::make_unique<H264CodecStub>();
#endif
}

}  // namespace mi::platform::media
