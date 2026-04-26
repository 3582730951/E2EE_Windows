#ifndef MI_E2EE_UI_PROTECTED_TEXT_VM_H
#define MI_E2EE_UI_PROTECTED_TEXT_VM_H

#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mi::client {

class UiProtectedTextLease {
 public:
  UiProtectedTextLease() = default;
  UiProtectedTextLease(const UiProtectedTextLease&) = delete;
  UiProtectedTextLease& operator=(const UiProtectedTextLease&) = delete;
  UiProtectedTextLease(UiProtectedTextLease&& other) noexcept;
  UiProtectedTextLease& operator=(UiProtectedTextLease&& other) noexcept;
  ~UiProtectedTextLease();

  std::string_view View();
  void WipeNow();
  bool valid() const { return valid_ && !wiped_; }

  const std::vector<char>& BufferForTest() const { return plain_; }

 private:
  friend class UiProtectedText;

  UiProtectedTextLease(std::vector<char> plain,
                       std::chrono::milliseconds ttl);

  bool Expired() const;

  std::vector<char> plain_;
  std::chrono::steady_clock::time_point expires_at_{};
  bool valid_{false};
  bool wiped_{true};
};

class UiProtectedText {
 public:
  UiProtectedText() = default;

  static UiProtectedText Protect(std::string_view plaintext);
  static UiProtectedText ProtectForTest(std::string_view plaintext,
                                        std::uint64_t seed);
  static bool ImportForTest(const std::vector<std::uint8_t>& bytecode,
                            std::uint64_t seed,
                            UiProtectedText& out);

  UiProtectedTextLease Open(std::chrono::milliseconds ttl) const;
  UiProtectedTextLease OpenForTest(std::chrono::milliseconds ttl) const {
    return Open(ttl);
  }

  template <typename Fn>
  bool WithPlaintext(std::chrono::milliseconds ttl, Fn&& fn) const {
    auto lease = Open(ttl);
    if (!lease.valid()) {
      return false;
    }
    const bool ok = static_cast<bool>(fn(lease.View()));
    lease.WipeNow();
    return ok;
  }

  const std::vector<std::uint8_t>& CiphertextForTest() const {
    return bytecode_;
  }

 private:
  bool Decode(std::vector<char>& out) const;

  std::vector<std::uint8_t> bytecode_;
  std::uint64_t seed_{0};
};

}  // namespace mi::client

#endif  // MI_E2EE_UI_PROTECTED_TEXT_VM_H
