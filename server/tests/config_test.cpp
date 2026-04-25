#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "config.h"
#include "test_permissions.h"

using mi::server::AuthMode;
using mi::server::ServerConfig;
using mi::server::LoadConfig;

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
  f.close();
  mi::server::test::SetOwnerOnlyFile(path);
}

int main() {
  std::error_code ec;
  const auto test_root =
      std::filesystem::temp_directory_path(ec) / "mi_e2ee_config_test";
  if (ec) {
    return 1;
  }
  std::filesystem::remove_all(test_root, ec);
  if (!mi::server::test::EnsureOwnerOnlyDirectory(test_root)) {
    return 1;
  }
  std::filesystem::current_path(test_root, ec);
  if (ec) {
    return 1;
  }

  {
    const std::string path = "tmp_config_mysql.ini";
    WriteFile(path,
              "[mode]  # auth mode\nmode=0  # mysql\n"
              "[mysql]\nmysql_ip=127.0.0.1\nmysql_port=3306\n"
              "mysql_database=test\nmysql_username=root\nmysql_password=pass\n"
              "[server]\nlist_port=9000  # listen port\n"
              "max_connections=10\n"
              "max_connections_per_ip=3\n"
              "max_connection_bytes=65536\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.mode == AuthMode::kMySQL);
    assert(cfg.mysql.host == "127.0.0.1");
    assert(cfg.mysql.port == 3306);
    assert(cfg.server.listen_port == 9000);
    assert(cfg.server.max_connections == 10);
    assert(cfg.server.max_connections_per_ip == 3);
    assert(cfg.server.max_connection_bytes == 65536);
    assert(cfg.server.tls_enable);
    assert(cfg.server.require_tls);
  }

  {
    const std::string path = "tmp_config_demo.ini";
    WriteFile(path,
              "[mode]  # auth mode\nmode=1  # demo\n"
              "[server]\nlist_port=8000  # listen port\n"
              "offline_blob_temp_budget_bytes=536870912\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.mode == AuthMode::kDemo);
    assert(cfg.server.listen_port == 8000);
    assert(cfg.server.offline_blob_temp_budget_bytes == 536870912ull);
    assert(cfg.server.tls_enable);
    assert(cfg.server.require_tls);
  }

  {
    const std::string path = "tmp_config_blob_budget_too_small.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\noffline_blob_temp_budget_bytes=1024\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_require_tls_fail.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nrequire_tls=1\ntls_enable=0\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_require_tls_ok.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nrequire_tls=1\ntls_enable=1\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.server.require_tls);
    assert(cfg.server.tls_enable);
  }

  {
    const std::string path = "tmp_config_conn_bytes_too_small.ini";
    WriteFile(path, "[mode]\nmode=1\n[server]\nlist_port=8000\nmax_connection_bytes=1\n"
                    "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_secure_delete_missing.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nsecure_delete_enabled=1\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_secure_delete_hash_missing.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nsecure_delete_enabled=1\n"
              "secure_delete_plugin=secure_delete_plugin.dll\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_secure_delete_ok.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nsecure_delete_enabled=1\n"
              "secure_delete_plugin=secure_delete_plugin.dll\n"
              "secure_delete_plugin_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.server.secure_delete_enabled);
  }

  {
    const std::string path = "tmp_config_secure_delete_required_fail.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nsecure_delete_required=1\n"
              "secure_delete_enabled=0\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_secure_delete_required_ok.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nsecure_delete_required=1\n"
              "secure_delete_enabled=1\n"
              "secure_delete_plugin=secure_delete_plugin.dll\n"
              "secure_delete_plugin_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.server.secure_delete_required);
  }

  {
    const std::string path = "tmp_config_kcp_policy_fail.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\n"
              "kt_signing_key=kt_signing_key.bin\n"
              "[kcp]\nenable=1\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_kcp_policy_ok.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\n"
              "kt_signing_key=kt_signing_key.bin\n"
              "[kcp]\nenable=1\nallow_insecure=1\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.server.kcp_enable);
    assert(cfg.server.kcp_allow_insecure);
  }

  {
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig("missing.ini", cfg, err);
    assert(!ok);
  }

#ifdef MI_E2EE_PRIVACY_STRICT
  {
    const std::string path = "tmp_config_privacy_debug_log.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\ndebug_log=1\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_privacy_ops.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nops_enable=1\n"
              "ops_token=abcdefghijklmnop\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }

  {
    const std::string path = "tmp_config_privacy_ops_remote.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\nops_allow_remote=1\n"
              "kt_signing_key=kt_signing_key.bin\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(!ok);
  }
#endif

  return 0;
}
