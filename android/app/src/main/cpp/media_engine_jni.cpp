#include <jni.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "c_api_client.h"
#include "group_call_media_adapter.h"
#include "group_call_session.h"
#include "media_pipeline.h"
#include "media_session.h"
#include "media_transport_capi.h"
#include "platform_time.h"

namespace {

constexpr std::uint32_t kCallIdLen = 16;

struct MediaEngine {
  bool is_group = false;
  mi_client_handle* client = nullptr;
  std::unique_ptr<mi::client::ui::CapiMediaTransport> transport;
  std::unique_ptr<mi::client::media::MediaSession> peer_session;
  std::unique_ptr<mi::client::media::GroupCallSession> group_session;
  std::unique_ptr<mi::client::media::GroupCallMediaAdapter> group_adapter;
  std::unique_ptr<mi::client::media::AudioPipeline> audio;
  std::unique_ptr<mi::client::media::VideoPipeline> video;
  int frame_samples = 0;
  std::string last_error;
};

struct MediaJniCache {
  jclass cls_video_frame = nullptr;
  jmethodID ctor_video_frame = nullptr;
};

static MediaJniCache g_cache;
static bool g_cache_ready = false;

static bool EnsureCache(JNIEnv* env) {
  if (g_cache_ready) {
    return true;
  }
  jclass local = env->FindClass("mi/e2ee/android/sdk/MediaVideoFrame");
  if (!local) {
    return false;
  }
  g_cache.cls_video_frame = reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  g_cache.ctor_video_frame =
      env->GetMethodID(g_cache.cls_video_frame, "<init>", "(IIIZ[B)V");
  if (!g_cache.ctor_video_frame) {
    return false;
  }
  g_cache_ready = true;
  return true;
}

static std::string JStringToString(JNIEnv* env, jstring input) {
  if (!input) {
    return std::string();
  }
  const char* utf8 = env->GetStringUTFChars(input, nullptr);
  std::string out = utf8 ? utf8 : "";
  if (utf8) {
    env->ReleaseStringUTFChars(input, utf8);
  }
  return out;
}

static std::vector<std::uint8_t> JByteArrayToVector(JNIEnv* env, jbyteArray input) {
  std::vector<std::uint8_t> out;
  if (!input) {
    return out;
  }
  jsize len = env->GetArrayLength(input);
  if (len <= 0) {
    return out;
  }
  out.resize(static_cast<std::size_t>(len));
  env->GetByteArrayRegion(input, 0, len,
                          reinterpret_cast<jbyte*>(out.data()));
  return out;
}

static jbyteArray ToJByteArray(JNIEnv* env, const std::uint8_t* data, std::size_t len) {
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(len));
  if (arr && data && len > 0) {
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));
  }
  return arr;
}

static bool LoadMediaConfig(mi_client_handle* handle,
                            mi_media_config_t& out_config,
                            std::string& error) {
  error.clear();
  std::memset(&out_config, 0, sizeof(out_config));
  if (!handle) {
    error = "invalid handle";
    return false;
  }
  if (!mi_client_get_media_config(handle, &out_config)) {
    const char* err = mi_client_last_error(handle);
    if (err && *err != '\0') {
      error = err;
    } else {
      error = "media config unavailable";
    }
    return false;
  }
  return true;
}

static MediaEngine* FromHandle(jlong handle) {
  return reinterpret_cast<MediaEngine*>(handle);
}

static std::array<std::uint8_t, kCallIdLen> ParseCallId(JNIEnv* env,
                                                        jbyteArray call_id) {
  std::array<std::uint8_t, kCallIdLen> out{};
  std::vector<std::uint8_t> data = JByteArrayToVector(env, call_id);
  if (data.size() != kCallIdLen) {
    out.fill(0);
    return out;
  }
  std::memcpy(out.data(), data.data(), out.size());
  return out;
}

static bool InitAudioPipeline(MediaEngine* engine,
                              mi::client::media::MediaSessionInterface& session,
                              int sample_rate,
                              int channels,
                              int frame_ms) {
  if (!engine) {
    return false;
  }
  mi::client::media::AudioPipelineConfig cfg;
  if (sample_rate > 0) {
    cfg.sample_rate = sample_rate;
  }
  if (channels > 0) {
    cfg.channels = channels;
  }
  if (frame_ms > 0) {
    cfg.frame_ms = frame_ms;
  }
  std::string error;
  engine->audio = std::make_unique<mi::client::media::AudioPipeline>(session, cfg);
  if (!engine->audio->Init(error)) {
    engine->last_error = error.empty() ? "audio init failed" : error;
    engine->audio.reset();
    return false;
  }
  engine->frame_samples = engine->audio->frame_samples();
  return true;
}

static bool InitVideoPipeline(MediaEngine* engine,
                              mi::client::media::MediaSessionInterface& session,
                              int width,
                              int height,
                              int fps) {
  if (!engine) {
    return false;
  }
  mi::client::media::VideoPipelineConfig cfg;
  if (width > 0) {
    cfg.width = static_cast<std::uint32_t>(width);
  }
  if (height > 0) {
    cfg.height = static_cast<std::uint32_t>(height);
  }
  if (fps > 0) {
    cfg.fps = static_cast<std::uint32_t>(fps);
  }
  std::string error;
  engine->video = std::make_unique<mi::client::media::VideoPipeline>(session, cfg);
  if (!engine->video->Init(error)) {
    engine->last_error = error.empty() ? "video init failed" : error;
    engine->video.reset();
    return false;
  }
  return true;
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_createPeerEngine(
    JNIEnv* env,
    jobject,
    jlong client_handle,
    jstring peer_username,
    jbyteArray call_id,
    jboolean initiator,
    jboolean enable_video,
    jint sample_rate,
    jint channels,
    jint frame_ms,
    jint video_width,
    jint video_height,
    jint video_fps) {
  mi_client_handle* client = reinterpret_cast<mi_client_handle*>(client_handle);
  if (!client) {
    return 0;
  }
  const std::array<std::uint8_t, kCallIdLen> call = ParseCallId(env, call_id);
  if (call == std::array<std::uint8_t, kCallIdLen>{}) {
    return 0;
  }
  std::string peer = JStringToString(env, peer_username);
  if (peer.empty()) {
    return 0;
  }

  auto* engine = new MediaEngine();
  engine->client = client;
  engine->is_group = false;
  engine->transport = std::make_unique<mi::client::ui::CapiMediaTransport>(client);

  mi_media_config_t media_cfg{};
  std::string error;
  if (!LoadMediaConfig(client, media_cfg, error)) {
    engine->last_error = error;
    delete engine;
    return 0;
  }

  mi::client::media::MediaSessionConfig cfg;
  cfg.peer_username = peer;
  cfg.call_id = call;
  cfg.initiator = initiator != 0;
  cfg.enable_audio = true;
  cfg.enable_video = enable_video != 0;
  cfg.audio_delay_ms = media_cfg.audio_delay_ms > 0 ? media_cfg.audio_delay_ms : 60;
  cfg.video_delay_ms = media_cfg.video_delay_ms > 0 ? media_cfg.video_delay_ms : 120;
  cfg.audio_max_frames = media_cfg.audio_max_frames > 0 ? media_cfg.audio_max_frames : 256;
  cfg.video_max_frames = media_cfg.video_max_frames > 0 ? media_cfg.video_max_frames : 256;

  auto session =
      std::make_unique<mi::client::media::MediaSession>(*engine->transport, cfg);
  if (!session->Init(error)) {
    engine->last_error = error.empty() ? "media session init failed" : error;
    delete engine;
    return 0;
  }
  engine->peer_session = std::move(session);

  if (!InitAudioPipeline(engine, *engine->peer_session, sample_rate, channels,
                         frame_ms)) {
    delete engine;
    return 0;
  }

  if (cfg.enable_video) {
    if (!InitVideoPipeline(engine, *engine->peer_session, video_width,
                           video_height, video_fps)) {
      delete engine;
      return 0;
    }
  }

  return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT jlong JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_createGroupEngine(
    JNIEnv* env,
    jobject,
    jlong client_handle,
    jstring group_id,
    jbyteArray call_id,
    jint key_id,
    jboolean enable_video,
    jint sample_rate,
    jint channels,
    jint frame_ms,
    jint video_width,
    jint video_height,
    jint video_fps) {
  mi_client_handle* client = reinterpret_cast<mi_client_handle*>(client_handle);
  if (!client) {
    return 0;
  }
  const std::array<std::uint8_t, kCallIdLen> call = ParseCallId(env, call_id);
  if (call == std::array<std::uint8_t, kCallIdLen>{}) {
    return 0;
  }
  std::string group = JStringToString(env, group_id);
  if (group.empty()) {
    return 0;
  }

  auto* engine = new MediaEngine();
  engine->client = client;
  engine->is_group = true;
  engine->transport = std::make_unique<mi::client::ui::CapiMediaTransport>(client);

  mi_media_config_t media_cfg{};
  std::string error;
  if (!LoadMediaConfig(client, media_cfg, error)) {
    engine->last_error = error;
    delete engine;
    return 0;
  }

  mi::client::media::GroupCallSessionConfig cfg;
  cfg.group_id = group;
  cfg.call_id = call;
  cfg.key_id = key_id > 0 ? static_cast<std::uint32_t>(key_id) : 1;
  cfg.enable_audio = true;
  cfg.enable_video = enable_video != 0;
  cfg.audio_delay_ms = media_cfg.audio_delay_ms > 0 ? media_cfg.audio_delay_ms : 60;
  cfg.video_delay_ms = media_cfg.video_delay_ms > 0 ? media_cfg.video_delay_ms : 120;
  cfg.audio_max_frames = media_cfg.audio_max_frames > 0 ? media_cfg.audio_max_frames : 256;
  cfg.video_max_frames = media_cfg.video_max_frames > 0 ? media_cfg.video_max_frames : 256;

  auto session =
      std::make_unique<mi::client::media::GroupCallSession>(*engine->transport, cfg);
  if (!session->Init(error)) {
    engine->last_error = error.empty() ? "group session init failed" : error;
    delete engine;
    return 0;
  }
  engine->group_session = std::move(session);
  engine->group_adapter =
      std::make_unique<mi::client::media::GroupCallMediaAdapter>(*engine->group_session);

  if (!InitAudioPipeline(engine, *engine->group_adapter, sample_rate, channels,
                         frame_ms)) {
    delete engine;
    return 0;
  }

  if (cfg.enable_video) {
    if (!InitVideoPipeline(engine, *engine->group_adapter, video_width,
                           video_height, video_fps)) {
      delete engine;
      return 0;
    }
  }

  return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT void JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_destroyEngine(JNIEnv*, jobject, jlong handle) {
  auto* engine = FromHandle(handle);
  delete engine;
}

extern "C" JNIEXPORT jstring JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_lastError(JNIEnv* env, jobject, jlong handle) {
  auto* engine = FromHandle(handle);
  if (!engine) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(engine->last_error.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_getAudioFrameSamples(JNIEnv*, jobject, jlong handle) {
  auto* engine = FromHandle(handle);
  if (!engine) {
    return 0;
  }
  return static_cast<jint>(engine->frame_samples);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_poll(JNIEnv*, jobject, jlong handle, jint max_packets, jint wait_ms) {
  auto* engine = FromHandle(handle);
  if (!engine) {
    return JNI_FALSE;
  }
  const std::uint32_t max_packets_u =
      max_packets > 0 ? static_cast<std::uint32_t>(max_packets) : 0u;
  const std::uint32_t wait_ms_u =
      wait_ms > 0 ? static_cast<std::uint32_t>(wait_ms) : 0u;
  std::string error;
  bool ok = true;
  if (engine->is_group) {
    if (!engine->group_session || !engine->group_adapter) {
      return JNI_FALSE;
    }
    ok = engine->group_session->PollIncoming(max_packets_u, wait_ms_u, error);
    const std::uint64_t now_ms = mi::platform::NowSteadyMs();
    mi::client::media::GroupMediaFrame frame;
    while (engine->group_session->PopAudioFrame(now_ms, frame)) {
      engine->group_adapter->PushIncoming(std::move(frame));
      frame = mi::client::media::GroupMediaFrame{};
    }
    while (engine->group_session->PopVideoFrame(now_ms, frame)) {
      engine->group_adapter->PushIncoming(std::move(frame));
      frame = mi::client::media::GroupMediaFrame{};
    }
  } else {
    if (!engine->peer_session) {
      return JNI_FALSE;
    }
    ok = engine->peer_session->PollIncoming(max_packets_u, wait_ms_u, error);
  }
  if (!ok && !error.empty()) {
    engine->last_error = error;
  }
  if (engine->audio) {
    engine->audio->PumpIncoming();
  }
  if (engine->video) {
    engine->video->PumpIncoming();
  }
  return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_sendPcm(JNIEnv* env,
                                                   jobject,
                                                   jlong handle,
                                                   jshortArray samples) {
  auto* engine = FromHandle(handle);
  if (!engine || !engine->audio || !samples) {
    return JNI_FALSE;
  }
  jsize len = env->GetArrayLength(samples);
  if (len <= 0) {
    return JNI_FALSE;
  }
  jshort* data = env->GetShortArrayElements(samples, nullptr);
  if (!data) {
    return JNI_FALSE;
  }
  const bool ok = engine->audio->SendPcmFrame(
      reinterpret_cast<const std::int16_t*>(data),
      static_cast<std::size_t>(len));
  env->ReleaseShortArrayElements(samples, data, JNI_ABORT);
  if (!ok) {
    engine->last_error = "audio send failed";
  }
  return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_sendNv12(JNIEnv* env,
                                                    jobject,
                                                    jlong handle,
                                                    jbyteArray data,
                                                    jint width,
                                                    jint height,
                                                    jint stride) {
  auto* engine = FromHandle(handle);
  if (!engine || !engine->video || !data) {
    return JNI_FALSE;
  }
  jsize len = env->GetArrayLength(data);
  if (len <= 0) {
    return JNI_FALSE;
  }
  jbyte* buf = env->GetByteArrayElements(data, nullptr);
  if (!buf) {
    return JNI_FALSE;
  }
  const bool ok = engine->video->SendNv12Frame(
      reinterpret_cast<const std::uint8_t*>(buf),
      static_cast<std::size_t>(stride > 0 ? stride : width),
      width > 0 ? static_cast<std::uint32_t>(width) : 0,
      height > 0 ? static_cast<std::uint32_t>(height) : 0);
  env->ReleaseByteArrayElements(data, buf, JNI_ABORT);
  if (!ok) {
    engine->last_error = "video send failed";
  }
  return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_popAudio(JNIEnv* env, jobject, jlong handle) {
  auto* engine = FromHandle(handle);
  if (!engine || !engine->audio) {
    return nullptr;
  }
  mi::client::media::PcmFrame frame;
  if (!engine->audio->PopDecodedFrame(frame)) {
    return nullptr;
  }
  if (frame.samples.empty()) {
    return nullptr;
  }
  jshortArray out = env->NewShortArray(static_cast<jsize>(frame.samples.size()));
  if (!out) {
    return nullptr;
  }
  env->SetShortArrayRegion(
      out, 0, static_cast<jsize>(frame.samples.size()),
      reinterpret_cast<const jshort*>(frame.samples.data()));
  return out;
}

extern "C" JNIEXPORT jobject JNICALL
Java_mi_e2ee_android_sdk_NativeMediaEngine_popVideo(JNIEnv* env, jobject, jlong handle) {
  if (!EnsureCache(env)) {
    return nullptr;
  }
  auto* engine = FromHandle(handle);
  if (!engine || !engine->video) {
    return nullptr;
  }
  mi::client::media::VideoFrameData frame;
  if (!engine->video->PopDecodedFrame(frame)) {
    return nullptr;
  }
  if (frame.nv12.empty()) {
    return nullptr;
  }
  jbyteArray data = ToJByteArray(env, frame.nv12.data(), frame.nv12.size());
  if (!data) {
    return nullptr;
  }
  jobject obj = env->NewObject(
      g_cache.cls_video_frame,
      g_cache.ctor_video_frame,
      static_cast<jint>(frame.width),
      static_cast<jint>(frame.height),
      static_cast<jint>(frame.stride),
      static_cast<jboolean>(frame.keyframe ? JNI_TRUE : JNI_FALSE),
      data);
  env->DeleteLocalRef(data);
  return obj;
}
