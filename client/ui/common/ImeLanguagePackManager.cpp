// Language pack manager implementation.
#include "ImeLanguagePackManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtGlobal>
#include <QStringConverter>
#include <QTextStream>
#include <QBuffer>

namespace {
QString BaseDataDir() {
    const QString envRoot = qEnvironmentVariable("MI_E2EE_IME_DIR");
    if (!envRoot.isEmpty()) {
        return envRoot;
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.mi_e2ee");
    }
    return base;
}

QString BuiltinPackBase(const QString &packId) {
    return QStringLiteral(":/mi/e2ee/ui/ime/packs/") + packId;
}

bool EnsureDir(const QString &path) {
    return QDir().mkpath(path);
}
}  // namespace

ImeLanguagePackManager &ImeLanguagePackManager::instance() {
    static ImeLanguagePackManager manager;
    return manager;
}

QString ImeLanguagePackManager::packsRoot() const {
    return BaseDataDir() + QStringLiteral("/ime/packs");
}

QString ImeLanguagePackManager::runtimeRoot() const {
    return BaseDataDir() + QStringLiteral("/ime/runtime");
}

QString ImeLanguagePackManager::configPath() const {
    return BaseDataDir() + QStringLiteral("/ime/pack_config.json");
}

QString ImeLanguagePackManager::loadActivePackId() const {
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("zh_cn");
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return QStringLiteral("zh_cn");
    }
    const QJsonObject obj = doc.object();
    const QString packId = obj.value(QStringLiteral("active_pack")).toString();
    return packId.isEmpty() ? QStringLiteral("zh_cn") : packId;
}

QString ImeLanguagePackManager::activePackId() const {
    if (!cachedActivePackId_.isEmpty()) {
        return cachedActivePackId_;
    }
    cachedActivePackId_ = loadActivePackId();
    return cachedActivePackId_;
}

QString ImeLanguagePackManager::preferredSchema() const {
    return cachedPreferredSchema_;
}

bool ImeLanguagePackManager::writePreferredSchema(const QString &userDir,
                                                  const QString &schema) const {
    if (schema.isEmpty()) {
        return false;
    }
    const QString path = userDir + QStringLiteral("/ime_schema.txt");
    return writeFileIfDifferent(path, schema.toUtf8());
}

QString ImeLanguagePackManager::resolveSource(const LanguagePack &pack,
                                              const QString &path,
                                              bool sourceIsResource) const {
    if (path.startsWith(QStringLiteral(":/"))) {
        return path;
    }
    if (sourceIsResource || pack.isResource) {
        return pack.baseResource + QLatin1Char('/') + path;
    }
    return QDir(pack.baseDir).filePath(path);
}

QByteArray ImeLanguagePackManager::readSourceBytes(const LanguagePack &pack,
                                                   const QString &path,
                                                   bool sourceIsResource,
                                                   QString *error) const {
    const QString resolved = resolveSource(pack, path, sourceIsResource);
    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open pack file: %1").arg(resolved);
        }
        return {};
    }
    return file.readAll();
}

bool ImeLanguagePackManager::writeFileIfDifferent(const QString &path,
                                                  const QByteArray &data) const {
    QFileInfo info(path);
    if (!info.dir().exists()) {
        if (!EnsureDir(info.path())) {
            return false;
        }
    }
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        if (existing.readAll() == data) {
            return true;
        }
    }
    existing.close();
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (out.write(data) != data.size()) {
        return false;
    }
    return true;
}

bool ImeLanguagePackManager::parseManifest(QIODevice &device,
                                           LanguagePack &pack,
                                           QString *error) const {
    const QJsonDocument doc = QJsonDocument::fromJson(device.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("manifest.json is not a JSON object");
        }
        return false;
    }
    const QJsonObject obj = doc.object();
    pack.id = obj.value(QStringLiteral("id")).toString();
    pack.name = obj.value(QStringLiteral("name")).toString();
    pack.version = obj.value(QStringLiteral("version")).toString();
    pack.engineApi = obj.value(QStringLiteral("engine_api")).toInt(0);
    pack.language = obj.value(QStringLiteral("language")).toString();
    pack.backend = obj.value(QStringLiteral("backend")).toString();
    pack.defaultSchema = obj.value(QStringLiteral("default_schema")).toString();

    if (pack.id.isEmpty() || pack.backend.isEmpty() || pack.engineApi != 1) {
        if (error) {
            *error = QStringLiteral("manifest.json missing required fields");
        }
        return false;
    }
    if (pack.backend != QStringLiteral("rime") &&
        pack.backend != QStringLiteral("internal")) {
        if (error) {
            *error = QStringLiteral("manifest.json backend unsupported");
        }
        return false;
    }

    const QJsonArray dicts = obj.value(QStringLiteral("dictionaries")).toArray();
    for (const auto &entry : dicts) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject dictObj = entry.toObject();
        PackDictionary dict;
        dict.id = dictObj.value(QStringLiteral("id")).toString();
        dict.type = dictObj.value(QStringLiteral("type")).toString();
        dict.format = dictObj.value(QStringLiteral("format")).toString();
        dict.target = dictObj.value(QStringLiteral("target")).toString();
        if (dictObj.contains(QStringLiteral("resource"))) {
            dict.source = dictObj.value(QStringLiteral("resource")).toString();
            dict.sourceIsResource = true;
        } else {
            dict.source = dictObj.value(QStringLiteral("path")).toString();
        }
        if (dict.source.isEmpty()) {
            continue;
        }
        pack.dictionaries.push_back(dict);
    }

    const QJsonArray rules = obj.value(QStringLiteral("rules")).toArray();
    for (const auto &entry : rules) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject ruleObj = entry.toObject();
        PackRule rule;
        rule.id = ruleObj.value(QStringLiteral("id")).toString();
        rule.format = ruleObj.value(QStringLiteral("format")).toString();
        rule.target = ruleObj.value(QStringLiteral("target")).toString();
        rule.scope = ruleObj.value(QStringLiteral("scope")).toString();
        if (ruleObj.contains(QStringLiteral("resource"))) {
            rule.source = ruleObj.value(QStringLiteral("resource")).toString();
            rule.sourceIsResource = true;
        } else {
            rule.source = ruleObj.value(QStringLiteral("path")).toString();
        }
        if (rule.source.isEmpty()) {
            continue;
        }
        pack.rules.push_back(rule);
    }
    return true;
}

bool ImeLanguagePackManager::loadPackFromDir(const QString &dir,
                                             LanguagePack &pack,
                                             QString *error) const {
    QFile file(QDir(dir).filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("manifest.json not found in %1").arg(dir);
        }
        return false;
    }
    if (!parseManifest(file, pack, error)) {
        return false;
    }
    pack.baseDir = dir;
    pack.isResource = false;
    return true;
}

bool ImeLanguagePackManager::loadPackFromResource(const QString &resourceBase,
                                                  LanguagePack &pack,
                                                  QString *error) const {
    QFile file(resourceBase + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("manifest.json not found in resource %1").arg(resourceBase);
        }
        return false;
    }
    if (!parseManifest(file, pack, error)) {
        return false;
    }
    pack.baseResource = resourceBase;
    pack.isResource = true;
    return true;
}

bool ImeLanguagePackManager::loadPack(const QString &packId,
                                      LanguagePack &pack,
                                      QString *error) const {
    const QString diskDir = QDir(packsRoot()).filePath(packId);
    if (QFileInfo::exists(QDir(diskDir).filePath(QStringLiteral("manifest.json")))) {
        return loadPackFromDir(diskDir, pack, error);
    }
    return loadPackFromResource(BuiltinPackBase(packId), pack, error);
}

bool ImeLanguagePackManager::convertCustomPhraseDict(const QByteArray &input,
                                                     QByteArray &output,
                                                     QString *error) const {
    QBuffer buffer;
    buffer.setData(input);
    if (!buffer.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open custom_phrase buffer");
        }
        return false;
    }
    QTextStream stream(&buffer);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    QStringList lines;
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }
        const QStringList parts = line.split(QChar('\t'));
        if (parts.size() < 2) {
            continue;
        }
        const QString code = parts.at(0).trimmed();
        const QString phrase = parts.at(1).trimmed();
        const QString weight = (parts.size() >= 3) ? parts.at(2).trimmed() : QStringLiteral("1");
        if (code.isEmpty() || phrase.isEmpty()) {
            continue;
        }
        lines.push_back(phrase + QLatin1Char('\t') + code + QLatin1Char('\t') + weight);
    }
    if (lines.isEmpty()) {
        if (error) {
            *error = QStringLiteral("custom_phrase dict produced no entries");
        }
        return false;
    }
    output = lines.join(QLatin1Char('\n')).toUtf8();
    output.append('\n');
    return true;
}

bool ImeLanguagePackManager::applyRimePackInternal(const LanguagePack &pack,
                                                   const QString &sharedDir,
                                                   const QString &userDir,
                                                   QString *error) const {
    for (const auto &rule : pack.rules) {
        if (rule.format != QStringLiteral("rime_patch")) {
            continue;
        }
        const QByteArray data = readSourceBytes(pack, rule.source, rule.sourceIsResource, error);
        if (data.isEmpty()) {
            return false;
        }
        const QString scope = rule.scope.isEmpty() ? QStringLiteral("user") : rule.scope;
        const QString targetName = rule.target.isEmpty() ? QFileInfo(rule.source).fileName()
                                                         : rule.target;
        const QString base = (scope == QStringLiteral("shared")) ? sharedDir : userDir;
        const QString path = base + QLatin1Char('/') + targetName;
        if (scope == QStringLiteral("user") && QFileInfo::exists(path)) {
            continue;
        }
        if (!writeFileIfDifferent(path, data)) {
            if (error) {
                *error = QStringLiteral("Failed to write rule file: %1").arg(path);
            }
            return false;
        }
    }

    for (const auto &dict : pack.dictionaries) {
        if (dict.type == QStringLiteral("custom_phrase")) {
            QByteArray data = readSourceBytes(pack, dict.source, dict.sourceIsResource, error);
            if (data.isEmpty()) {
                return false;
            }
            QByteArray out;
            if (dict.format == QStringLiteral("tsv")) {
                if (!convertCustomPhraseDict(data, out, error)) {
                    return false;
                }
            } else {
                out = data;
            }
            const QString targetName = dict.target.isEmpty() ? QStringLiteral("custom_phrase.txt")
                                                             : dict.target;
            const QString path = userDir + QLatin1Char('/') + targetName;
            if (QFileInfo::exists(path)) {
                continue;
            }
            if (!writeFileIfDifferent(path, out)) {
                if (error) {
                    *error = QStringLiteral("Failed to write custom phrase dict: %1").arg(path);
                }
                return false;
            }
            continue;
        }
        if (dict.type == QStringLiteral("rime_dict") ||
            dict.type == QStringLiteral("rime_schema") ||
            dict.type == QStringLiteral("rime_shared")) {
            const QByteArray data = readSourceBytes(pack, dict.source, dict.sourceIsResource, error);
            if (data.isEmpty()) {
                return false;
            }
            const QString targetName = dict.target.isEmpty() ? QFileInfo(dict.source).fileName()
                                                             : dict.target;
            const QString path = sharedDir + QLatin1Char('/') + targetName;
            if (QFileInfo::exists(path)) {
                continue;
            }
            if (!writeFileIfDifferent(path, data)) {
                if (error) {
                    *error = QStringLiteral("Failed to write shared dict: %1").arg(path);
                }
                return false;
            }
        }
    }
    return true;
}

bool ImeLanguagePackManager::ensureEnglishDict(const LanguagePack &pack) {
    for (const auto &dict : pack.dictionaries) {
        if (dict.type != QStringLiteral("english")) {
            continue;
        }
        QString error;
        const QByteArray data = readSourceBytes(pack, dict.source, dict.sourceIsResource, &error);
        if (data.isEmpty()) {
            continue;
        }
        const QString targetName = dict.target.isEmpty() ? QStringLiteral("english.dict")
                                                         : dict.target;
        const QString path = runtimeRoot() + QLatin1Char('/') + targetName;
        if (!writeFileIfDifferent(path, data)) {
            return false;
        }
        cachedEnglishDictPath_ = path;
        return true;
    }
    return false;
}

QString ImeLanguagePackManager::englishDictPath() {
    if (!cachedEnglishDictPath_.isEmpty() && QFileInfo::exists(cachedEnglishDictPath_)) {
        return cachedEnglishDictPath_;
    }
    LanguagePack active;
    QString error;
    if (loadPack(activePackId(), active, &error)) {
        if (ensureEnglishDict(active)) {
            return cachedEnglishDictPath_;
        }
    }
    LanguagePack fallback;
    if (loadPack(QStringLiteral("en"), fallback, &error)) {
        if (ensureEnglishDict(fallback)) {
            return cachedEnglishDictPath_;
        }
    }
    return {};
}

bool ImeLanguagePackManager::applyRimePack(const QString &sharedDir,
                                           const QString &userDir) {
    LanguagePack pack;
    QString error;
    if (!loadPack(activePackId(), pack, &error) || pack.backend != QStringLiteral("rime")) {
        if (!loadPack(QStringLiteral("zh_cn"), pack, &error)) {
            return false;
        }
    }
    if (!applyRimePackInternal(pack, sharedDir, userDir, &error)) {
        return false;
    }
    cachedPreferredSchema_ = pack.defaultSchema.isEmpty() ? QStringLiteral("rime_ice")
                                                          : pack.defaultSchema;
    if (!writePreferredSchema(userDir, cachedPreferredSchema_)) {
        return false;
    }
    cachedActivePackId_ = pack.id;
    ensureEnglishDict(pack);
    return true;
}
