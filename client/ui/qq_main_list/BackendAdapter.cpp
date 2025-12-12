#include "BackendAdapter.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>

BackendAdapter::BackendAdapter(QObject *parent) : QObject(parent) {}

namespace {
QString ResolveConfigPath(const QString& name) {
    if (name.isEmpty()) {
        return {};
    }
    const QFileInfo info(name);
    if (info.isAbsolute() && QFile::exists(name)) {
        return name;
    }
    if (QFile::exists(name)) {
        return name;
    }
    const QString candidate = QCoreApplication::applicationDirPath() + QStringLiteral("/") + name;
    if (QFile::exists(candidate)) {
        return candidate;
    }
    return name;
}
}  // namespace

bool BackendAdapter::init(const QString &configPath) {
    if (inited_) {
        if (!configPath.isEmpty() && configPath != configPath_) {
            // 允许在首次之后更新配置路径
            configPath_ = ResolveConfigPath(configPath);
            inited_ = core_.Init(configPath_.toStdString());
        }
        return inited_;
    }
    // 兼容旧版配置文件名：优先 client_config.ini，若不存在则回落 config.ini
    if (!configPath.isEmpty()) {
        configPath_ = ResolveConfigPath(configPath);
    } else if (!ResolveConfigPath(QStringLiteral("client_config.ini")).isEmpty() &&
               QFile::exists(ResolveConfigPath(QStringLiteral("client_config.ini")))) {
        configPath_ = ResolveConfigPath(QStringLiteral("client_config.ini"));
    } else {
        configPath_ = ResolveConfigPath(QStringLiteral("config.ini"));
    }
    inited_ = core_.Init(configPath_.toStdString());
    return inited_;
}

bool BackendAdapter::ensureInited(QString &err) {
    if (!inited_) {
        if (!init(configPath_)) {
            err = QStringLiteral("后端初始化失败（检查 %1）")
                      .arg(configPath_.isEmpty() ? QStringLiteral("config.ini") : configPath_);
            return false;
        }
    }
    return true;
}

bool BackendAdapter::login(const QString &account, const QString &password, QString &err) {
    if (account.trimmed().isEmpty() || password.isEmpty()) {
        err = QStringLiteral("账号或密码为空");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.Login(account.trimmed().toStdString(), password.toStdString())) {
        err = QStringLiteral("登录失败：请检查账号/密码或服务器状态");
        loggedIn_ = false;
        return false;
    }
    loggedIn_ = true;
    currentUser_ = account.trimmed();
    err.clear();
    startPolling();
    return true;
}

QVector<BackendAdapter::FriendEntry> BackendAdapter::listFriends(QString &err) {
    QVector<FriendEntry> out;
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return out;
    }
    if (!ensureInited(err)) {
        return out;
    }
    const auto friends = core_.ListFriends();
    out.reserve(static_cast<int>(friends.size()));
    for (const auto &f : friends) {
        FriendEntry e;
        e.username = QString::fromStdString(f.username);
        e.remark = QString::fromStdString(f.remark);
        out.push_back(std::move(e));
    }
    err.clear();
    return out;
}

bool BackendAdapter::addFriend(const QString &account, const QString &remark, QString &err) {
    const QString target = account.trimmed();
    if (target.isEmpty()) {
        err = QStringLiteral("账号为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.AddFriend(target.toStdString(), remark.trimmed().toStdString())) {
        err = QStringLiteral("添加好友失败：账号不存在或服务器异常");
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::setFriendRemark(const QString &account, const QString &remark, QString &err) {
    const QString target = account.trimmed();
    if (target.isEmpty()) {
        err = QStringLiteral("账号为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.SetFriendRemark(target.toStdString(), remark.trimmed().toStdString())) {
        err = QStringLiteral("备注更新失败：账号不存在或服务器异常");
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendText(const QString &targetId, const QString &text, QString &err) {
    if (text.trimmed().isEmpty()) {
        err = QStringLiteral("发送内容为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    QByteArray payload;
    if (!currentUser_.isEmpty()) {
        payload = QByteArray("CONV:") + currentUser_.toUtf8() + QByteArray("\n");
    }
    payload += text.toUtf8();
    std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
    if (!core_.SendOffline(targetId.toStdString(), bytes)) {
        err = QStringLiteral("后端发送失败");
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendFile(const QString &targetId, const QString &filePath, QString &err) {
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("无法读取文件");
        return false;
    }
    const QByteArray data = file.readAll();
    const QString name = QFileInfo(filePath).fileName();
    QByteArray payload;
    if (!currentUser_.isEmpty()) {
        payload = QByteArray("CONV:") + currentUser_.toUtf8() + QByteArray("\n");
    }
    payload += QByteArray("FILE:") + name.toUtf8() + QByteArray("\n") + data;
    std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
    if (!core_.SendOffline(targetId.toStdString(), bytes)) {
        err = QStringLiteral("发送文件失败");
        return false;
    }
    err.clear();
    return true;
}

void BackendAdapter::startPolling(int intervalMs) {
    if (!pollTimer_) {
        pollTimer_ = std::make_unique<QTimer>(this);
        connect(pollTimer_.get(), &QTimer::timeout, this, &BackendAdapter::pollOffline);
    }
    pollTimer_->start(intervalMs);
}

BackendAdapter::ParsedOffline BackendAdapter::parsePayload(const std::vector<std::uint8_t> &payload) {
    ParsedOffline p;
    QByteArray data(reinterpret_cast<const char *>(payload.data()),
                    static_cast<int>(payload.size()));
    QList<QByteArray> lines = data.split('\n');
    int idx = 0;
    if (!lines.isEmpty() && lines.first().startsWith("CONV:")) {
        p.convId = QString::fromUtf8(lines.first().mid(5)).trimmed();
        idx = 1;
    }
    if (p.convId.isEmpty()) {
        p.convId = QStringLiteral("system");
    }
    QByteArray body;
    for (int i = idx; i < lines.size(); ++i) {
        body.append(lines[i]);
        if (i != lines.size() - 1) {
            body.append('\n');
        }
    }
    if (body.startsWith("FILE:")) {
        p.isFile = true;
        p.text = QStringLiteral("收到文件：%1").arg(QString::fromUtf8(body.mid(5).trimmed()));
    } else {
        p.text = QString::fromUtf8(body);
    }
    return p;
}

void BackendAdapter::pollOffline() {
    if (!loggedIn_) {
        return;
    }
    QString err;
    if (!ensureInited(err)) {
        return;
    }
    auto messages = core_.PullOffline();
    if (messages.empty()) {
        return;
    }
    for (const auto &m : messages) {
        auto parsed = parsePayload(m);
        emit offlineMessage(parsed.convId, parsed.text, parsed.isFile);
    }
}
