#include <cassert>
#include <fstream>
#include <string>

#include "config.h"

using mi::server::AuthMode;
using mi::server::ServerConfig;
using mi::server::LoadConfig;

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  {
    const std::string path = "tmp_config_mysql.ini";
    WriteFile(path,
              "[mode]\nmode=0\n"
              "[mysql]\nmysql_ip=127.0.0.1\nmysql_port=3306\n"
              "mysql_database=test\nmysql_username=root\nmysql_password=pass\n"
              "[server]\nlist_port=9000\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.mode == AuthMode::kMySQL);
    assert(cfg.mysql.host == "127.0.0.1");
    assert(cfg.mysql.port == 3306);
    assert(cfg.server.listen_port == 9000);
  }

  {
    const std::string path = "tmp_config_demo.ini";
    WriteFile(path,
              "[mode]\nmode=1\n"
              "[server]\nlist_port=8000\n");
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig(path, cfg, err);
    assert(ok);
    assert(cfg.mode == AuthMode::kDemo);
    assert(cfg.server.listen_port == 8000);
  }

  {
    ServerConfig cfg;
    std::string err;
    bool ok = LoadConfig("missing.ini", cfg, err);
    assert(!ok);
  }

  return 0;
}
