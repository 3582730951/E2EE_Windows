#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <cstdint>
#include <memory>
#include <QTimer>

#include "../../include/client_core.h"

class BackendAdapter : public QObject {
    Q_OBJECT

public:
    explicit BackendAdapter(QObject *parent = nullptr);

    bool init(const QString &configPath = QString());
    bool login(const QString &account, const QString &password, QString &err);
    QStringList listFriends(QString &err);
    bool addFriend(const QString &account, QString &err);
    bool sendText(const QString &targetId, const QString &text, QString &err);
    bool sendFile(const QString &targetId, const QString &filePath, QString &err);
    void startPolling(int intervalMs = 2000);

    bool isLoggedIn() const { return loggedIn_; }
    QString currentUser() const { return currentUser_; }

signals:
    void offlineMessage(const QString &convId, const QString &text, bool isFile);

private:
    bool ensureInited(QString &err);
    void pollOffline();
    struct ParsedOffline {
        QString convId;
        QString text;
        bool isFile{false};
    };
    static ParsedOffline parsePayload(const std::vector<std::uint8_t> &payload);

    mi::client::ClientCore core_;
    bool inited_{false};
    bool loggedIn_{false};
    QString currentUser_;
    QString configPath_{"client_config.ini"};
    std::unique_ptr<QTimer> pollTimer_;
};
