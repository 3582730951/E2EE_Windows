// Runtime path setup for bundled UI assets and plugins.
#include "UiRuntimePaths.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

QString ResolveAppDir(const char *argv0) {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return QFileInfo(QString::fromWCharArray(path, static_cast<int>(len))).absolutePath();
    }
#endif
    if (argv0 && *argv0) {
        const QFileInfo info(QString::fromLocal8Bit(argv0));
        if (info.isAbsolute()) {
            return info.absolutePath();
        }
        return QDir::current().absoluteFilePath(info.filePath());
    }
    return QDir::currentPath();
}

}  // namespace

namespace UiRuntimePaths {

void Prepare(const char *argv0) {
    const QString appDir = ResolveAppDir(argv0);
    if (appDir.isEmpty()) {
        return;
    }
    const QString runtimeDir = appDir + QStringLiteral("/runtime");
    const QString pluginDir = runtimeDir + QStringLiteral("/plugins");
    const QString platformDir = pluginDir + QStringLiteral("/platforms");

#ifdef _WIN32
    if (QDir(runtimeDir).exists()) {
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(runtimeDir.utf16()));
    }
#endif

    if (QDir(pluginDir).exists() && qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", pluginDir.toUtf8());
    }
    if (QDir(platformDir).exists() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH")) {
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", platformDir.toUtf8());
    }
}

}  // namespace UiRuntimePaths
