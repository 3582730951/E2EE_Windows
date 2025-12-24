// Runtime path setup for bundled UI assets and plugins.
#pragma once

#include <QString>

namespace UiRuntimePaths {

void Prepare(const char *argv0);
QString AppRootDir();
QString RuntimeDir();

}  // namespace UiRuntimePaths
