#include "config_service.h"

namespace mi::client {

bool ConfigService::Load(const std::string& config_path,
                         ClientConfig& out_cfg,
                         std::string& error) {
  config_dir_ = ResolveConfigDir(config_path);
  data_dir_ = ResolveDataDir(config_dir_);
  return LoadClientConfig(config_path, out_cfg, error);
}

}  // namespace mi::client
