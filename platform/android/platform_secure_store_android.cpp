#include "platform_secure_store.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <jni.h>

#include "monocypher.h"
#include "platform_fs.h"
#include "platform_random.h"

namespace mi::platform {

namespace {
constexpr char kLegacyBlobMagic[] = "MI_E2EE_SECURE_STORE_V1";
constexpr char kKeystoreBlobMagic[] = "MI_E2EE_SECURE_STORE_KS1";
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kNonceBytes = 24;
constexpr std::size_t kTagBytes = 16;
constexpr char kKeyFileName[] = "mi_e2ee_secure_store.key";

std::filesystem::path ResolveBaseDir() {
  if (const char* env = std::getenv("MI_E2EE_DATA_DIR")) {
    if (*env != '\0') {
      return std::filesystem::path(env);
    }
  }
  std::error_code ec;
  auto cwd = fs::CurrentPath(ec);
  if (!cwd.empty()) {
    return cwd;
  }
  return std::filesystem::path{"."};
}

std::filesystem::path KeyPath() {
  return ResolveBaseDir() / kKeyFileName;
}

void SetOwnerOnlyPermissions(const std::filesystem::path& path) {
#ifndef _WIN32
  std::error_code ec;
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace, ec);
#else
  (void)path;
#endif
}

bool ReadKeyFile(std::array<std::uint8_t, kKeyBytes>& key, std::string& error) {
  error.clear();
  const auto path = KeyPath();
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "secure store key not found";
    return false;
  }
  f.read(reinterpret_cast<char*>(key.data()), key.size());
  if (!f || f.gcount() != static_cast<std::streamsize>(key.size())) {
    error = "secure store key read failed";
    return false;
  }
  return true;
}

bool WriteKeyFile(const std::array<std::uint8_t, kKeyBytes>& key,
                  std::string& error) {
  error.clear();
  const auto path = KeyPath();
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::CreateDirectories(path.parent_path(), ec);
    if (ec) {
      error = "secure store dir create failed";
      return false;
    }
  }
  if (!fs::AtomicWrite(path, key.data(), key.size(), ec)) {
    error = "secure store key write failed";
    return false;
  }
  SetOwnerOnlyPermissions(path);
  return true;
}

bool GetOrCreateMasterKey(std::array<std::uint8_t, kKeyBytes>& key,
                          std::string& error) {
  error.clear();
  static bool cached = false;
  static std::array<std::uint8_t, kKeyBytes> cached_key{};
  static std::mutex cache_mu;

  {
    std::lock_guard<std::mutex> lock(cache_mu);
    if (cached) {
      key = cached_key;
      return true;
    }
  }

  std::string read_err;
  if (!ReadKeyFile(key, read_err)) {
    if (!RandomBytes(key.data(), key.size())) {
      error = "secure store rng failed";
      return false;
    }
    std::string write_err;
    if (!WriteKeyFile(key, write_err)) {
      error = write_err.empty() ? "secure store key write failed" : write_err;
      return false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(cache_mu);
    cached_key = key;
    cached = true;
  }
  return true;
}

bool StartsWithBytes(const std::vector<std::uint8_t>& blob,
                     const char* magic,
                     std::size_t magic_len) {
  if (!magic || magic_len == 0 || blob.size() < magic_len) {
    return false;
  }
  return std::memcmp(blob.data(), magic, magic_len) == 0;
}

bool ParseLegacyEncryptedBlob(const std::vector<std::uint8_t>& blob,
                              std::array<std::uint8_t, kNonceBytes>& nonce,
                              std::array<std::uint8_t, kTagBytes>& tag,
                              std::vector<std::uint8_t>& cipher) {
  const std::size_t magic_len = sizeof(kLegacyBlobMagic) - 1;
  if (blob.size() < magic_len + nonce.size() + tag.size()) {
    return false;
  }
  if (!StartsWithBytes(blob, kLegacyBlobMagic, magic_len)) {
    return false;
  }
  std::size_t off = magic_len;
  std::memcpy(nonce.data(), blob.data() + off, nonce.size());
  off += nonce.size();
  std::memcpy(tag.data(), blob.data() + off, tag.size());
  off += tag.size();
  cipher.assign(blob.begin() + static_cast<std::ptrdiff_t>(off), blob.end());
  return true;
}

std::mutex g_jni_mu;
JavaVM* g_vm = nullptr;
jclass g_secure_store_class = nullptr;
jmethodID g_secure_store_encrypt = nullptr;
jmethodID g_secure_store_decrypt = nullptr;
jmethodID g_secure_store_supported = nullptr;
jmethodID g_secure_store_last_error = nullptr;
std::atomic<int> g_keystore_disabled{-1};
std::atomic<int> g_keystore_required{-1};

bool IsKeystoreBlob(const std::vector<std::uint8_t>& blob) {
  return StartsWithBytes(blob, kKeystoreBlobMagic,
                         sizeof(kKeystoreBlobMagic) - 1);
}

bool IsLegacyBlob(const std::vector<std::uint8_t>& blob) {
  return StartsWithBytes(blob, kLegacyBlobMagic,
                         sizeof(kLegacyBlobMagic) - 1);
}

bool KeystoreDisabled() {
  int cached = g_keystore_disabled.load();
  if (cached >= 0) {
    return cached != 0;
  }
  const char* env = std::getenv("MI_E2EE_ANDROID_DISABLE_KEYSTORE");
  bool disabled = false;
  if (env && *env != '\0') {
    const std::string v(env);
    disabled = (v == "1" || v == "true" || v == "TRUE" || v == "on" || v == "ON");
  }
  g_keystore_disabled.store(disabled ? 1 : 0);
  return disabled;
}

bool KeystoreRequired() {
  int cached = g_keystore_required.load();
  if (cached >= 0) {
    return cached != 0;
  }
  const char* env = std::getenv("MI_E2EE_ANDROID_REQUIRE_KEYSTORE");
  bool required = false;
  if (env && *env != '\0') {
    const std::string v(env);
    required = (v == "1" || v == "true" || v == "TRUE" || v == "on" || v == "ON");
  }
  g_keystore_required.store(required ? 1 : 0);
  return required;
}

bool GetJniEnv(JNIEnv*& env, bool& did_attach, std::string& error) {
  env = nullptr;
  did_attach = false;
  if (!g_vm) {
    error = "android jvm unavailable";
    return false;
  }
  const jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (rc == JNI_OK) {
    return true;
  }
  if (rc != JNI_EDETACHED) {
    error = "android jni env unavailable";
    return false;
  }
  if (g_vm->AttachCurrentThread(&env, nullptr) != 0 || !env) {
    error = "android jni attach failed";
    return false;
  }
  did_attach = true;
  return true;
}

void DetachIfNeeded(bool did_attach) {
  if (did_attach && g_vm) {
    g_vm->DetachCurrentThread();
  }
}

jbyteArray ToJByteArray(JNIEnv* env, const std::uint8_t* data, std::size_t len) {
  if (!env) {
    return nullptr;
  }
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(len));
  if (arr && data && len > 0) {
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(len),
                            reinterpret_cast<const jbyte*>(data));
  }
  return arr;
}

bool JByteArrayToVector(JNIEnv* env, jbyteArray input,
                        std::vector<std::uint8_t>& out) {
  out.clear();
  if (!env || !input) {
    return false;
  }
  const jsize len = env->GetArrayLength(input);
  if (len <= 0) {
    return true;
  }
  out.resize(static_cast<std::size_t>(len));
  env->GetByteArrayRegion(input, 0, len,
                          reinterpret_cast<jbyte*>(out.data()));
  return true;
}

std::string GetJavaLastError(JNIEnv* env) {
  if (!env || !g_secure_store_class || !g_secure_store_last_error) {
    return "secure store keystore error";
  }
  jstring jerr = static_cast<jstring>(
      env->CallStaticObjectMethod(g_secure_store_class,
                                  g_secure_store_last_error));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return "secure store keystore error";
  }
  if (!jerr) {
    return "secure store keystore error";
  }
  const char* utf8 = env->GetStringUTFChars(jerr, nullptr);
  std::string out = utf8 ? utf8 : "";
  if (utf8) {
    env->ReleaseStringUTFChars(jerr, utf8);
  }
  env->DeleteLocalRef(jerr);
  if (out.empty()) {
    return "secure store keystore error";
  }
  return out;
}

bool EnsureSecureStoreJni(JNIEnv* env, std::string& error) {
  std::lock_guard<std::mutex> lock(g_jni_mu);
  if (g_secure_store_class && g_secure_store_encrypt &&
      g_secure_store_decrypt) {
    return true;
  }
  if (!env) {
    error = "secure store jni unavailable";
    return false;
  }
  jclass local = env->FindClass("mi/e2ee/android/sdk/AndroidSecureStore");
  if (!local) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
    error = "secure store class missing";
    return false;
  }
  g_secure_store_class =
      reinterpret_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  if (!g_secure_store_class) {
    error = "secure store class init failed";
    return false;
  }
  g_secure_store_encrypt = env->GetStaticMethodID(
      g_secure_store_class, "encrypt", "([B[B)[B");
  g_secure_store_decrypt = env->GetStaticMethodID(
      g_secure_store_class, "decrypt", "([B[B)[B");
  g_secure_store_supported = env->GetStaticMethodID(
      g_secure_store_class, "isSupported", "()Z");
  g_secure_store_last_error = env->GetStaticMethodID(
      g_secure_store_class, "lastError", "()Ljava/lang/String;");
  if (!g_secure_store_encrypt || !g_secure_store_decrypt) {
    error = "secure store methods missing";
    return false;
  }
  return true;
}

bool KeystoreSupported(JNIEnv* env) {
  if (!env || !g_secure_store_supported || !g_secure_store_class) {
    return true;
  }
  const jboolean ok =
      env->CallStaticBooleanMethod(g_secure_store_class,
                                   g_secure_store_supported);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  return ok == JNI_TRUE;
}

bool KeystoreEncrypt(const std::vector<std::uint8_t>& plain,
                     const std::uint8_t* entropy,
                     std::size_t entropy_len,
                     std::vector<std::uint8_t>& out,
                     std::string& error) {
  out.clear();
  error.clear();
  if (KeystoreDisabled()) {
    error = "secure store keystore disabled";
    return false;
  }
  JNIEnv* env = nullptr;
  bool did_attach = false;
  if (!GetJniEnv(env, did_attach, error)) {
    return false;
  }
  if (!EnsureSecureStoreJni(env, error)) {
    DetachIfNeeded(did_attach);
    return false;
  }
  if (!KeystoreSupported(env)) {
    error = "secure store keystore unsupported";
    DetachIfNeeded(did_attach);
    return false;
  }

  jbyteArray jplain = ToJByteArray(env, plain.data(), plain.size());
  jbyteArray jentropy = nullptr;
  if (entropy && entropy_len > 0) {
    jentropy = ToJByteArray(env, entropy, entropy_len);
  }
  jbyteArray result = static_cast<jbyteArray>(
      env->CallStaticObjectMethod(g_secure_store_class,
                                  g_secure_store_encrypt,
                                  jplain, jentropy));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    error = GetJavaLastError(env);
    if (jplain) env->DeleteLocalRef(jplain);
    if (jentropy) env->DeleteLocalRef(jentropy);
    DetachIfNeeded(did_attach);
    return false;
  }
  if (!result) {
    error = GetJavaLastError(env);
    if (jplain) env->DeleteLocalRef(jplain);
    if (jentropy) env->DeleteLocalRef(jentropy);
    DetachIfNeeded(did_attach);
    return false;
  }
  JByteArrayToVector(env, result, out);
  env->DeleteLocalRef(result);
  if (jplain) env->DeleteLocalRef(jplain);
  if (jentropy) env->DeleteLocalRef(jentropy);
  DetachIfNeeded(did_attach);
  if (out.empty()) {
    error = "secure store keystore failed";
    return false;
  }
  return true;
}

bool KeystoreDecrypt(const std::vector<std::uint8_t>& blob,
                     const std::uint8_t* entropy,
                     std::size_t entropy_len,
                     std::vector<std::uint8_t>& out,
                     std::string& error) {
  out.clear();
  error.clear();
  if (KeystoreDisabled()) {
    error = "secure store keystore disabled";
    return false;
  }
  JNIEnv* env = nullptr;
  bool did_attach = false;
  if (!GetJniEnv(env, did_attach, error)) {
    return false;
  }
  if (!EnsureSecureStoreJni(env, error)) {
    DetachIfNeeded(did_attach);
    return false;
  }
  if (!KeystoreSupported(env)) {
    error = "secure store keystore unsupported";
    DetachIfNeeded(did_attach);
    return false;
  }

  jbyteArray jblob = ToJByteArray(env, blob.data(), blob.size());
  jbyteArray jentropy = nullptr;
  if (entropy && entropy_len > 0) {
    jentropy = ToJByteArray(env, entropy, entropy_len);
  }
  jbyteArray result = static_cast<jbyteArray>(
      env->CallStaticObjectMethod(g_secure_store_class,
                                  g_secure_store_decrypt,
                                  jblob, jentropy));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    error = GetJavaLastError(env);
    if (jblob) env->DeleteLocalRef(jblob);
    if (jentropy) env->DeleteLocalRef(jentropy);
    DetachIfNeeded(did_attach);
    return false;
  }
  if (!result) {
    error = GetJavaLastError(env);
    if (jblob) env->DeleteLocalRef(jblob);
    if (jentropy) env->DeleteLocalRef(jentropy);
    DetachIfNeeded(did_attach);
    return false;
  }
  JByteArrayToVector(env, result, out);
  env->DeleteLocalRef(result);
  if (jblob) env->DeleteLocalRef(jblob);
  if (jentropy) env->DeleteLocalRef(jentropy);
  DetachIfNeeded(did_attach);
  if (out.empty()) {
    error = "secure store keystore failed";
    return false;
  }
  return true;
}

bool ProtectSecureBlobLegacy(const std::vector<std::uint8_t>& plain,
                             const std::uint8_t* entropy,
                             std::size_t entropy_len,
                             std::vector<std::uint8_t>& out,
                             std::string& error) {
  out.clear();
  error.clear();
  if (plain.empty()) {
    error = "secure store plain empty";
    return false;
  }

  std::array<std::uint8_t, kKeyBytes> key{};
  if (!GetOrCreateMasterKey(key, error)) {
    return false;
  }
  std::array<std::uint8_t, kNonceBytes> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    error = "secure store rng failed";
    return false;
  }
  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, kTagBytes> tag{};

  const std::uint8_t* ad = entropy_len > 0 ? entropy : nullptr;
  const std::size_t ad_len = entropy_len > 0 ? entropy_len : 0;
  crypto_aead_lock(cipher.data(), tag.data(), key.data(), nonce.data(), ad,
                   ad_len, plain.data(), plain.size());

  const std::size_t magic_len = sizeof(kLegacyBlobMagic) - 1;
  out.reserve(magic_len + nonce.size() + tag.size() + cipher.size());
  out.insert(out.end(), kLegacyBlobMagic, kLegacyBlobMagic + magic_len);
  out.insert(out.end(), nonce.begin(), nonce.end());
  out.insert(out.end(), tag.begin(), tag.end());
  out.insert(out.end(), cipher.begin(), cipher.end());
  return true;
}

bool UnprotectSecureBlobLegacy(const std::vector<std::uint8_t>& blob,
                               const std::uint8_t* entropy,
                               std::size_t entropy_len,
                               std::vector<std::uint8_t>& out,
                               std::string& error) {
  out.clear();
  error.clear();
  if (blob.empty()) {
    error = "secure store blob empty";
    return false;
  }
  std::array<std::uint8_t, kKeyBytes> key{};
  if (!GetOrCreateMasterKey(key, error)) {
    return false;
  }
  std::array<std::uint8_t, kNonceBytes> nonce{};
  std::array<std::uint8_t, kTagBytes> tag{};
  std::vector<std::uint8_t> cipher;
  if (!ParseLegacyEncryptedBlob(blob, nonce, tag, cipher)) {
    error = "secure store blob invalid";
    return false;
  }
  out.resize(cipher.size());
  const std::uint8_t* ad = entropy_len > 0 ? entropy : nullptr;
  const std::size_t ad_len = entropy_len > 0 ? entropy_len : 0;
  const int rc = crypto_aead_unlock(out.data(), tag.data(), key.data(),
                                    nonce.data(), ad, ad_len, cipher.data(),
                                    cipher.size());
  if (rc != 0) {
    out.clear();
    error = "secure store auth failed";
    return false;
  }
  return true;
}

}  // namespace

bool SecureStoreSupported() {
  return true;
}

bool ProtectSecureBlob(const std::vector<std::uint8_t>& plain,
                       const std::uint8_t* entropy,
                       std::size_t entropy_len,
                       std::vector<std::uint8_t>& out,
                       std::string& error) {
  error.clear();
  out.clear();
  if (plain.empty()) {
    error = "secure store plain empty";
    return false;
  }

  std::string ks_err;
  if (KeystoreEncrypt(plain, entropy, entropy_len, out, ks_err)) {
    return true;
  }
  if (KeystoreRequired()) {
    error = ks_err.empty() ? "secure store keystore required" : ks_err;
    return false;
  }

  std::string legacy_err;
  if (ProtectSecureBlobLegacy(plain, entropy, entropy_len, out, legacy_err)) {
    return true;
  }

  error = !legacy_err.empty() ? legacy_err : ks_err;
  if (error.empty()) {
    error = "secure store protect failed";
  }
  return false;
}

bool UnprotectSecureBlob(const std::vector<std::uint8_t>& blob,
                         const std::uint8_t* entropy,
                         std::size_t entropy_len,
                         std::vector<std::uint8_t>& out,
                         std::string& error) {
  error.clear();
  out.clear();
  if (blob.empty()) {
    error = "secure store blob empty";
    return false;
  }

  const bool is_keystore = IsKeystoreBlob(blob);
  const bool is_legacy = IsLegacyBlob(blob);
  if (is_keystore) {
    return KeystoreDecrypt(blob, entropy, entropy_len, out, error);
  }
  if (is_legacy) {
    if (KeystoreRequired()) {
      error = "secure store keystore required";
      return false;
    }
    return UnprotectSecureBlobLegacy(blob, entropy, entropy_len, out, error);
  }

  std::string ks_err;
  if (KeystoreDecrypt(blob, entropy, entropy_len, out, ks_err)) {
    return true;
  }
  if (KeystoreRequired()) {
    error = ks_err.empty() ? "secure store keystore required" : ks_err;
    return false;
  }
  std::string legacy_err;
  if (UnprotectSecureBlobLegacy(blob, entropy, entropy_len, out, legacy_err)) {
    return true;
  }
  error = !ks_err.empty() ? ks_err : legacy_err;
  if (error.empty()) {
    error = "secure store blob invalid";
  }
  return false;
}

bool ProtectSecureBlobScoped(const std::vector<std::uint8_t>& plain,
                             const std::uint8_t* entropy,
                             std::size_t entropy_len,
                             SecureStoreScope /*scope*/,
                             std::vector<std::uint8_t>& out,
                             std::string& error) {
  return ProtectSecureBlob(plain, entropy, entropy_len, out, error);
}

bool UnprotectSecureBlobScoped(const std::vector<std::uint8_t>& blob,
                               const std::uint8_t* entropy,
                               std::size_t entropy_len,
                               SecureStoreScope /*scope*/,
                               std::vector<std::uint8_t>& out,
                               std::string& error) {
  return UnprotectSecureBlob(blob, entropy, entropy_len, out, error);
}

namespace android {

void SetJavaVm(JavaVM* vm) {
  std::lock_guard<std::mutex> lock(g_jni_mu);
  g_vm = vm;
}

void RegisterSecureStore(JNIEnv* env) {
  std::string error;
  (void)EnsureSecureStoreJni(env, error);
}

}  // namespace android

}  // namespace mi::platform
