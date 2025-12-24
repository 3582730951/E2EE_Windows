// Dynamic IME plugin loader for optional engines (e.g. librime).
#pragma once

#include <cstddef>
#include <memory>
#include <QByteArray>
#include <QString>
#include <QStringList>

class QLockFile;

class ImePluginLoader {
public:
    static ImePluginLoader &instance();

    bool available() const;
    void *createSession();
    void destroySession(void *session);
    QStringList queryCandidates(void *session, const QString &input, int maxCandidates);
    QString queryPreedit(void *session);
    bool commitCandidate(void *session, int index);
    void clearComposition(void *session);

private:
    ImePluginLoader();
    ~ImePluginLoader();
    ImePluginLoader(const ImePluginLoader &) = delete;
    ImePluginLoader &operator=(const ImePluginLoader &) = delete;

    bool ensureLoaded();
    bool ensureInitialized();
    bool ensureRimeData(QString &sharedDir, QString &userDir);
    bool copyResourceIfMissing(const QString &resourcePath, const QString &targetPath, bool overwrite = false);
    bool copyFileIfPresent(const QString &sourcePath, const QString &targetPath, bool overwrite = false);
    void reset();

    class QLibrary *library_{nullptr};
    bool loadAttempted_{false};
    bool initialized_{false};

    using ApiVersionFn = int (*)();
    using InitializeFn = bool (*)(const char *, const char *);
    using ShutdownFn = void (*)();
    using CreateSessionFn = void *(*)();
    using DestroySessionFn = void (*)(void *);
    using GetCandidatesFn = int (*)(void *, const char *, char *, size_t, int);
    using GetPreeditFn = int (*)(void *, char *, size_t);
    using CommitCandidateFn = bool (*)(void *, int);
    using ClearCompositionFn = void (*)(void *);

    ApiVersionFn apiVersion_{nullptr};
    InitializeFn initialize_{nullptr};
    ShutdownFn shutdown_{nullptr};
    CreateSessionFn createSession_{nullptr};
    DestroySessionFn destroySession_{nullptr};
    GetCandidatesFn getCandidates_{nullptr};
    GetPreeditFn getPreedit_{nullptr};
    CommitCandidateFn commitCandidate_{nullptr};
    ClearCompositionFn clearComposition_{nullptr};

    QByteArray sharedDirBytes_;
    QByteArray userDirBytes_;
    std::unique_ptr<QLockFile> userLock_;
};
