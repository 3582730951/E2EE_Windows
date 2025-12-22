// Dynamic IME plugin loader for optional engines (e.g. librime).
#pragma once

#include <cstddef>
#include <QByteArray>
#include <QString>
#include <QStringList>

class ImePluginLoader {
public:
    static ImePluginLoader &instance();

    bool available() const;
    void *createSession();
    void destroySession(void *session);
    QStringList queryCandidates(void *session, const QString &input, int maxCandidates);

private:
    ImePluginLoader();
    ~ImePluginLoader();
    ImePluginLoader(const ImePluginLoader &) = delete;
    ImePluginLoader &operator=(const ImePluginLoader &) = delete;

    bool ensureLoaded();
    bool ensureInitialized();
    bool ensureRimeData(QString &sharedDir, QString &userDir);
    bool copyResourceIfMissing(const QString &resourcePath, const QString &targetPath);
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

    ApiVersionFn apiVersion_{nullptr};
    InitializeFn initialize_{nullptr};
    ShutdownFn shutdown_{nullptr};
    CreateSessionFn createSession_{nullptr};
    DestroySessionFn destroySession_{nullptr};
    GetCandidatesFn getCandidates_{nullptr};

    QByteArray sharedDirBytes_;
    QByteArray userDirBytes_;
};
