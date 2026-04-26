#include "protected_text_vm.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

bool Require(bool value, const char* message) {
  if (!value) {
    std::fprintf(stderr, "ui_protected_text_vm_test failed: %s\n", message);
  }
  return value;
}

bool ContainsPlaintext(const std::vector<std::uint8_t>& haystack,
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

bool AllZero(const std::vector<char>& bytes) {
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

  if (!Require(!ContainsPlaintext(protected_text.CiphertextForTest(), plaintext),
               "protected UI ciphertext contains plaintext")) {
    return 1;
  }

  bool callback_called = false;
  protected_text.WithPlaintext(
      std::chrono::milliseconds(50),
      [&](std::string_view view) {
        callback_called = true;
        return Require(view == plaintext,
                       "protected UI plaintext callback mismatch");
      });
  if (!Require(callback_called, "protected UI plaintext callback not called")) {
    return 1;
  }

  auto lease = protected_text.OpenForTest(std::chrono::milliseconds(50));
  if (!Require(lease.View() == plaintext, "protected UI lease mismatch") ||
      !Require(!AllZero(lease.BufferForTest()),
               "protected UI lease was wiped before use")) {
    return 1;
  }
  lease.WipeNow();
  if (!Require(lease.View().empty(), "wiped UI lease still exposes text") ||
      !Require(AllZero(lease.BufferForTest()),
               "wiped UI lease buffer not zeroed")) {
    return 1;
  }

  auto expiring = protected_text.OpenForTest(std::chrono::milliseconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if (!Require(expiring.View().empty(),
               "expired UI lease still exposes plaintext") ||
      !Require(AllZero(expiring.BufferForTest()),
               "expired UI lease buffer not zeroed")) {
    return 1;
  }

  auto tampered = protected_text.CiphertextForTest();
  if (!tampered.empty()) {
    tampered[0] ^= 0x7F;
  }
  mi::client::UiProtectedText rejected;
  if (!Require(!mi::client::UiProtectedText::ImportForTest(
                   tampered, 0x6d695f75695f766du, rejected),
               "tampered UI protected bytecode was accepted")) {
    return 1;
  }

  return 0;
}
