#ifndef SHARD_SECURE_TYPES_H
#define SHARD_SECURE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace shard {

constexpr std::size_t kMetadataSize = 4;

template <std::size_t N>
struct PermutationBank {
  using Permutation = std::array<std::uint8_t, N>;

  static constexpr Permutation identity() {
    Permutation p{};
    for (std::size_t i = 0; i < N; ++i) {
      p[i] = static_cast<std::uint8_t>(i);
    }
    return p;
  }

  static constexpr Permutation reversed() {
    Permutation p{};
    for (std::size_t i = 0; i < N; ++i) {
      p[i] = static_cast<std::uint8_t>(N - 1 - i);
    }
    return p;
  }

  static const std::array<Permutation, 2> &list() {
    static const std::array<Permutation, 2> perms = {identity(), reversed()};
    return perms;
  }
};

template <>
struct PermutationBank<1> {
  using Permutation = std::array<std::uint8_t, 1>;
  static const std::array<Permutation, 1> &list() {
    static const std::array<Permutation, 1> perms = {{{0}}};
    return perms;
  }
};

template<>
struct PermutationBank<2> {
    using Permutation = std::array<uint8_t, 2>;

    static inline const std::array<Permutation, 2> perms = {{
        {0, 1},
        {1, 0}
    }};

    static const auto& list() { return perms; }
};

template<>
struct PermutationBank<4> {
    using Permutation = std::array<uint8_t, 4>;

    static inline const std::array<Permutation, 3> perms = {{
        {0, 1, 2, 3},
        {3, 2, 1, 0},
        {1, 3, 0, 2}
    }};

    static const auto& list() { return perms; }
};

template<>
struct PermutationBank<8> {
    using Permutation = std::array<uint8_t, 8>;

    static inline const std::array<Permutation, 3> perms = {{
        {0, 7, 3, 4, 6, 2, 1, 5},
        {5, 0, 7, 2, 4, 1, 3, 6},
        {3, 6, 1, 7, 0, 5, 2, 4}
    }};

    static const auto& list() { return perms; }
};


template <typename T>
class ScrambledValue {
 public:
  using value_type = T;
  // Obfuscation only; not a cryptographic protection.

  ScrambledValue() { encrypt(T{}); }
  explicit ScrambledValue(const T &value) { encrypt(value); }
  ~ScrambledValue() { buffer_.fill(0); }

  void set(const T &value) { encrypt(value); }

  T get() const {
    T value = decrypt();
    encrypt(value);  // 
    return value;
  }

  void refresh() { encrypt(decrypt()); }

  std::size_t storedSize() const { return kValueSize + kMetadataSize; }

  const std::uint8_t *encryptedData() const { return buffer_.data(); }

 private:
  static_assert(std::is_trivially_copyable_v<T>,
                "ScrambledValue requires trivially copyable type");

  static constexpr std::size_t kValueSize = sizeof(T);
  using Permutation = typename PermutationBank<kValueSize>::Permutation;

  static const auto &permutations() { return PermutationBank<kValueSize>::list(); }

  static std::mt19937 &randomEngine() {
    thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
  }

  void encrypt(const T &value) const {
    std::array<std::uint8_t, kValueSize> raw{};
    std::memcpy(raw.data(), &value, kValueSize);

    const auto &perms = permutations();
    const std::size_t permCount = perms.size();
    std::size_t permIndex = 0;
    if (permCount > 1) {
      std::uniform_int_distribution<std::size_t> dist(0, permCount - 1);
      permIndex = dist(randomEngine());
    }

    const auto &selected = perms[permIndex];
    for (std::size_t i = 0; i < kValueSize; ++i) {
      buffer_[i] = raw[selected[i]];
    }
    writePermutationIndex(static_cast<std::uint32_t>(permIndex));
  }

  T decrypt() const {
    const auto &perms = permutations();
    const std::uint32_t permIndex = readPermutationIndex();
    if (permIndex >= perms.size()) {
      throw std::runtime_error("Invalid permutation metadata");
    }

    const auto &selected = perms[permIndex];
    std::array<std::uint8_t, kValueSize> raw{};
    for (std::size_t i = 0; i < kValueSize; ++i) {
      raw[selected[i]] = buffer_[i];
    }

    T value{};
    std::memcpy(&value, raw.data(), kValueSize);
    return value;
  }

  std::uint32_t readPermutationIndex() const {
    std::uint32_t idx = 0;
    for (std::size_t i = 0; i < kMetadataSize; ++i) {
      idx |= static_cast<std::uint32_t>(buffer_[kValueSize + i]) << (i * 8);
    }
    return idx;
  }

  void writePermutationIndex(std::uint32_t index) const {
    for (std::size_t i = 0; i < kMetadataSize; ++i) {
      buffer_[kValueSize + i] =
          static_cast<std::uint8_t>((index >> (i * 8)) & 0xFF);
    }
  }

  mutable std::array<std::uint8_t, kValueSize + kMetadataSize> buffer_{};
};

class ScrambledString {
 public:
  ScrambledString() { encrypt(std::string{}); }
  explicit ScrambledString(const std::string &value) { encrypt(value); }
  ~ScrambledString() { std::fill(buffer_.begin(), buffer_.end(), 0); }

  void set(const std::string &value) { encrypt(value); }

  std::string get() const {
    auto plain = decrypt();
    encrypt(plain);
    return plain;
  }

  std::size_t size() const { return len_; }
  std::size_t storedSize() const { return buffer_.size(); }
  const std::uint8_t *encryptedData() const { return buffer_.data(); }

 private:
  void encrypt(const std::string &value) const;
  std::string decrypt() const;

  static std::mt19937 &randomEngine() {
    thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
  }

  static void writeU32(std::vector<std::uint8_t> &buf, std::size_t offset,
                       std::uint32_t v) {
    buf[offset + 0] = static_cast<std::uint8_t>(v & 0xFF);
    buf[offset + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    buf[offset + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    buf[offset + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
  }

  static std::uint32_t readU32(const std::vector<std::uint8_t> &buf,
                               std::size_t offset) {
    return static_cast<std::uint32_t>(buf[offset]) |
           (static_cast<std::uint32_t>(buf[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(buf[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(buf[offset + 3]) << 24);
  }

  static constexpr std::size_t kPermMetaSize = 4;
  static constexpr std::size_t kKeyMetaSize = 4;

  mutable std::vector<std::uint8_t> buffer_{};
  mutable std::size_t len_{0};
};

}  // namespace shard
#endif  // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shard_secure_i32 shard_secure_i32;
typedef struct shard_secure_u32 shard_secure_u32;
typedef struct shard_secure_i64 shard_secure_i64;
typedef struct shard_secure_u64 shard_secure_u64;
typedef struct shard_secure_f32 shard_secure_f32;
typedef struct shard_secure_f64 shard_secure_f64;
typedef struct shard_secure_bool shard_secure_bool;
typedef struct shard_secure_string shard_secure_string;

shard_secure_i32 *shard_secure_i32_create(int32_t value);
void shard_secure_i32_destroy(shard_secure_i32 *handle);
void shard_secure_i32_set(shard_secure_i32 *handle, int32_t value);
int32_t shard_secure_i32_get(shard_secure_i32 *handle);

shard_secure_u32 *shard_secure_u32_create(uint32_t value);
void shard_secure_u32_destroy(shard_secure_u32 *handle);
void shard_secure_u32_set(shard_secure_u32 *handle, uint32_t value);
uint32_t shard_secure_u32_get(shard_secure_u32 *handle);

shard_secure_i64 *shard_secure_i64_create(int64_t value);
void shard_secure_i64_destroy(shard_secure_i64 *handle);
void shard_secure_i64_set(shard_secure_i64 *handle, int64_t value);
int64_t shard_secure_i64_get(shard_secure_i64 *handle);

shard_secure_u64 *shard_secure_u64_create(uint64_t value);
void shard_secure_u64_destroy(shard_secure_u64 *handle);
void shard_secure_u64_set(shard_secure_u64 *handle, uint64_t value);
uint64_t shard_secure_u64_get(shard_secure_u64 *handle);

shard_secure_f32 *shard_secure_f32_create(float value);
void shard_secure_f32_destroy(shard_secure_f32 *handle);
void shard_secure_f32_set(shard_secure_f32 *handle, float value);
float shard_secure_f32_get(shard_secure_f32 *handle);

shard_secure_f64 *shard_secure_f64_create(double value);
void shard_secure_f64_destroy(shard_secure_f64 *handle);
void shard_secure_f64_set(shard_secure_f64 *handle, double value);
double shard_secure_f64_get(shard_secure_f64 *handle);

shard_secure_bool *shard_secure_bool_create(int value);
void shard_secure_bool_destroy(shard_secure_bool *handle);
void shard_secure_bool_set(shard_secure_bool *handle, int value);
int shard_secure_bool_get(shard_secure_bool *handle);

shard_secure_string *shard_secure_string_create(const char *utf8);
shard_secure_string *shard_secure_string_create_len(const char *utf8,
                                                    size_t len);
void shard_secure_string_destroy(shard_secure_string *handle);
size_t shard_secure_string_length(const shard_secure_string *handle);
int shard_secure_string_set(shard_secure_string *handle, const char *utf8,
                            size_t len);
size_t shard_secure_string_get(shard_secure_string *handle, char *out,
                               size_t buffer_len);
char *shard_secure_string_clone(shard_secure_string *handle,
                                size_t *out_len);
void shard_secure_string_free(char *buffer);

#ifdef __cplusplus
}
#endif

#endif  // SHARD_SECURE_TYPES_H
