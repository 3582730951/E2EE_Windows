#ifndef MI_E2EE_CLIENT_CONFIG_SERVICE_H
#define MI_E2EE_CLIENT_CONFIG_SERVICE_H

#include <filesystem>
#include <string>

#include "client_config.h"

namespace mi::client {

class ConfigService {
 public:
  bool Load(const std::string& config_path, ClientConfig& out_cfg,
            std::string& error);

  const std::filesystem::path& config_dir() const { return config_dir_; }
  const std::filesystem::path& data_dir() const { return data_dir_; }

 private:
  std::filesystem::path config_dir_;
  std::filesystem::path data_dir_;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CONFIG_SERVICE_H
