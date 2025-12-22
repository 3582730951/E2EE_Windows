#include "ImePluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QStandardPaths>

#include "ImePluginApi.h"

namespace {
QString PluginFileName() {
#if defined(Q_OS_WIN)
    return QStringLiteral("mi_ime_rime.dll");
#elif defined(Q_OS_MAC)
    return QStringLiteral("libmi_ime_rime.dylib");
#else
    return QStringLiteral("libmi_ime_rime.so");
#endif
}

QString RimeResourcePath(const QString &name) {
    return QStringLiteral(":/mi/e2ee/ui/ime/rime/") + name;
}
}  // namespace

ImePluginLoader &ImePluginLoader::instance() {
    static ImePluginLoader loader;
    return loader;
}

ImePluginLoader::ImePluginLoader() = default;

ImePluginLoader::~ImePluginLoader() {
    if (initialized_ && shutdown_) {
        shutdown_();
    }
    reset();
}

bool ImePluginLoader::available() const {
    return initialized_;
}

void ImePluginLoader::reset() {
    if (library_) {
        if (library_->isLoaded()) {
            library_->unload();
        }
        delete library_;
        library_ = nullptr;
    }
    initialized_ = false;
    apiVersion_ = nullptr;
    initialize_ = nullptr;
    shutdown_ = nullptr;
    createSession_ = nullptr;
    destroySession_ = nullptr;
    getCandidates_ = nullptr;
    sharedDirBytes_.clear();
    userDirBytes_.clear();
}

bool ImePluginLoader::ensureLoaded() {
    if (initialized_) {
        return true;
    }
    if (loadAttempted_) {
        return false;
    }
    loadAttempted_ = true;
    if (!library_) {
        library_ = new QLibrary();
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString pluginPath = appDir + QLatin1Char('/') + PluginFileName();
    library_->setFileName(pluginPath);
    library_->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!library_->load()) {
        library_->setFileName(QStringLiteral("mi_ime_rime"));
        library_->load();
    }
    if (!library_->isLoaded()) {
        return false;
    }
    apiVersion_ = reinterpret_cast<ApiVersionFn>(library_->resolve("MiImeApiVersion"));
    initialize_ = reinterpret_cast<InitializeFn>(library_->resolve("MiImeInitialize"));
    shutdown_ = reinterpret_cast<ShutdownFn>(library_->resolve("MiImeShutdown"));
    createSession_ = reinterpret_cast<CreateSessionFn>(library_->resolve("MiImeCreateSession"));
    destroySession_ = reinterpret_cast<DestroySessionFn>(library_->resolve("MiImeDestroySession"));
    getCandidates_ = reinterpret_cast<GetCandidatesFn>(library_->resolve("MiImeGetCandidates"));
    if (!apiVersion_ || !initialize_ || !shutdown_ || !createSession_ ||
        !destroySession_ || !getCandidates_) {
        reset();
        return false;
    }
    if (apiVersion_() != kMiImeApiVersion) {
        reset();
        return false;
    }
    return true;
}

bool ImePluginLoader::ensureInitialized() {
    if (initialized_) {
        return true;
    }
    if (!ensureLoaded()) {
        return false;
    }
    QString sharedDir;
    QString userDir;
    if (!ensureRimeData(sharedDir, userDir)) {
        return false;
    }
    sharedDirBytes_ = sharedDir.toUtf8();
    userDirBytes_ = userDir.toUtf8();
    if (!initialize_(sharedDirBytes_.constData(), userDirBytes_.constData())) {
        reset();
        return false;
    }
    initialized_ = true;
    return true;
}

void *ImePluginLoader::createSession() {
    if (!ensureInitialized() || !createSession_) {
        return nullptr;
    }
    return createSession_();
}

void ImePluginLoader::destroySession(void *session) {
    if (session && destroySession_) {
        destroySession_(session);
    }
}

QStringList ImePluginLoader::queryCandidates(void *session,
                                             const QString &input,
                                             int maxCandidates) {
    if (!ensureInitialized() || !session || !getCandidates_ || input.isEmpty()) {
        return {};
    }
    QByteArray inputBytes = input.toUtf8();
    QByteArray buffer;
    buffer.resize(8192);
    buffer.fill(0);
    const int count = getCandidates_(session,
                                     inputBytes.constData(),
                                     buffer.data(),
                                     static_cast<size_t>(buffer.size()),
                                     maxCandidates);
    if (count <= 0) {
        return {};
    }
    const QString payload = QString::fromUtf8(buffer.constData()).trimmed();
    if (payload.isEmpty()) {
        return {};
    }
    QStringList items = payload.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (items.size() > maxCandidates) {
        items = items.mid(0, maxCandidates);
    }
    return items;
}

bool ImePluginLoader::copyResourceIfMissing(const QString &resourcePath,
                                            const QString &targetPath) {
    if (QFile::exists(targetPath)) {
        return true;
    }
    QFile in(resourcePath);
    if (!in.exists()) {
        return false;
    }
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray data = in.readAll();
    if (data.isEmpty()) {
        return false;
    }
    if (out.write(data) != data.size()) {
        return false;
    }
    return true;
}

bool ImePluginLoader::ensureRimeData(QString &sharedDir, QString &userDir) {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.mi_e2ee");
    }
    sharedDir = base + QStringLiteral("/rime/share");
    userDir = base + QStringLiteral("/rime/user");
    if (!QDir().mkpath(sharedDir) || !QDir().mkpath(userDir)) {
        return false;
    }
    const QStringList files = {
        QStringLiteral("default.yaml"),
        QStringLiteral("pinyin.yaml"),
        QStringLiteral("luna_pinyin.dict.yaml"),
        QStringLiteral("mi_pinyin.schema.yaml"),
    };
    for (const auto &file : files) {
        const QString src = RimeResourcePath(file);
        const QString dst = sharedDir + QLatin1Char('/') + file;
        if (!copyResourceIfMissing(src, dst)) {
            return false;
        }
    }
    return true;
}
