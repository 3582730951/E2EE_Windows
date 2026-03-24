#include "UiPathSecurity.h"

#include <QDir>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../../../shard/path_security.h"

#include <filesystem>
#endif

namespace UiPathSecurity {

bool HardenDataDirAcl(const QString& path, QString* errorMessage) {
#ifdef _WIN32
  std::string aclError;
  const std::filesystem::path fsPath(QDir::cleanPath(path).toStdWString());
  if (mi::shard::security::HardenPathAcl(fsPath, aclError)) {
    if (errorMessage) {
      errorMessage->clear();
    }
    return true;
  }
  if (errorMessage) {
    *errorMessage = QString::fromStdString(
        aclError.empty() ? "data dir acl harden failed" : aclError);
  }
  return false;
#else
  (void)path;
  if (errorMessage) {
    errorMessage->clear();
  }
  return true;
#endif
}

}  // namespace UiPathSecurity
