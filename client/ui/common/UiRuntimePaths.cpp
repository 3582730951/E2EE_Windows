// Runtime path setup for bundled UI assets and plugins.
#include "UiRuntimePaths.h"

#include <QCoreApplication>
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

QString ResolveAppRoot(const QString &appDir) {
    if (appDir.isEmpty()) {
        return {};
    }
    QDir dir(appDir);
    if (dir.dirName().compare(QStringLiteral("runtime"), Qt::CaseInsensitive) == 0) {
        if (dir.cdUp()) {
            return dir.absolutePath();
        }
    }
    return dir.absolutePath();
}

QString ResolveRuntimeDir(const QString &appDir) {
    if (appDir.isEmpty()) {
        return {};
    }
    QDir dir(appDir);
    const QString leaf = dir.dirName();
    if (leaf.compare(QStringLiteral("dll"), Qt::CaseInsensitive) == 0 ||
        leaf.compare(QStringLiteral("runtime"), Qt::CaseInsensitive) == 0) {
        return dir.absolutePath();
    }
    const QString rootDir = ResolveAppRoot(appDir);
    if (rootDir.isEmpty()) {
        return {};
    }
    const QString dllDir = QDir(rootDir).filePath(QStringLiteral("dll"));
    if (QDir(dllDir).exists()) {
        return dllDir;
    }
    return QDir(rootDir).filePath(QStringLiteral("runtime"));
}

}  // namespace

namespace UiRuntimePaths {

void Prepare(const char *argv0) {
    const QString appDir = ResolveAppDir(argv0);
    if (appDir.isEmpty()) {
        return;
    }
    const QString runtimeDir = ResolveRuntimeDir(appDir);
    if (runtimeDir.isEmpty()) {
        return;
    }
    const QString pluginRoot =
        QDir(runtimeDir + QStringLiteral("/plugins")).exists()
            ? runtimeDir + QStringLiteral("/plugins")
            : runtimeDir;
    const QString platformDir = pluginRoot + QStringLiteral("/platforms");
    const QString qmlDir = runtimeDir + QStringLiteral("/qml");

#ifdef _WIN32
    if (QDir(runtimeDir).exists()) {
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(runtimeDir.utf16()));
    }
#endif

    if (QDir(pluginRoot).exists() && qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", pluginRoot.toUtf8());
    }
    if (QDir(platformDir).exists() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH")) {
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", platformDir.toUtf8());
    }
    if (QDir(qmlDir).exists() && qEnvironmentVariableIsEmpty("QML2_IMPORT_PATH")) {
        qputenv("QML2_IMPORT_PATH", qmlDir.toUtf8());
    }
    if (QDir(qmlDir).exists() && qEnvironmentVariableIsEmpty("QML_IMPORT_PATH")) {
        qputenv("QML_IMPORT_PATH", qmlDir.toUtf8());
    }
}

QString AppRootDir() {
    return ResolveAppRoot(QCoreApplication::applicationDirPath());
}

QString RuntimeDir() {
    return ResolveRuntimeDir(QCoreApplication::applicationDirPath());
}

}  // namespace UiRuntimePaths
