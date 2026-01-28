#pragma once

#include <memory>
#include <string>

#include "config.h"
#include "metadata_protector.h"
#include "state_store.h"

namespace mi::server {

std::unique_ptr<StateStore> CreateMysqlStateStore(
    const MySqlConfig& cfg,
    MetadataProtector* metadata_protector,
    std::string& error);

}  // namespace mi::server
