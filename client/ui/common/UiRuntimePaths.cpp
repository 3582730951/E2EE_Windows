// Runtime path setup for bundled UI assets and plugins.
#include "UiRuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QString>
#include <QtGlobal>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static void InitUiResources() {
    Q_INIT_RESOURCE(ui_resources);
}

namespace {

bool HasUiQmlResource() {
    return QFile::exists(QStringLiteral(":/mi/e2ee/ui/qml/Main.qml"));
}

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
    if (leaf.compare(QStringLiteral("dll"), Qt::CaseInsensitive) == 0) {
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
    const QString runtimeDir = QDir(rootDir).filePath(QStringLiteral("runtime"));
    if (QDir(runtimeDir).exists()) {
        return runtimeDir;
    }
    if (leaf.compare(QStringLiteral("runtime"), Qt::CaseInsensitive) == 0) {
        return dir.absolutePath();
    }
    return dir.absolutePath();
}

void PrependEnvVar(const char *name, const QString &value) {
    if (value.isEmpty()) {
        return;
    }
    const QByteArray valueBytes = QDir::toNativeSeparators(value).toUtf8();
    const QByteArray current = qgetenv(name);
    if (current.contains(valueBytes)) {
        return;
    }
    if (current.isEmpty()) {
        qputenv(name, valueBytes);
        return;
    }
    QByteArray updated = valueBytes;
    updated.append(';');
    updated.append(current);
    qputenv(name, updated);
}

void TryRegisterUiResourceFromDisk(const QString &appDir) {
    if (HasUiQmlResource()) {
        return;
    }
    const QString runtimeDir = ResolveRuntimeDir(appDir);
    if (runtimeDir.isEmpty()) {
        return;
    }
    const QString rccPath = QDir(runtimeDir).filePath(QStringLiteral("ui_resources.rcc"));
    if (!QFile::exists(rccPath)) {
        return;
    }
    QResource::registerResource(rccPath);
}

}  // namespace

namespace UiRuntimePaths {

void Prepare(const char *argv0) {
    InitUiResources();
    const QString appDir = ResolveAppDir(argv0);
    if (appDir.isEmpty()) {
        return;
    }
    TryRegisterUiResourceFromDisk(appDir);
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

    if (QDir(pluginRoot).exists()) {
        PrependEnvVar("QT_PLUGIN_PATH", pluginRoot);
    }
    if (QDir(platformDir).exists()) {
        PrependEnvVar("QT_QPA_PLATFORM_PLUGIN_PATH", platformDir);
    }
    if (QDir(qmlDir).exists()) {
        PrependEnvVar("QML2_IMPORT_PATH", qmlDir);
        PrependEnvVar("QML_IMPORT_PATH", qmlDir);
    }
}

QString AppRootDir() {
    return ResolveAppRoot(QCoreApplication::applicationDirPath());
}

QString RuntimeDir() {
    return ResolveRuntimeDir(QCoreApplication::applicationDirPath());
}

}  // namespace UiRuntimePaths
