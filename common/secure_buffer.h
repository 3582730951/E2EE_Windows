#ifndef MI_E2EE_SECURE_BUFFER_H
#define MI_E2EE_SECURE_BUFFER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace mi::common {

inline void SecureWipe(void* data, std::size_t len) {
  if (!data || len == 0) {
    return;
  }
  volatile std::uint8_t* p = reinterpret_cast<volatile std::uint8_t*>(data);
  while (len--) {
    *p++ = 0;
  }
}

inline void SecureWipe(std::vector<std::uint8_t>& buf) {
  SecureWipe(buf.data(), buf.size());
}

template <std::size_t N>
inline void SecureWipe(std::array<std::uint8_t, N>& buf) {
  SecureWipe(buf.data(), buf.size());
}

class ScopedWipe {
 public:
  ScopedWipe() = default;

  ScopedWipe(void* data, std::size_t len) : data_(data), len_(len) {}

  explicit ScopedWipe(std::vector<std::uint8_t>& buf)
      : data_(buf.data()), len_(buf.size()) {}

  template <std::size_t N>
  explicit ScopedWipe(std::array<std::uint8_t, N>& buf)
      : data_(buf.data()), len_(buf.size()) {}

  explicit ScopedWipe(std::string& text)
      : data_(text.empty() ? nullptr : text.data()), len_(text.size()) {}

  ScopedWipe(const ScopedWipe&) = delete;
  ScopedWipe& operator=(const ScopedWipe&) = delete;

  ScopedWipe(ScopedWipe&& other) noexcept
      : data_(other.data_), len_(other.len_) {
    other.data_ = nullptr;
    other.len_ = 0;
  }

  ScopedWipe& operator=(ScopedWipe&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    Wipe();
    data_ = other.data_;
    len_ = other.len_;
    other.data_ = nullptr;
    other.len_ = 0;
    return *this;
  }

  ~ScopedWipe() { Wipe(); }

  void Release() {
    data_ = nullptr;
    len_ = 0;
  }

 private:
  void Wipe() {
    if (data_ && len_ > 0) {
      SecureWipe(data_, len_);
    }
  }

  void* data_{nullptr};
  std::size_t len_{0};
};

class SecureBuffer {
 public:
  SecureBuffer() = default;
  explicit SecureBuffer(std::size_t size) : data_(size) {}

  SecureBuffer(const std::uint8_t* data, std::size_t len)
      : data_(data, data + len) {}

  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;

  SecureBuffer(SecureBuffer&& other) noexcept
      : data_(std::move(other.data_)) {
    other.data_.clear();
  }

  SecureBuffer& operator=(SecureBuffer&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    SecureWipe(data_);
    data_ = std::move(other.data_);
    other.data_.clear();
    return *this;
  }

  ~SecureBuffer() { SecureWipe(data_); }

  std::uint8_t* data() { return data_.data(); }
  const std::uint8_t* data() const { return data_.data(); }
  std::size_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  void resize(std::size_t size) { data_.resize(size); }

  void assign(const std::uint8_t* data, std::size_t len) {
    data_.assign(data, data + len);
  }

  std::vector<std::uint8_t>& bytes() { return data_; }
  const std::vector<std::uint8_t>& bytes() const { return data_; }

 private:
  std::vector<std::uint8_t> data_;
};

}  // namespace mi::common

#endif  // MI_E2EE_SECURE_BUFFER_H
