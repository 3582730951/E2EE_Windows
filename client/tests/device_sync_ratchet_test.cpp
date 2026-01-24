#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "client_core.h"
#include "sync_service.h"

namespace {

#define FAIL()                                                         \
  do {                                                                 \
    std::cerr << "device_sync_ratchet_test failed at " << __FILE__     \
              << ":" << __LINE__ << "\n";                              \
    return 1;                                                          \
  } while (false)

std::filesystem::path MakeTempDir(const std::string& name_prefix) {
  std::error_code ec;
  auto base = std::filesystem::temp_directory_path(ec);
  if (base.empty()) {
    base = std::filesystem::current_path(ec);
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }
  std::filesystem::path dir = base / name_prefix;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void WriteConfig(const std::filesystem::path& path,
                 const std::string& role_line) {
  std::ofstream out(path);
  out << "[client]\n";
  out << "server_ip=127.0.0.1\n";
  out << "server_port=9000\n";
  out << "use_tls=0\n";
  out << "require_tls=0\n";
  out << "tls_verify_mode=ca\n";
  out << "auth_mode=opaque\n";
  out << "allow_legacy_login=0\n";
  out << "\n";
  out << "[device_sync]\n";
  out << "enabled=1\n";
  out << role_line;
  out << "key_path=device_sync_key.bin\n";
  out << "rotate_interval_sec=0\n";
  out << "rotate_message_limit=0\n";
  out << "ratchet_enable=1\n";
  out << "ratchet_max_skip=32\n";
  out.close();
}

bool EqualBytes(const std::vector<std::uint8_t>& a,
                const std::vector<std::uint8_t>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

bool InitCore(mi::client::ClientCore& core, const std::filesystem::path& dir,
              const std::string& role_line) {
  const auto cfg_path = dir / "client_config.ini";
  WriteConfig(cfg_path, role_line);
  return core.Init(cfg_path.string());
}

}  // namespace

int main() {
  const auto sender_dir = MakeTempDir("mi_e2ee_device_sync_sender");
  const auto receiver_dir = MakeTempDir("mi_e2ee_device_sync_receiver");

  mi::client::ClientCore sender;
  mi::client::ClientCore receiver;
  if (!InitCore(sender, sender_dir, "role=primary\n")) {
    FAIL();
  }
  if (!InitCore(receiver, receiver_dir, "role=linked\n")) {
    FAIL();
  }

  std::array<std::uint8_t, 32> key{};
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(i);
  }

  mi::client::SyncService sync;
  if (!sync.StoreDeviceSyncKey(sender, key)) {
    FAIL();
  }
  if (!sync.StoreDeviceSyncKey(receiver, key)) {
    FAIL();
  }

  const std::vector<std::uint8_t> msg1 = {'h', 'e', 'l', 'l', 'o'};
  std::vector<std::uint8_t> cipher1;
  if (!sync.EncryptDeviceSync(sender, msg1, cipher1)) {
    FAIL();
  }
  std::vector<std::uint8_t> out1;
  if (!sync.DecryptDeviceSync(receiver, cipher1, out1)) {
    FAIL();
  }
  if (!EqualBytes(msg1, out1)) {
    FAIL();
  }

  const std::vector<std::uint8_t> msg2 = {'w', 'o', 'r', 'l', 'd'};
  std::vector<std::uint8_t> cipher2;
  if (!sync.EncryptDeviceSync(sender, msg2, cipher2)) {
    FAIL();
  }
  std::vector<std::uint8_t> out2;
  if (!sync.DecryptDeviceSync(receiver, cipher2, out2)) {
    FAIL();
  }
  if (!EqualBytes(msg2, out2)) {
    FAIL();
  }

  std::vector<std::uint8_t> out_replay;
  if (sync.DecryptDeviceSync(receiver, cipher1, out_replay)) {
    FAIL();
  }

  return 0;
}
