#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifndef MI_E2EE_MESSAGING_SERVICE_SOURCE
#define MI_E2EE_MESSAGING_SERVICE_SOURCE ""
#endif

namespace {

bool ReadFile(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

bool Contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

std::string_view FunctionBody(std::string_view source, std::string_view name) {
  const std::string marker = "bool MessagingService::" + std::string(name);
  const std::size_t start = source.find(marker);
  if (start == std::string_view::npos) {
    return {};
  }
  const std::size_t next =
      source.find("\nbool MessagingService::", start + marker.size());
  if (next == std::string_view::npos) {
    return source.substr(start);
  }
  return source.substr(start, next - start);
}

bool Require(bool value, const std::string& message) {
  if (!value) {
    std::cerr << "messaging_history_source_test failed: " << message << "\n";
  }
  return value;
}

}  // namespace

int main() {
  const std::filesystem::path source_path = MI_E2EE_MESSAGING_SERVICE_SOURCE;
  std::string source;
  if (!Require(!source_path.empty(), "messaging service source path empty") ||
      !Require(ReadFile(source_path, source), "failed to read messaging service source")) {
    return 1;
  }

  const char* methods[] = {
      "SendGroupInvite",
      "SendChatLocation",
      "SendChatContactCard",
      "SendChatSticker",
      "SendChatReadReceipt",
      "SendChatTyping",
      "SendChatPresence",
      "SendChatFile",
  };

  bool ok = true;
  for (const char* method : methods) {
    const std::string_view body = FunctionBody(source, method);
    ok &= Require(!body.empty(), std::string(method) + " missing");
    ok &= Require(Contains(body, "BestEffortPersistHistoryEnvelope("),
                  std::string(method) + " must persist its canonical envelope");
    ok &= Require(!Contains(body, "return core.PushDeviceSyncCiphertext(event_cipher);"),
                  std::string(method) + " must not bypass history on linked devices");
    ok &= Require(!Contains(body, "if (!core.PushDeviceSyncCiphertext(event_cipher))"),
                  std::string(method) + " must persist failed linked-device sends");
    ok &= Require(!Contains(body, "return core.SendPrivateE2ee(peer_username, envelope);"),
                  std::string(method) + " must not bypass history on primary devices");
    ok &= Require(!Contains(body, "if (!core.SendPrivateE2ee(peer_username, envelope))"),
                  std::string(method) + " must persist failed primary sends");
  }

  return ok ? 0 : 1;
}
