// Language pack manager for IME resources (manifest+dict+rules+lm).
#pragma once

#include <QString>
#include <QVector>

class QIODevice;

class ImeLanguagePackManager {
public:
    static ImeLanguagePackManager &instance();

    QString activePackId() const;
    QString preferredSchema() const;
    QString englishDictPath();

    bool applyRimePack(const QString &sharedDir, const QString &userDir);

private:
    ImeLanguagePackManager() = default;

    QString packsRoot() const;
    QString configPath() const;
    QString runtimeRoot() const;

    QString loadActivePackId() const;
    bool writePreferredSchema(const QString &userDir, const QString &schema) const;

    struct PackDictionary {
        QString id;
        QString type;
        QString format;
        QString source;
        QString target;
        bool sourceIsResource{false};
    };

    struct PackRule {
        QString id;
        QString format;
        QString source;
        QString target;
        QString scope;
        bool sourceIsResource{false};
    };

    struct LanguagePack {
        QString id;
        QString name;
        QString version;
        int engineApi{0};
        QString language;
        QString backend;
        QString defaultSchema;
        QString baseDir;
        QString baseResource;
        bool isResource{false};
        QVector<PackDictionary> dictionaries;
        QVector<PackRule> rules;
    };

    bool loadPack(const QString &packId, LanguagePack &pack, QString *error) const;
    bool loadPackFromDir(const QString &dir, LanguagePack &pack, QString *error) const;
    bool loadPackFromResource(const QString &resourceBase, LanguagePack &pack, QString *error) const;
    bool parseManifest(QIODevice &device, LanguagePack &pack, QString *error) const;
    QString resolveSource(const LanguagePack &pack, const QString &path, bool sourceIsResource) const;
    QByteArray readSourceBytes(const LanguagePack &pack, const QString &path, bool sourceIsResource, QString *error) const;
    bool writeFileIfDifferent(const QString &path, const QByteArray &data) const;
    bool applyRimePackInternal(const LanguagePack &pack, const QString &sharedDir, const QString &userDir, QString *error) const;
    bool ensureEnglishDict(const LanguagePack &pack);
    bool convertCustomPhraseDict(const QByteArray &input, QByteArray &output, QString *error) const;

    mutable QString cachedActivePackId_;
    mutable QString cachedPreferredSchema_;
    mutable QString cachedEnglishDictPath_;
};
