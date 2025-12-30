#include "ImePluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QLibrary>
#include <QStandardPaths>
#include <QtGlobal>

#include "ImePluginApi.h"
#include "ImeLanguagePackManager.h"
#include "UiRuntimePaths.h"

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

bool HasBinFiles(const QString &root) {
    if (root.isEmpty() || !QDir(root).exists()) {
        return false;
    }
    QDirIterator it(root, QStringList() << QStringLiteral("*.bin"),
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}

QString ResolveRimePrebuiltDir() {
    const QString env = qEnvironmentVariable("MI_E2EE_RIME_PREBUILT_DIR");
    if (!env.isEmpty()) {
        return env;
    }
    const QString appRoot = UiRuntimePaths::AppRootDir();
    const QString runtimeDir = UiRuntimePaths::RuntimeDir();
    const QStringList candidates = {
        appRoot.isEmpty() ? QString() : QDir(appRoot).filePath(QStringLiteral("database/rime/prebuilt")),
        appRoot.isEmpty() ? QString() : QDir(appRoot).filePath(QStringLiteral("rime/prebuilt")),
        runtimeDir.isEmpty() ? QString() : QDir(runtimeDir).filePath(QStringLiteral("rime/prebuilt")),
    };
    for (const auto &dir : candidates) {
        if (!dir.isEmpty() && QDir(dir).exists()) {
            return dir;
        }
    }
    return {};
}

void CopyPrebuiltUserData(const QString &srcRoot, const QString &dstRoot) {
    if (srcRoot.isEmpty() || dstRoot.isEmpty()) {
        return;
    }
    QDir srcDir(srcRoot);
    if (!srcDir.exists()) {
        return;
    }
    QDir dstDir(dstRoot);
    if (!dstDir.exists()) {
        if (!QDir().mkpath(dstRoot)) {
            return;
        }
    }
    QDirIterator it(srcRoot, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString rel = srcDir.relativeFilePath(info.filePath());
        const QString dstPath = dstDir.filePath(rel);
        if (info.isDir()) {
            QDir().mkpath(dstPath);
            continue;
        }
        if (QFile::exists(dstPath)) {
            continue;
        }
        const QFileInfo dstInfo(dstPath);
        if (!dstInfo.dir().exists()) {
            QDir().mkpath(dstInfo.path());
        }
        QFile::copy(info.filePath(), dstPath);
    }
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
    commitCandidate_ = nullptr;
    clearComposition_ = nullptr;
    getPreedit_ = nullptr;
    sharedDirBytes_.clear();
    userDirBytes_.clear();
    userLock_.reset();
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
    const QString runtimeDir = UiRuntimePaths::RuntimeDir();
    const QStringList candidatePaths = {
        runtimeDir.isEmpty() ? QString() : QDir(runtimeDir).filePath(PluginFileName()),
        appDir.isEmpty() ? QString() : QDir(appDir).filePath(PluginFileName()),
    };
    library_->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    for (const auto &path : candidatePaths) {
        if (path.isEmpty()) {
            continue;
        }
        library_->setFileName(path);
        if (library_->load()) {
            break;
        }
    }
    if (!library_->isLoaded()) {
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
    getPreedit_ = reinterpret_cast<GetPreeditFn>(library_->resolve("MiImeGetPreedit"));
    commitCandidate_ = reinterpret_cast<CommitCandidateFn>(library_->resolve("MiImeCommitCandidate"));
    clearComposition_ = reinterpret_cast<ClearCompositionFn>(library_->resolve("MiImeClearComposition"));
    if (!apiVersion_ || !initialize_ || !shutdown_ || !createSession_ ||
        !destroySession_ || !getCandidates_ || !getPreedit_ || !commitCandidate_ ||
        !clearComposition_) {
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

QString ImePluginLoader::queryPreedit(void *session) {
    if (!ensureInitialized() || !session || !getPreedit_) {
        return {};
    }
    QByteArray buffer;
    buffer.resize(512);
    buffer.fill(0);
    const int count = getPreedit_(session, buffer.data(), static_cast<size_t>(buffer.size()));
    if (count <= 0) {
        return {};
    }
    return QString::fromUtf8(buffer.constData());
}

bool ImePluginLoader::commitCandidate(void *session, int index) {
    if (!ensureInitialized() || !session || !commitCandidate_) {
        return false;
    }
    return commitCandidate_(session, index);
}

void ImePluginLoader::clearComposition(void *session) {
    if (!ensureInitialized() || !session || !clearComposition_) {
        return;
    }
    clearComposition_(session);
}

bool ImePluginLoader::copyResourceIfMissing(const QString &resourcePath,
                                            const QString &targetPath,
                                            bool overwrite) {
    if (!overwrite && QFile::exists(targetPath)) {
        return true;
    }
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists()) {
        if (!QDir().mkpath(targetInfo.path())) {
            return false;
        }
    }
    QFile in(resourcePath);
    if (!in.exists()) {
        return false;
    }
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
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

bool ImePluginLoader::copyFileIfPresent(const QString &sourcePath,
                                        const QString &targetPath,
                                        bool overwrite) {
    if (!overwrite && QFile::exists(targetPath)) {
        return true;
    }
    if (!QFile::exists(sourcePath)) {
        return true;
    }
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists()) {
        if (!QDir().mkpath(targetInfo.path())) {
            return false;
        }
    }
    QFile in(sourcePath);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
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
    QString base = qEnvironmentVariable("MI_E2EE_IME_DIR");
    if (base.isEmpty()) {
        const QString appRoot = UiRuntimePaths::AppRootDir();
        if (!appRoot.isEmpty()) {
            base = QDir(appRoot).filePath(QStringLiteral("database"));
        }
    }
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.mi_e2ee");
    }
    sharedDir = base + QStringLiteral("/rime/share");
    userDir = base + QStringLiteral("/rime/user");
    if (!QDir().mkpath(sharedDir) || !QDir().mkpath(userDir)) {
        return false;
    }
    userLock_.reset(new QLockFile(userDir + QStringLiteral("/.ime.lock")));
    userLock_->setStaleLockTime(0);
    if (!userLock_->tryLock(0)) {
        const QString pidSuffix = QString::number(QCoreApplication::applicationPid());
        const QString altUserDir = userDir + QStringLiteral("_") + pidSuffix;
        if (!QDir().mkpath(altUserDir)) {
            return false;
        }
        userDir = altUserDir;
        userLock_.reset(new QLockFile(userDir + QStringLiteral("/.ime.lock")));
        userLock_->setStaleLockTime(0);
        if (!userLock_->tryLock(0)) {
            return false;
        }
    }
    if (!HasBinFiles(userDir)) {
        const QString prebuiltDir = ResolveRimePrebuiltDir();
        if (!prebuiltDir.isEmpty() && HasBinFiles(prebuiltDir)) {
            CopyPrebuiltUserData(prebuiltDir, userDir);
        }
    }
    const QStringList forcedFiles = {
        QStringLiteral("default.yaml"),
        QStringLiteral("key_bindings.yaml"),
        QStringLiteral("punctuation.yaml"),
        QStringLiteral("symbols.yaml"),
        QStringLiteral("luna_pinyin.schema.yaml"),
        QStringLiteral("stroke.schema.yaml"),
        QStringLiteral("mi_pinyin.schema.yaml"),
        QStringLiteral("rime_ice.schema.yaml"),
        QStringLiteral("melt_eng.schema.yaml"),
        QStringLiteral("radical_pinyin.schema.yaml"),
        QStringLiteral("symbols_v.yaml"),
        QStringLiteral("opencc/emoji.json"),
        QStringLiteral("lua/autocap_filter.lua"),
        QStringLiteral("lua/calc_translator.lua"),
        QStringLiteral("lua/cn_en_spacer.lua"),
        QStringLiteral("lua/corrector.lua"),
        QStringLiteral("lua/date_translator.lua"),
        QStringLiteral("lua/debuger.lua"),
        QStringLiteral("lua/en_spacer.lua"),
        QStringLiteral("lua/force_gc.lua"),
        QStringLiteral("lua/is_in_user_dict.lua"),
        QStringLiteral("lua/long_word_filter.lua"),
        QStringLiteral("lua/lunar.lua"),
        QStringLiteral("lua/number_translator.lua"),
        QStringLiteral("lua/pin_cand_filter.lua"),
        QStringLiteral("lua/reduce_english_filter.lua"),
        QStringLiteral("lua/search.lua"),
        QStringLiteral("lua/select_character.lua"),
        QStringLiteral("lua/t9_preedit.lua"),
        QStringLiteral("lua/unicode.lua"),
        QStringLiteral("lua/uuid.lua"),
        QStringLiteral("lua/v_filter.lua"),
        QStringLiteral("lua/cold_word_drop/drop_words.lua"),
        QStringLiteral("lua/cold_word_drop/filter.lua"),
        QStringLiteral("lua/cold_word_drop/hide_words.lua"),
        QStringLiteral("lua/cold_word_drop/logger.lua"),
        QStringLiteral("lua/cold_word_drop/metatable.lua"),
        QStringLiteral("lua/cold_word_drop/processor.lua"),
        QStringLiteral("lua/cold_word_drop/reduce_freq_words.lua"),
        QStringLiteral("lua/cold_word_drop/string.lua"),
    };
    for (const auto &file : forcedFiles) {
        const QString src = RimeResourcePath(file);
        const QString dst = sharedDir + QLatin1Char('/') + file;
        if (!copyResourceIfMissing(src, dst, true)) {
            return false;
        }
    }
    const QStringList optionalFiles = {
        QStringLiteral("pinyin.yaml"),
        QStringLiteral("luna_pinyin.dict.yaml"),
        QStringLiteral("stroke.dict.yaml"),
        QStringLiteral("rime_ice.dict.yaml"),
        QStringLiteral("cn_dicts/8105.dict.yaml"),
        QStringLiteral("cn_dicts/41448.dict.yaml"),
        QStringLiteral("cn_dicts/base.dict.yaml"),
        QStringLiteral("cn_dicts/ext.dict.yaml"),
        QStringLiteral("cn_dicts/tencent.dict.yaml"),
        QStringLiteral("cn_dicts/others.dict.yaml"),
        QStringLiteral("en_dicts/en.dict.yaml"),
        QStringLiteral("en_dicts/en_ext.dict.yaml"),
        QStringLiteral("melt_eng.dict.yaml"),
        QStringLiteral("radical_pinyin.dict.yaml"),
    };
    for (const auto &file : optionalFiles) {
        const QString src = RimeResourcePath(file);
        const QString dst = sharedDir + QLatin1Char('/') + file;
        if (!copyResourceIfMissing(src, dst)) {
            return false;
        }
    }
    const QStringList userFiles = {
        QStringLiteral("rime_ice.custom.yaml"),
    };
    for (const auto &file : userFiles) {
        const QString src = RimeResourcePath(file);
        const QString dst = userDir + QLatin1Char('/') + file;
        if (!copyResourceIfMissing(src, dst)) {
            return false;
        }
    }
    if (!ImeLanguagePackManager::instance().applyRimePack(sharedDir, userDir)) {
        return false;
    }
    const QString appRoot = UiRuntimePaths::AppRootDir();
    const QString runtimeDir = UiRuntimePaths::RuntimeDir();
    QStringList openccSearchDirs;
    if (!runtimeDir.isEmpty()) {
        openccSearchDirs << (runtimeDir + QStringLiteral("/opencc"))
                         << (runtimeDir + QStringLiteral("/data/opencc"))
                         << (runtimeDir + QStringLiteral("/rime/opencc"));
    }
    if (!appRoot.isEmpty()) {
        openccSearchDirs << (appRoot + QStringLiteral("/opencc"))
                         << (appRoot + QStringLiteral("/data/opencc"))
                         << (appRoot + QStringLiteral("/rime/opencc"))
                         << (appRoot + QStringLiteral("/database/opencc"))
                         << (appRoot + QStringLiteral("/database/data/opencc"))
                         << (appRoot + QStringLiteral("/database/rime/opencc"));
    }
    const QString openccDestDir = sharedDir + QStringLiteral("/opencc");
    QDir().mkpath(openccDestDir);
    const QStringList openccFilters = {
        QStringLiteral("*.json"),
        QStringLiteral("*.ocd2"),
        QStringLiteral("*.txt"),
    };
    for (const auto &dir : openccSearchDirs) {
        QDir source(dir);
        if (!source.exists()) {
            continue;
        }
        const QFileInfoList entries =
            source.entryInfoList(openccFilters, QDir::Files | QDir::Readable);
        for (const auto &entry : entries) {
            const QString fileName = entry.fileName();
            const QString dst = openccDestDir + QLatin1Char('/') + fileName;
            const bool overwrite = (fileName != QStringLiteral("emoji.json"));
            if (!copyFileIfPresent(entry.filePath(), dst, overwrite)) {
                return false;
            }
        }
    }
    return true;
}
