#include "chat_history_store.h"
#include "client_core.h"
#include "protocol.h"
#include "test_permissions.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool Require(bool value, const char* message) {
  if (!value) {
    std::fprintf(stderr, "chat_history_store_test failed: %s\n", message);
  }
  return value;
}

void WriteLe16(std::uint16_t value, std::vector<std::uint8_t>& out) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out) {
  out.clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());
  return true;
}

bool WriteFileBytes(const std::filesystem::path& path,
                    const std::vector<std::uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  out.close();
  return out.good();
}

bool ContainsBytes(const std::vector<std::uint8_t>& haystack,
                   const std::vector<std::uint8_t>& needle) {
  if (needle.empty() || haystack.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (haystack[i + j] != needle[j]) {
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

bool TamperFirstWrappedRecord(std::vector<std::uint8_t>& bytes) {
  const std::array<std::uint8_t, 4> magic{{'M', 'I', 'H', '2'}};
  for (std::size_t i = 4; i + magic.size() + 8 < bytes.size(); ++i) {
    if (!std::equal(magic.begin(), magic.end(), bytes.begin() + i)) {
      continue;
    }
    const std::uint32_t record_len =
        static_cast<std::uint32_t>(bytes[i - 4]) |
        (static_cast<std::uint32_t>(bytes[i - 3]) << 8) |
        (static_cast<std::uint32_t>(bytes[i - 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[i - 1]) << 24);
    if (record_len < 16 || i + record_len > bytes.size()) {
      continue;
    }
    bytes[i + record_len - 1] ^= 0x5A;
    return true;
  }
  return false;
}

bool HasPlatformContainerShape(const std::vector<std::uint8_t>& bytes) {
#if defined(_WIN32)
  return bytes.size() >= 2 && bytes[0] == 'M' && bytes[1] == 'Z';
#elif defined(__APPLE__)
  if (bytes.size() < 4) {
    return false;
  }
  const std::uint32_t magic =
      (static_cast<std::uint32_t>(bytes[0]) << 24) |
      (static_cast<std::uint32_t>(bytes[1]) << 16) |
      (static_cast<std::uint32_t>(bytes[2]) << 8) |
      static_cast<std::uint32_t>(bytes[3]);
  return magic == 0xFEEDFACFu || magic == 0xCFFAEDFEu ||
         magic == 0xFEEDFACEu || magic == 0xCEFAEDFEu;
#else
  return bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' &&
         bytes[2] == 'L' && bytes[3] == 'F';
#endif
}

bool HasPlatformContainerExtension(const std::filesystem::path& path) {
#if defined(_WIN32)
  return path.extension() == ".dll";
#elif defined(__APPLE__)
  return path.extension() == ".dylib";
#else
  return path.extension() == ".so";
#endif
}

std::vector<std::uint8_t> BuildUnknownEnvelope() {
  std::vector<std::uint8_t> out;
  out.insert(out.end(), {'M', 'I', 'C', 'H'});
  out.push_back(1);
  out.push_back(0xF1);
  for (std::uint8_t i = 0; i < 16; ++i) {
    out.push_back(static_cast<std::uint8_t>(0xA0u + i));
  }
  WriteLe16(4, out);
  out.insert(out.end(), {'T', 'L', 'V', 0});
  return out;
}

std::array<std::uint8_t, 16> MakeId(std::uint8_t seed) {
  std::array<std::uint8_t, 16> out{};
  for (std::uint8_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

std::array<std::uint8_t, 32> MakeKey(std::uint8_t seed) {
  std::array<std::uint8_t, 32> out{};
  for (std::uint8_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

std::vector<std::uint8_t> MakeBytes(std::uint8_t seed, std::size_t size) {
  std::vector<std::uint8_t> out(size);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(seed + i);
  }
  return out;
}

std::vector<std::uint8_t> ToVector(const std::array<std::uint8_t, 32>& in) {
  return {in.begin(), in.end()};
}

std::vector<std::uint8_t> StartEnvelope(std::uint8_t type, std::uint8_t seed) {
  std::vector<std::uint8_t> out;
  out.insert(out.end(), {'M', 'I', 'C', 'H'});
  out.push_back(1);
  out.push_back(type);
  const auto id = MakeId(seed);
  out.insert(out.end(), id.begin(), id.end());
  return out;
}

std::vector<std::uint8_t> BuildControlEnvelope(std::uint8_t type,
                                               std::uint8_t seed) {
  return StartEnvelope(type, seed);
}

std::vector<std::uint8_t> BuildTextEnvelope(std::uint8_t seed,
                                            const std::string& text) {
  auto out = StartEnvelope(1, seed);
  mi::server::proto::WriteString(text, out);
  return out;
}

std::vector<std::uint8_t> BuildFileEnvelope(
    std::uint8_t seed,
    bool group,
    const std::array<std::uint8_t, 32>& file_key) {
  auto out = StartEnvelope(group ? 6 : 3, seed);
  if (group) {
    mi::server::proto::WriteString("group-alpha", out);
  }
  mi::server::proto::WriteUint64(123456, out);
  mi::server::proto::WriteString("photo.bin", out);
  mi::server::proto::WriteString("file-id-alpha", out);
  out.insert(out.end(), file_key.begin(), file_key.end());
  return out;
}

std::vector<std::uint8_t> BuildGroupTextEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(4, seed);
  mi::server::proto::WriteString("group-alpha", out);
  mi::server::proto::WriteString("group text", out);
  return out;
}

std::vector<std::uint8_t> BuildGroupInviteEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(5, seed);
  mi::server::proto::WriteString("group-alpha", out);
  return out;
}

std::vector<std::uint8_t> BuildSenderKeyDistEnvelope(
    std::uint8_t seed,
    const std::array<std::uint8_t, 32>& sender_key) {
  auto out = StartEnvelope(7, seed);
  mi::server::proto::WriteString("group-alpha", out);
  mi::server::proto::WriteUint32(2, out);
  mi::server::proto::WriteUint32(3, out);
  mi::server::proto::WriteBytes(sender_key.data(), sender_key.size(), out);
  const auto sig = MakeBytes(0x60, 64);
  mi::server::proto::WriteBytes(sig, out);
  return out;
}

std::vector<std::uint8_t> BuildSenderKeyReqEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(8, seed);
  mi::server::proto::WriteString("group-alpha", out);
  mi::server::proto::WriteUint32(2, out);
  return out;
}

std::vector<std::uint8_t> BuildRichTextEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(9, seed);
  out.push_back(1);
  out.push_back(1);
  const auto reply = MakeId(0x80);
  out.insert(out.end(), reply.begin(), reply.end());
  mi::server::proto::WriteString("reply preview", out);
  mi::server::proto::WriteString("rich text", out);
  return out;
}

std::vector<std::uint8_t> BuildRichLocationEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(9, seed);
  out.push_back(2);
  out.push_back(0);
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(399087000), out);
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(-77036000), out);
  mi::server::proto::WriteString("map pin", out);
  return out;
}

std::vector<std::uint8_t> BuildRichContactEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(9, seed);
  out.push_back(3);
  out.push_back(0);
  mi::server::proto::WriteString("alice", out);
  mi::server::proto::WriteString("Alice", out);
  return out;
}

std::vector<std::uint8_t> BuildBoolControlEnvelope(std::uint8_t type,
                                                   std::uint8_t seed,
                                                   bool value) {
  auto out = StartEnvelope(type, seed);
  out.push_back(value ? 1 : 0);
  return out;
}

std::vector<std::uint8_t> BuildStickerEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(12, seed);
  mi::server::proto::WriteString("sticker-smile", out);
  return out;
}

std::vector<std::uint8_t> BuildCallKeyDistEnvelope(
    std::uint8_t seed,
    const std::array<std::uint8_t, 32>& call_key) {
  auto out = StartEnvelope(14, seed);
  mi::server::proto::WriteString("group-alpha", out);
  const auto call_id = MakeId(0x90);
  out.insert(out.end(), call_id.begin(), call_id.end());
  mi::server::proto::WriteUint32(4, out);
  mi::server::proto::WriteBytes(call_key.data(), call_key.size(), out);
  const auto sig = MakeBytes(0xA0, 64);
  mi::server::proto::WriteBytes(sig, out);
  return out;
}

std::vector<std::uint8_t> BuildCallKeyReqEnvelope(std::uint8_t seed) {
  auto out = StartEnvelope(15, seed);
  mi::server::proto::WriteString("group-alpha", out);
  const auto call_id = MakeId(0xB0);
  out.insert(out.end(), call_id.begin(), call_id.end());
  mi::server::proto::WriteUint32(4, out);
  return out;
}

std::uint8_t SummaryKind(const std::vector<std::uint8_t>& summary) {
  const std::size_t kind_offset = mi::client::kHistorySummaryMagic.size() + 1;
  if (summary.size() <= kind_offset) {
    return 0;
  }
  return summary[kind_offset];
}

std::size_t CountPlatformContainers(const std::filesystem::path& dir) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec) || ec) {
    return 0;
  }
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      return count;
    }
    if (entry.is_regular_file(ec) &&
        HasPlatformContainerExtension(entry.path())) {
      ++count;
    }
  }
  return count;
}

}  // namespace

int main() {
  {
    mi::client::ClientCore core;
    if (!Require(!core.history_enabled(), "history must be disabled by default")) {
      return 1;
    }
  }

  std::error_code ec;
  const auto root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_history_store_test";
  if (!Require(!ec, "temp directory unavailable")) {
    return 1;
  }
  std::filesystem::remove_all(root, ec);
  if (!Require(mi::client::test::EnsureOwnerOnlyDirectory(root),
               "test directory setup failed")) {
    return 1;
  }

  mi::client::ChatHistoryStore store;
  std::string error;
  if (!Require(store.Init(root / "e2ee_state", "alice", error),
               "history store init failed")) {
    return 1;
  }

  struct Case {
    std::vector<std::uint8_t> envelope;
    std::uint8_t expected_summary_kind{0};
    std::vector<std::uint8_t> sensitive_bytes;
  };

  const auto file_key = MakeKey(0x20);
  const auto group_file_key = MakeKey(0x30);
  const auto sender_key = MakeKey(0x40);
  const auto call_key = MakeKey(0x50);
  std::vector<Case> cases;
  cases.push_back({BuildUnknownEnvelope(), 255, {}});
  cases.push_back({BuildTextEnvelope(0x01, "hello"), 1, {}});
  cases.push_back({BuildControlEnvelope(2, 0x02), 7, {}});
  cases.push_back({BuildFileEnvelope(0x03, false, file_key), 2,
                   ToVector(file_key)});
  cases.push_back({BuildGroupTextEnvelope(0x04), 1, {}});
  cases.push_back({BuildGroupInviteEnvelope(0x05), 6, {}});
  cases.push_back({BuildFileEnvelope(0x06, true, group_file_key), 2,
                   ToVector(group_file_key)});
  cases.push_back({BuildSenderKeyDistEnvelope(0x07, sender_key), 7,
                   ToVector(sender_key)});
  cases.push_back({BuildSenderKeyReqEnvelope(0x08), 7, {}});
  cases.push_back({BuildRichTextEnvelope(0x09), 1, {}});
  cases.push_back({BuildControlEnvelope(10, 0x0A), 7, {}});
  cases.push_back({BuildBoolControlEnvelope(11, 0x0B, true), 7, {}});
  cases.push_back({BuildStickerEnvelope(0x0C), 3, {}});
  cases.push_back({BuildBoolControlEnvelope(13, 0x0D, true), 7, {}});
  cases.push_back({BuildCallKeyDistEnvelope(0x0E, call_key), 7,
                   ToVector(call_key)});
  cases.push_back({BuildCallKeyReqEnvelope(0x0F), 7, {}});
  cases.push_back({BuildRichLocationEnvelope(0x10), 4, {}});
  cases.push_back({BuildRichContactEnvelope(0x11), 5, {}});

  std::uint64_t ts = 42;
  for (const auto& c : cases) {
    if (!Require(store.AppendEnvelope(false, false, "bob", "bob", c.envelope,
                                      mi::client::ChatHistoryStatus::kDelivered,
                                      ts++, error),
                 "append envelope failed")) {
      return 1;
    }
  }
  if (!Require(store.Flush(error), "flush failed")) {
    return 1;
  }

  const auto status_envelope =
      BuildTextEnvelope(0x21, "status update target");
  if (!Require(store.AppendEnvelope(false, true, "carol", "alice",
                                    status_envelope,
                                    mi::client::ChatHistoryStatus::kSent,
                                    ts++, error),
               "append status target envelope failed") ||
      !Require(store.AppendStatusUpdate(
                   false, "carol", MakeId(0x21),
                   mi::client::ChatHistoryStatus::kRead, ts++, error),
               "append trailing status update failed")) {
    return 1;
  }

  const auto early_status_envelope =
      BuildTextEnvelope(0x22, "early status target");
  if (!Require(store.AppendStatusUpdate(
                   false, "dave", MakeId(0x22),
                   mi::client::ChatHistoryStatus::kDelivered, ts++, error),
               "append leading status update failed") ||
      !Require(store.AppendEnvelope(false, false, "dave", "dave",
                                    early_status_envelope,
                                    mi::client::ChatHistoryStatus::kSent,
                                    ts++, error),
               "append early status target envelope failed")) {
    return 1;
  }

  const std::string system_text = "local system notice canary";
  if (!Require(store.AppendSystem(false, "system-conv", system_text, ts++,
                                  error),
               "append system message failed") ||
      !Require(store.Flush(error), "second flush failed")) {
    return 1;
  }

  std::vector<mi::client::ChatHistoryMessage> loaded;
  if (!Require(store.LoadConversation(false, "bob", 10, loaded, error),
               "load conversation failed")) {
    return 1;
  }
  if (!Require(loaded.size() == 10, "expected limited loaded messages")) {
    return 1;
  }

  std::vector<mi::client::ChatHistoryMessage> all_loaded;
  if (!Require(store.LoadConversation(false, "bob", 0, all_loaded, error),
               "load all conversation failed") ||
      !Require(all_loaded.size() == cases.size(),
               "expected all loaded messages")) {
    return 1;
  }
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const auto& c = cases[i];
    const auto& m = all_loaded[i];
    if (!Require(m.envelope == c.envelope,
                 "canonical envelope did not round-trip exactly") ||
        !Require(m.summary.size() >=
                     mi::client::kHistorySummaryMagic.size() + 2,
                 "summary too short") ||
        !Require(std::equal(mi::client::kHistorySummaryMagic.begin(),
                            mi::client::kHistorySummaryMagic.end(),
                            m.summary.begin()),
                 "summary magic mismatch") ||
        !Require(m.summary[mi::client::kHistorySummaryMagic.size()] ==
                     mi::client::kHistorySummaryVersion,
                 "summary version mismatch") ||
        !Require(SummaryKind(m.summary) == c.expected_summary_kind,
                 "summary kind mismatch")) {
      return 1;
    }
    if (!c.sensitive_bytes.empty() &&
        !Require(!ContainsBytes(m.summary, c.sensitive_bytes),
                 "summary leaked sensitive key bytes")) {
      return 1;
    }
  }

  std::vector<mi::client::ChatHistoryMessage> status_loaded;
  if (!Require(store.LoadConversation(false, "carol", 0, status_loaded, error),
               "load status conversation failed") ||
      !Require(status_loaded.size() == 1,
               "status update created extra messages") ||
      !Require(status_loaded[0].envelope == status_envelope,
               "status target envelope did not round-trip") ||
      !Require(status_loaded[0].status ==
                   mi::client::ChatHistoryStatus::kRead,
               "trailing status update was not applied")) {
    return 1;
  }

  std::vector<mi::client::ChatHistoryMessage> early_status_loaded;
  if (!Require(store.LoadConversation(false, "dave", 0, early_status_loaded,
                                      error),
               "load early status conversation failed") ||
      !Require(early_status_loaded.size() == 1,
               "early status update created extra messages") ||
      !Require(early_status_loaded[0].envelope == early_status_envelope,
               "early status target envelope did not round-trip") ||
      !Require(early_status_loaded[0].status ==
                   mi::client::ChatHistoryStatus::kDelivered,
               "leading status update was not applied")) {
    return 1;
  }

  const auto same_id_text = BuildTextEnvelope(0x23, "same id text");
  const auto same_id_read = BuildControlEnvelope(10, 0x23);
  if (!Require(store.AppendEnvelope(false, false, "erin", "erin",
                                    same_id_text,
                                    mi::client::ChatHistoryStatus::kSent,
                                    ts++, error),
               "append same-id text failed") ||
      !Require(store.AppendEnvelope(false, false, "erin", "erin",
                                    same_id_read,
                                    mi::client::ChatHistoryStatus::kRead,
                                    ts++, error),
               "append same-id control failed") ||
      !Require(store.Flush(error), "same-id flush failed")) {
    return 1;
  }
  std::vector<mi::client::ChatHistoryMessage> same_id_loaded;
  if (!Require(store.LoadConversation(false, "erin", 0, same_id_loaded, error),
               "load same-id conversation failed") ||
      !Require(same_id_loaded.size() == 2,
               "same-id messages with different types were collapsed") ||
      !Require(same_id_loaded[0].envelope == same_id_text,
               "same-id text envelope was replaced") ||
      !Require(same_id_loaded[1].envelope == same_id_read,
               "same-id control envelope missing")) {
    return 1;
  }

  const auto snapshot_text = BuildTextEnvelope(0x24, "snapshot visible");
  if (!Require(store.AppendEnvelope(false, false, "frank", "frank",
                                    snapshot_text,
                                    mi::client::ChatHistoryStatus::kSent,
                                    ts++, error),
               "append snapshot text failed")) {
    return 1;
  }
  for (std::uint8_t i = 0; i < 6; ++i) {
    if (!Require(store.AppendEnvelope(false, false, "frank", "frank",
                                      BuildControlEnvelope(10, 0x25 + i),
                                      mi::client::ChatHistoryStatus::kRead,
                                      ts++, error),
                 "append snapshot control failed")) {
      return 1;
    }
  }
  if (!Require(store.Flush(error), "snapshot priority flush failed")) {
    return 1;
  }
  std::vector<mi::client::ChatHistoryMessage> snapshot_loaded;
  if (!Require(store.ExportRecentSnapshot(1, 1, snapshot_loaded, error),
               "export recent snapshot failed") ||
      !Require(snapshot_loaded.size() == 1,
               "snapshot priority returned wrong count") ||
      !Require(snapshot_loaded[0].envelope == snapshot_text,
               "snapshot controls starved visible history")) {
    return 1;
  }

  std::vector<mi::client::ChatHistoryMessage> system_loaded;
  if (!Require(store.LoadConversation(false, "system-conv", 0, system_loaded,
                                      error),
               "load system conversation failed") ||
      !Require(system_loaded.size() == 1, "system message missing") ||
      !Require(system_loaded[0].is_system, "system message flag missing") ||
      !Require(system_loaded[0].system_text_utf8 == system_text,
               "system message text did not round-trip")) {
    return 1;
  }

  bool saw_container = false;
  std::vector<std::filesystem::path> container_paths;
  const std::vector<std::uint8_t> system_plain(system_text.begin(),
                                               system_text.end());
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    (void)entry;
  }
  const auto database_dir = root / "database";
  for (const auto& entry : std::filesystem::directory_iterator(database_dir, ec)) {
    if (!Require(!ec, "database directory iteration failed")) {
      return 1;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    std::vector<std::uint8_t> bytes;
    if (!Require(ReadFileBytes(entry.path(), bytes), "container read failed") ||
        !Require(HasPlatformContainerExtension(entry.path()),
                 "history container extension is not platform dynamic library") ||
        !Require(HasPlatformContainerShape(bytes),
                 "history container is not platform dynamic-library-shaped") ||
        !Require(ContainsBytes(bytes, {'M', 'I', 'H', '3'}),
                 "history container missing MIH3 block")) {
      return 1;
    }
    for (const auto& c : cases) {
      if (!Require(!ContainsBytes(bytes, c.envelope),
                   "history container leaked canonical envelope plaintext")) {
        return 1;
      }
      if (!c.sensitive_bytes.empty() &&
          !Require(!ContainsBytes(bytes, c.sensitive_bytes),
                   "history container leaked sensitive key bytes")) {
        return 1;
      }
    }
    if (!Require(!ContainsBytes(bytes, status_envelope),
                 "history container leaked status target plaintext") ||
        !Require(!ContainsBytes(bytes, early_status_envelope),
                 "history container leaked early status target plaintext") ||
        !Require(!ContainsBytes(bytes, system_plain),
                 "history container leaked system plaintext")) {
      return 1;
    }
    saw_container = true;
    container_paths.push_back(entry.path());
  }
  if (!Require(saw_container, "history container not created")) {
    return 1;
  }

  if (!Require(!container_paths.empty(), "history container path missing")) {
    return 1;
  }
  for (const auto& container_path : container_paths) {
    std::vector<std::uint8_t> tampered_bytes;
    if (!Require(ReadFileBytes(container_path, tampered_bytes),
                 "history container reread failed") ||
        !Require(tampered_bytes.size() > 64, "history container too small") ||
        !Require(TamperFirstWrappedRecord(tampered_bytes),
                 "history wrapped record not found for tamper test") ||
        !Require(WriteFileBytes(container_path, tampered_bytes),
                 "history container tamper write failed")) {
      return 1;
    }
  }
  mi::client::ChatHistoryStore tampered_store;
  if (!Require(tampered_store.Init(root / "e2ee_state", "alice", error),
               "tampered history store init failed")) {
    return 1;
  }
  std::vector<mi::client::ChatHistoryMessage> tampered_loaded;
  const bool tampered_load_ok =
      tampered_store.LoadConversation(false, "bob", 0, tampered_loaded, error);
  if (!Require(!tampered_load_ok || tampered_loaded.empty(),
               "tampered history returned readable messages")) {
    return 1;
  }

  const auto clear_root = root / "clear_all_case";
  std::filesystem::remove_all(clear_root, ec);
  if (!Require(mi::client::test::EnsureOwnerOnlyDirectory(clear_root),
               "clear-all test directory setup failed")) {
    return 1;
  }
  mi::client::ChatHistoryStore clear_store;
  if (!Require(clear_store.Init(clear_root / "e2ee_state", "clear_user",
                                error),
               "clear-all store init failed") ||
      !Require(clear_store.AppendEnvelope(
                   false, false, "clear-conv", "peer",
                   BuildTextEnvelope(0x31, "clear all target"),
                   mi::client::ChatHistoryStatus::kSent, 1, error),
               "clear-all append failed") ||
      !Require(clear_store.Flush(error), "clear-all flush failed")) {
    return 1;
  }
  const auto clear_database_dir = clear_root / "database";
  if (!Require(CountPlatformContainers(clear_database_dir) > 0,
               "clear-all setup did not create a platform container") ||
      !Require(clear_store.ClearAll(true, true, error),
               "clear-all failed") ||
      !Require(CountPlatformContainers(clear_database_dir) == 0,
               "clear-all left platform history containers behind")) {
    return 1;
  }
  return 0;
}
