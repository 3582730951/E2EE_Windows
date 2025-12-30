#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QStringList>
#include <QTextStream>

#include "third_party/rime_api.h"

#include "../common/ImeLanguagePackManager.h"
#include "../common/UiRuntimePaths.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
QString ResourcePath(const QString &name) {
    return QStringLiteral(":/mi/e2ee/ui/ime/rime/") + name;
}

bool EnsureDir(const QString &path) {
    return QDir().mkpath(path);
}

bool CopyResourceFile(const QString &resourcePath,
                      const QString &targetPath,
                      bool overwrite) {
    if (!overwrite && QFile::exists(targetPath)) {
        return true;
    }
    QFileInfo info(targetPath);
    if (!info.dir().exists()) {
        if (!EnsureDir(info.path())) {
            return false;
        }
    }
    QFile in(resourcePath);
    if (!in.exists() || !in.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = in.readAll();
    if (data.isEmpty()) {
        return false;
    }
    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (out.write(data) != data.size()) {
        return false;
    }
    return true;
}

void CopyOpenccFiles(const QString &srcDir, const QString &dstDir) {
    if (!QDir(srcDir).exists()) {
        return;
    }
    EnsureDir(dstDir);
    const QStringList filters = {
        QStringLiteral("*.json"),
        QStringLiteral("*.ocd2"),
        QStringLiteral("*.txt"),
    };
    const QFileInfoList entries =
        QDir(srcDir).entryInfoList(filters, QDir::Files | QDir::Readable);
    for (const auto &entry : entries) {
        const QString dst = QDir(dstDir).filePath(entry.fileName());
        QFile::remove(dst);
        QFile::copy(entry.filePath(), dst);
    }
}

bool HasBinFiles(const QString &root) {
    if (root.isEmpty() || !QDir(root).exists()) {
        return false;
    }
    QDirIterator it(root, QStringList() << QStringLiteral("*.bin"),
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}

struct RimeLibrary {
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif
    RimeApi *api = nullptr;
};

RimeLibrary LoadRimeApi(const QString &runtimeDir) {
    RimeLibrary lib;
#ifdef _WIN32
    if (!runtimeDir.isEmpty()) {
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(runtimeDir.utf16()));
    }
    const QStringList candidates = {
        QDir(runtimeDir).filePath(QStringLiteral("rime.dll")),
        QDir(runtimeDir).filePath(QStringLiteral("librime.dll")),
        QStringLiteral("rime.dll"),
        QStringLiteral("librime.dll"),
    };
    for (const auto &path : candidates) {
        lib.handle = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (lib.handle) {
            break;
        }
    }
    if (!lib.handle) {
        return lib;
    }
    auto getApi = reinterpret_cast<RimeApi *(*)()>(
        GetProcAddress(lib.handle, "rime_get_api"));
    if (!getApi) {
        FreeLibrary(lib.handle);
        lib.handle = nullptr;
        return lib;
    }
    lib.api = getApi();
#else
    Q_UNUSED(runtimeDir);
    lib.handle = nullptr;
    lib.api = nullptr;
#endif
    return lib;
}

void UnloadRime(RimeLibrary &lib) {
#ifdef _WIN32
    if (lib.handle) {
        FreeLibrary(lib.handle);
    }
#endif
    lib.handle = nullptr;
    lib.api = nullptr;
}

bool PrepareRimeData(const QString &sharedDir, const QString &userDir) {
    if (!EnsureDir(sharedDir) || !EnsureDir(userDir)) {
        return false;
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
        const QString src = ResourcePath(file);
        const QString dst = QDir(sharedDir).filePath(file);
        if (!CopyResourceFile(src, dst, true)) {
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
        const QString src = ResourcePath(file);
        const QString dst = QDir(sharedDir).filePath(file);
        if (!CopyResourceFile(src, dst, false)) {
            return false;
        }
    }
    const QStringList userFiles = {
        QStringLiteral("rime_ice.custom.yaml"),
    };
    for (const auto &file : userFiles) {
        const QString src = ResourcePath(file);
        const QString dst = QDir(userDir).filePath(file);
        if (!CopyResourceFile(src, dst, false)) {
            return false;
        }
    }

    if (!ImeLanguagePackManager::instance().applyRimePack(sharedDir, userDir)) {
        return false;
    }
    return true;
}

bool DeployRime(const QString &sharedDir, const QString &userDir, const QString &runtimeDir) {
    RimeLibrary lib = LoadRimeApi(runtimeDir);
    if (!lib.api) {
        UnloadRime(lib);
        return false;
    }
    QByteArray sharedBytes = QDir::toNativeSeparators(sharedDir).toUtf8();
    QByteArray userBytes = QDir::toNativeSeparators(userDir).toUtf8();
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = sharedBytes.constData();
    traits.user_data_dir = userBytes.constData();
    traits.distribution_name = "mi_e2ee";
    traits.distribution_code_name = "mi_e2ee";
    traits.distribution_version = "1.0";
    traits.app_name = "rime.mi_e2ee.precompile";
    traits.min_log_level = 2;
    traits.log_dir = "";
    lib.api->setup(&traits);
    if (lib.api->deployer_initialize) {
        lib.api->deployer_initialize(&traits);
    }
    lib.api->initialize(&traits);
    bool ok = true;
    bool maintenance = false;
    if (lib.api->start_maintenance) {
        maintenance = lib.api->start_maintenance(False) == True;
    }
    if (lib.api->deploy) {
        ok = lib.api->deploy() == True;
    }
    if (maintenance && lib.api->join_maintenance_thread) {
        lib.api->join_maintenance_thread();
    }
    if (lib.api->finalize) {
        lib.api->finalize();
    }
    UnloadRime(lib);
    return ok;
}

}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    UiRuntimePaths::Prepare(argv[0]);

    QString outputDir;
    QString runtimeDir = qEnvironmentVariable("RIME_RUNTIME_DIR");
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == "--output-dir" && i + 1 < args.size()) {
            outputDir = args.at(++i);
        } else if (arg == "--runtime-dir" && i + 1 < args.size()) {
            runtimeDir = args.at(++i);
        } else if (arg == "--help") {
            QTextStream(stdout)
                << "Usage: mi_rime_precompile --output-dir <dir> "
                   "[--runtime-dir <dir>]\n";
            return 0;
        }
    }
    if (outputDir.isEmpty()) {
        QTextStream(stderr) << "Missing --output-dir\n";
        return 2;
    }
    const QString baseDir = QDir::cleanPath(outputDir);
    const QString sharedDir = QDir(baseDir).filePath(QStringLiteral("share"));
    const QString userDir = QDir(baseDir).filePath(QStringLiteral("user"));
    qputenv("MI_E2EE_IME_DIR", baseDir.toUtf8());

    if (!PrepareRimeData(sharedDir, userDir)) {
        QTextStream(stderr) << "Failed to prepare rime data\n";
        return 3;
    }

    const QStringList openccRoots = {
        QDir(runtimeDir).filePath(QStringLiteral("opencc")),
        QDir(runtimeDir).filePath(QStringLiteral("data/opencc")),
        QDir(runtimeDir).filePath(QStringLiteral("rime/opencc")),
    };
    const QString openccDst = QDir(sharedDir).filePath(QStringLiteral("opencc"));
    for (const auto &dir : openccRoots) {
        CopyOpenccFiles(dir, openccDst);
    }

    if (!DeployRime(sharedDir, userDir, runtimeDir)) {
        QTextStream(stderr) << "Rime deploy failed\n";
        return 4;
    }
    if (!HasBinFiles(userDir)) {
        QTextStream(stderr) << "Rime deploy produced no .bin files\n";
        return 5;
    }
    QTextStream(stdout) << "Rime precompile OK\n";
    return 0;
}
