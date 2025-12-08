#include "secure_types.h"

#ifdef __cplusplus

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename T>
struct SecureWrapper {
  shard::ScrambledValue<T> value;
};

}  // namespace

namespace shard {

void ScrambledString::encrypt(const std::string &value) const {
  len_ = value.size();
  const std::uint32_t key = randomEngine()();
  const std::uint8_t keyBytes[4] = {
      static_cast<std::uint8_t>(key & 0xFF),
      static_cast<std::uint8_t>((key >> 8) & 0xFF),
      static_cast<std::uint8_t>((key >> 16) & 0xFF),
      static_cast<std::uint8_t>((key >> 24) & 0xFF)};

  const std::uint32_t permIndex = (len_ > 1) ? (randomEngine()() & 0x1u) : 0u;

  buffer_.assign(len_ + kPermMetaSize + kKeyMetaSize, 0);
  for (std::size_t i = 0; i < len_; ++i) {
    const std::size_t target = (permIndex == 0) ? i : (len_ - 1 - i);
    const std::uint8_t b =
        static_cast<std::uint8_t>(static_cast<unsigned char>(value[i]));
    buffer_[target] = static_cast<std::uint8_t>(b ^ keyBytes[i & 3]);
  }

  writeU32(buffer_, len_, permIndex);
  writeU32(buffer_, len_ + kPermMetaSize, key);
}

std::string ScrambledString::decrypt() const {
  if (buffer_.size() < kPermMetaSize + kKeyMetaSize) {
    return std::string{};
  }

  const std::uint32_t permIndex = readU32(buffer_, len_);
  const std::uint32_t key = readU32(buffer_, len_ + kPermMetaSize);
  const std::uint8_t keyBytes[4] = {
      static_cast<std::uint8_t>(key & 0xFF),
      static_cast<std::uint8_t>((key >> 8) & 0xFF),
      static_cast<std::uint8_t>((key >> 16) & 0xFF),
      static_cast<std::uint8_t>((key >> 24) & 0xFF)};

  std::string plain(len_, '\0');
  for (std::size_t i = 0; i < len_; ++i) {
    const std::size_t original = (permIndex == 0) ? i : (len_ - 1 - i);
    const std::uint8_t enc = buffer_[i];
    plain[original] = static_cast<char>(enc ^ keyBytes[i & 3]);
  }
  return plain;
}

}  // namespace shard

extern "C" {

struct shard_secure_i32 {
  SecureWrapper<int32_t> impl;
};

struct shard_secure_u32 {
  SecureWrapper<uint32_t> impl;
};

struct shard_secure_i64 {
  SecureWrapper<int64_t> impl;
};

struct shard_secure_u64 {
  SecureWrapper<uint64_t> impl;
};

struct shard_secure_f32 {
  SecureWrapper<float> impl;
};

struct shard_secure_f64 {
  SecureWrapper<double> impl;
};

struct shard_secure_bool {
  SecureWrapper<bool> impl;
};

struct shard_secure_string {
  shard::ScrambledString impl;
};

shard_secure_i32 *shard_secure_i32_create(int32_t value) {
  try {
    return new shard_secure_i32{SecureWrapper<int32_t>{shard::ScrambledValue<int32_t>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_i32_destroy(shard_secure_i32 *handle) { delete handle; }

void shard_secure_i32_set(shard_secure_i32 *handle, int32_t value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

int32_t shard_secure_i32_get(shard_secure_i32 *handle) {
  if (!handle) {
    return 0;
  }
  return handle->impl.value.get();
}

shard_secure_u32 *shard_secure_u32_create(uint32_t value) {
  try {
    return new shard_secure_u32{SecureWrapper<uint32_t>{shard::ScrambledValue<uint32_t>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_u32_destroy(shard_secure_u32 *handle) { delete handle; }

void shard_secure_u32_set(shard_secure_u32 *handle, uint32_t value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

uint32_t shard_secure_u32_get(shard_secure_u32 *handle) {
  if (!handle) {
    return 0;
  }
  return handle->impl.value.get();
}

shard_secure_i64 *shard_secure_i64_create(int64_t value) {
  try {
    return new shard_secure_i64{SecureWrapper<int64_t>{shard::ScrambledValue<int64_t>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_i64_destroy(shard_secure_i64 *handle) { delete handle; }

void shard_secure_i64_set(shard_secure_i64 *handle, int64_t value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

int64_t shard_secure_i64_get(shard_secure_i64 *handle) {
  if (!handle) {
    return 0;
  }
  return handle->impl.value.get();
}

shard_secure_u64 *shard_secure_u64_create(uint64_t value) {
  try {
    return new shard_secure_u64{SecureWrapper<uint64_t>{shard::ScrambledValue<uint64_t>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_u64_destroy(shard_secure_u64 *handle) { delete handle; }

void shard_secure_u64_set(shard_secure_u64 *handle, uint64_t value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

uint64_t shard_secure_u64_get(shard_secure_u64 *handle) {
  if (!handle) {
    return 0;
  }
  return handle->impl.value.get();
}

shard_secure_f32 *shard_secure_f32_create(float value) {
  try {
    return new shard_secure_f32{SecureWrapper<float>{shard::ScrambledValue<float>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_f32_destroy(shard_secure_f32 *handle) { delete handle; }

void shard_secure_f32_set(shard_secure_f32 *handle, float value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

float shard_secure_f32_get(shard_secure_f32 *handle) {
  if (!handle) {
    return 0.0f;
  }
  return handle->impl.value.get();
}

shard_secure_f64 *shard_secure_f64_create(double value) {
  try {
    return new shard_secure_f64{SecureWrapper<double>{shard::ScrambledValue<double>(value)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_f64_destroy(shard_secure_f64 *handle) { delete handle; }

void shard_secure_f64_set(shard_secure_f64 *handle, double value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value);
}

double shard_secure_f64_get(shard_secure_f64 *handle) {
  if (!handle) {
    return 0.0;
  }
  return handle->impl.value.get();
}

shard_secure_bool *shard_secure_bool_create(int value) {
  try {
    return new shard_secure_bool{
        SecureWrapper<bool>{shard::ScrambledValue<bool>(value != 0)}};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_bool_destroy(shard_secure_bool *handle) { delete handle; }

void shard_secure_bool_set(shard_secure_bool *handle, int value) {
  if (!handle) {
    return;
  }
  handle->impl.value.set(value != 0);
}

int shard_secure_bool_get(shard_secure_bool *handle) {
  if (!handle) {
    return 0;
  }
  return handle->impl.value.get() ? 1 : 0;
}

shard_secure_string *shard_secure_string_create(const char *utf8) {
  const std::string value = utf8 ? std::string(utf8) : std::string{};
  try {
    return new shard_secure_string{shard::ScrambledString(value)};
  } catch (...) {
    return nullptr;
  }
}

shard_secure_string *shard_secure_string_create_len(const char *utf8,
                                                    size_t len) {
  const std::string value = (utf8 && len > 0) ? std::string(utf8, len)
                                              : std::string{};
  try {
    return new shard_secure_string{shard::ScrambledString(value)};
  } catch (...) {
    return nullptr;
  }
}

void shard_secure_string_destroy(shard_secure_string *handle) { delete handle; }

size_t shard_secure_string_length(const shard_secure_string *handle) {
  return handle ? handle->impl.size() : 0;
}

int shard_secure_string_set(shard_secure_string *handle, const char *utf8,
                            size_t len) {
  if (!handle || !utf8) {
    return -1;
  }
  handle->impl.set(std::string(utf8, len));
  return 0;
}

size_t shard_secure_string_get(shard_secure_string *handle, char *out,
                               size_t buffer_len) {
  if (!handle) {
    return 0;
  }
  const std::string plain = handle->impl.get();
  const size_t need = plain.size();
  if (out && buffer_len > 0) {
    const size_t copy_len = (buffer_len - 1 < need) ? (buffer_len - 1) : need;
    if (copy_len > 0) {
      std::memcpy(out, plain.data(), copy_len);
    }
    out[copy_len] = '\0';
  }
  return need;
}

char *shard_secure_string_clone(shard_secure_string *handle,
                                size_t *out_len) {
  if (!handle) {
    return nullptr;
  }
  const std::string plain = handle->impl.get();
  char *buf = static_cast<char *>(std::malloc(plain.size() + 1));
  if (!buf) {
    return nullptr;
  }
  if (!plain.empty()) {
    std::memcpy(buf, plain.data(), plain.size());
  }
  buf[plain.size()] = '\0';
  if (out_len) {
    *out_len = plain.size();
  }
  return buf;
}

void shard_secure_string_free(char *buffer) {
  std::free(buffer);
}

}  // extern "C"

#endif  // __cplusplus
