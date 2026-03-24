#pragma once

#include <QString>

namespace UiPathSecurity {

bool HardenDataDirAcl(const QString& path, QString* errorMessage = nullptr);

}  // namespace UiPathSecurity
