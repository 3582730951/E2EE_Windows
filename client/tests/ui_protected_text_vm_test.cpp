#include "protected_text_vm.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

bool require_true(bool value, const char* message) {
  if (!value) {
    std::fprintf(stderr, "ui_protected_text_vm_test failed: %s\n", message);
  }
  return value;
}

bool contains_plaintext(const std::vector<std::uint8_t>& haystack,
                       const std::string& needle) {
  if (needle.empty() || haystack.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (haystack[i + j] != static_cast<std::uint8_t>(needle[j])) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

bool all_zero(const std::vector<char>& bytes) {
  for (char ch : bytes) {
    if (ch != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const std::string plaintext = "unlock-capture-secret";
  auto protected_text = mi::client::UiProtectedText::ProtectForTest(
      plaintext, 0x6d695f75695f766du);

  if (!require_true(!contains_plaintext(protected_text.CiphertextForTest(), plaintext),
               "protected UI ciphertext contains plaintext")) {
    return 1;
  }

  bool callback_called = false;
  protected_text.WithPlaintext(
      std::chrono::milliseconds(50),
      [&](std::string_view view) {
        callback_called = true;
        return require_true(view == plaintext,
                       "protected UI plaintext callback mismatch");
      });
  if (!require_true(callback_called, "protected UI plaintext callback not called")) {
    return 1;
  }

  auto lease = protected_text.OpenForTest(std::chrono::milliseconds(50));
  if (!require_true(lease.View() == plaintext, "protected UI lease mismatch") ||
      !require_true(!all_zero(lease.BufferForTest()),
               "protected UI lease was wiped before use")) {
    return 1;
  }
  lease.WipeNow();
  if (!require_true(lease.View().empty(), "wiped UI lease still exposes text") ||
      !require_true(all_zero(lease.BufferForTest()),
               "wiped UI lease buffer not zeroed")) {
    return 1;
  }

  auto expiring = protected_text.OpenForTest(std::chrono::milliseconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if (!require_true(expiring.View().empty(),
               "expired UI lease still exposes plaintext") ||
      !require_true(all_zero(expiring.BufferForTest()),
               "expired UI lease buffer not zeroed")) {
    return 1;
  }

  auto tampered = protected_text.CiphertextForTest();
  if (!tampered.empty()) {
    tampered[0] ^= 0x7F;
  }
  mi::client::UiProtectedText rejected;
  if (!require_true(!mi::client::UiProtectedText::ImportForTest(
                   tampered, 0x6d695f75695f766du, rejected),
               "tampered UI protected bytecode was accepted")) {
    return 1;
  }

  return 0;
}
