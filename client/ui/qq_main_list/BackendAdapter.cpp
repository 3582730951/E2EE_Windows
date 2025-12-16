#include "BackendAdapter.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>
#include <QDateTime>
#include <QByteArray>
#include <thread>
#include <filesystem>

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

QString GenerateMessageIdHex() {
    QByteArray bytes;
    bytes.resize(16);
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }
    return QString::fromLatin1(bytes.toHex());
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
    if (fileTransferActive_.load()) {
        err = QStringLiteral("文件传输中，请稍后");
        return false;
    }
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
    online_ = true;
    startPolling(basePollIntervalMs_);
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

bool BackendAdapter::sendFriendRequest(const QString &account, const QString &remark, QString &err) {
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
    if (!core_.SendFriendRequest(target.toStdString(), remark.trimmed().toUtf8().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送好友申请失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

QVector<BackendAdapter::FriendRequestEntry> BackendAdapter::listFriendRequests(QString &err) {
    QVector<FriendRequestEntry> out;
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return out;
    }
    if (!ensureInited(err)) {
        return out;
    }
    const auto reqs = core_.ListFriendRequests();
    out.reserve(static_cast<int>(reqs.size()));
    for (const auto &r : reqs) {
        FriendRequestEntry e;
        e.requesterUsername = QString::fromStdString(r.requester_username);
        e.requesterRemark = QString::fromStdString(r.requester_remark);
        out.push_back(std::move(e));
    }
    err.clear();
    return out;
}

bool BackendAdapter::respondFriendRequest(const QString &requester, bool accept, QString &err) {
    const QString u = requester.trimmed();
    if (u.isEmpty()) {
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
    if (!core_.RespondFriendRequest(u.toStdString(), accept)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("处理好友申请失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::deleteFriend(const QString &account, QString &err) {
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
    if (!core_.DeleteFriend(target.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("删除好友失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::setUserBlocked(const QString &account, bool blocked, QString &err) {
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
    if (!core_.SetUserBlocked(target.toStdString(), blocked)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("操作失败") : QString::fromStdString(coreErr);
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

bool BackendAdapter::sendText(const QString &targetId, const QString &text, QString &outMessageId, QString &err) {
    outMessageId.clear();
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

    std::string msgId;
    const bool ok = core_.SendChatText(targetId.toStdString(), text.toUtf8().toStdString(), msgId);
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("后端发送失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = targetId;
            p.messageId = outMessageId;
            p.isGroup = false;
            p.isFile = false;
            p.kind = PendingOutgoing::Kind::Text;
            p.text = text;
            pendingOutgoing_[msgId] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendTextWithReply(const QString &targetId,
                                      const QString &text,
                                      const QString &replyToMessageId,
                                      const QString &replyPreview,
                                      QString &outMessageId,
                                      QString &err) {
    outMessageId.clear();
    if (text.trimmed().isEmpty()) {
        err = QStringLiteral("发送内容为空");
        return false;
    }
    if (replyToMessageId.trimmed().isEmpty()) {
        return sendText(targetId, text, outMessageId, err);
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    std::string msgId;
    const bool ok = core_.SendChatTextWithReply(targetId.toStdString(),
                                               text.toUtf8().toStdString(),
                                               replyToMessageId.trimmed().toStdString(),
                                               replyPreview.toUtf8().toStdString(),
                                               msgId);
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("后端发送失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = targetId;
            p.messageId = outMessageId;
            p.isGroup = false;
            p.isFile = false;
            p.kind = PendingOutgoing::Kind::ReplyText;
            p.text = text;
            p.replyToMessageId = replyToMessageId.trimmed();
            p.replyPreview = replyPreview;
            pendingOutgoing_[msgId] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::resendText(const QString &targetId, const QString &messageId, const QString &text, QString &err) {
    if (messageId.trimmed().isEmpty()) {
        err = QStringLiteral("消息 ID 为空");
        return false;
    }
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

    const std::string key = messageId.trimmed().toStdString();
    bool ok = false;
    auto it = pendingOutgoing_.find(key);
    if (it != pendingOutgoing_.end() && !it->second.isFile && !it->second.isGroup) {
        const PendingOutgoing &p = it->second;
        if (p.kind == PendingOutgoing::Kind::ReplyText) {
            ok = core_.ResendChatTextWithReply(targetId.toStdString(),
                                              key,
                                              p.text.toUtf8().toStdString(),
                                              p.replyToMessageId.trimmed().toStdString(),
                                              p.replyPreview.toUtf8().toStdString());
        } else if (p.kind == PendingOutgoing::Kind::Location) {
            ok = core_.ResendChatLocation(targetId.toStdString(),
                                          key,
                                          static_cast<std::int32_t>(p.latE7),
                                          static_cast<std::int32_t>(p.lonE7),
                                          p.locationLabel.toUtf8().toStdString());
        } else if (p.kind == PendingOutgoing::Kind::ContactCard) {
            ok = core_.ResendChatContactCard(targetId.toStdString(),
                                             key,
                                             p.cardUsername.trimmed().toStdString(),
                                             p.cardDisplay.toUtf8().toStdString());
        }
    }
    if (!ok) {
        ok = core_.ResendChatText(targetId.toStdString(),
                                  key,
                                  text.toUtf8().toStdString());
    }
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("重试失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    pendingOutgoing_.erase(key);
    emit messageResent(targetId, messageId.trimmed());
    err.clear();
    return true;
}

bool BackendAdapter::sendFile(const QString &targetId, const QString &filePath, QString &outMessageId, QString &err) {
    outMessageId.clear();
    if (filePath.trimmed().isEmpty()) {
        err = QStringLiteral("文件路径为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        err = QStringLiteral("文件不存在");
        return false;
    }

    outMessageId = GenerateMessageIdHex();
    startAsyncFileSend(targetId.trimmed(), false, outMessageId, filePath, false);
    err.clear();
    return true;
}

bool BackendAdapter::resendFile(const QString &targetId, const QString &messageId, const QString &filePath, QString &err) {
    if (messageId.trimmed().isEmpty()) {
        err = QStringLiteral("消息 ID 为空");
        return false;
    }
    if (filePath.trimmed().isEmpty()) {
        err = QStringLiteral("文件路径为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        err = QStringLiteral("文件不存在");
        return false;
    }

    startAsyncFileSend(targetId.trimmed(), false, messageId.trimmed(), filePath, true);
    err.clear();
    return true;
}

void BackendAdapter::startAsyncFileSend(const QString &convId, bool isGroup,
                                       const QString &messageId,
                                       const QString &filePath, bool isResend) {
    bool expected = false;
    if (!fileTransferActive_.compare_exchange_strong(expected, true)) {
        emit fileSendFinished(convId, messageId, false, QStringLiteral("已有文件传输在进行"));
        return;
    }

    const QString cid = convId.trimmed();
    const QString mid = messageId.trimmed();
    const QString pathStr = filePath;
    QPointer<BackendAdapter> self(this);

    std::thread([self, cid, isGroup, mid, pathStr, isResend]() {
        if (!self) {
            return;
        }
        bool ok = false;
        std::string coreErr;
        const std::filesystem::path path = std::filesystem::path(pathStr.toStdWString());
        if (isGroup) {
            ok = self->core_.ResendGroupChatFile(cid.toStdString(), mid.toStdString(), path);
        } else {
            ok = self->core_.ResendChatFile(cid.toStdString(), mid.toStdString(), path);
        }
        coreErr = self->core_.last_error();

        const QString err = coreErr.empty() ? QString() : QString::fromStdString(coreErr);
        QMetaObject::invokeMethod(self, [self, cid, isGroup, mid, pathStr, ok, err, isResend]() {
            if (!self) {
                return;
            }
            self->fileTransferActive_.store(false);
            if (ok) {
                self->pendingOutgoing_.erase(mid.toStdString());
                if (isResend) {
                    emit self->messageResent(cid, mid);
                }
                emit self->fileSendFinished(cid, mid, true, err);
                return;
            }

            if (!pathStr.trimmed().isEmpty()) {
                PendingOutgoing p;
                p.convId = cid;
                p.messageId = mid;
                p.isGroup = isGroup;
                p.isFile = true;
                p.filePath = pathStr;
                self->pendingOutgoing_[mid.toStdString()] = std::move(p);
            }
            emit self->fileSendFinished(cid, mid, false,
                                       err.isEmpty() ? QStringLiteral("文件发送失败") : err);
            self->maybeEmitPeerTrustRequired(true);
            self->maybeEmitServerTrustRequired(true);
        }, Qt::QueuedConnection);
    }).detach();
}

void BackendAdapter::startAsyncFileSave(const QString &convId,
                                       const QString &messageId,
                                       const mi::client::ClientCore::ChatFileMessage &file,
                                       const QString &outPath) {
    bool expected = false;
    if (!fileTransferActive_.compare_exchange_strong(expected, true)) {
        emit fileSaveFinished(convId, messageId, false, QStringLiteral("已有文件传输在进行"), outPath);
        return;
    }

    const QString cid = convId.trimmed();
    const QString mid = messageId.trimmed();
    const QString outPathStr = outPath;
    const mi::client::ClientCore::ChatFileMessage fileCopy = file;
    QPointer<BackendAdapter> self(this);

    std::thread([self, cid, mid, fileCopy, outPathStr]() {
        if (!self) {
            return;
        }
        bool ok = false;
        std::string coreErr;
        const std::filesystem::path path = std::filesystem::path(outPathStr.toStdWString());
        ok = self->core_.DownloadChatFileToPath(fileCopy, path, true);
        coreErr = self->core_.last_error();

        const QString err = coreErr.empty() ? QString() : QString::fromStdString(coreErr);
        QMetaObject::invokeMethod(self, [self, cid, mid, outPathStr, ok, err]() {
            if (!self) {
                return;
            }
            self->fileTransferActive_.store(false);
            emit self->fileSaveFinished(cid, mid, ok,
                                       ok ? QString() : (err.isEmpty() ? QStringLiteral("保存失败") : err),
                                       outPathStr);
            if (!ok) {
                self->maybeEmitPeerTrustRequired(true);
                self->maybeEmitServerTrustRequired(true);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

bool BackendAdapter::sendLocation(const QString &targetId,
                                 qint32 latE7,
                                 qint32 lonE7,
                                 const QString &label,
                                 QString &outMessageId,
                                 QString &err) {
    outMessageId.clear();
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (targetId.trimmed().isEmpty()) {
        err = QStringLiteral("账号为空");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    std::string msgId;
    const bool ok = core_.SendChatLocation(targetId.trimmed().toStdString(),
                                          static_cast<std::int32_t>(latE7),
                                          static_cast<std::int32_t>(lonE7),
                                          label.toUtf8().toStdString(),
                                          msgId);
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送位置失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = targetId;
            p.messageId = outMessageId;
            p.isGroup = false;
            p.isFile = false;
            p.kind = PendingOutgoing::Kind::Location;
            p.latE7 = latE7;
            p.lonE7 = lonE7;
            p.locationLabel = label;
            pendingOutgoing_[msgId] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendContactCard(const QString &targetId,
                                     const QString &cardUsername,
                                     const QString &cardDisplay,
                                     QString &outMessageId,
                                     QString &err) {
    outMessageId.clear();
    if (cardUsername.trimmed().isEmpty()) {
        err = QStringLiteral("名片账号为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    std::string msgId;
    const bool ok = core_.SendChatContactCard(targetId.trimmed().toStdString(),
                                             cardUsername.trimmed().toStdString(),
                                             cardDisplay.toUtf8().toStdString(),
                                             msgId);
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送名片失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = targetId;
            p.messageId = outMessageId;
            p.isGroup = false;
            p.isFile = false;
            p.kind = PendingOutgoing::Kind::ContactCard;
            p.cardUsername = cardUsername.trimmed();
            p.cardDisplay = cardDisplay;
            pendingOutgoing_[msgId] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendSticker(const QString &targetId,
                                 const QString &stickerId,
                                 QString &outMessageId,
                                 QString &err) {
    outMessageId.clear();
    if (targetId.trimmed().isEmpty()) {
        err = QStringLiteral("账号为空");
        return false;
    }
    if (stickerId.trimmed().isEmpty()) {
        err = QStringLiteral("贴纸为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    std::string msgId;
    const bool ok = core_.SendChatSticker(targetId.trimmed().toStdString(),
                                         stickerId.trimmed().toStdString(),
                                         msgId);
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送贴纸失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = targetId;
            p.messageId = outMessageId;
            p.isGroup = false;
            p.isFile = false;
            p.kind = PendingOutgoing::Kind::Sticker;
            p.stickerId = stickerId.trimmed();
            pendingOutgoing_[msgId] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::resendSticker(const QString &targetId,
                                   const QString &messageId,
                                   const QString &stickerId,
                                   QString &err) {
    if (targetId.trimmed().isEmpty() || messageId.trimmed().isEmpty() || stickerId.trimmed().isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    const std::string mid = messageId.trimmed().toStdString();
    if (!core_.ResendChatSticker(targetId.trimmed().toStdString(),
                                 mid,
                                 stickerId.trimmed().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("重试发送贴纸失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    pendingOutgoing_.erase(mid);
    err.clear();
    return true;
}

bool BackendAdapter::sendReadReceipt(const QString &targetId, const QString &messageId, QString &err) {
    if (targetId.trimmed().isEmpty() || messageId.trimmed().isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.SendChatReadReceipt(targetId.trimmed().toStdString(),
                                   messageId.trimmed().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送已读回执失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendTyping(const QString &targetId, bool typing, QString &err) {
    if (targetId.trimmed().isEmpty()) {
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
    if (!core_.SendChatTyping(targetId.trimmed().toStdString(), typing)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送输入状态失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendPresence(const QString &targetId, bool online, QString &err) {
    if (targetId.trimmed().isEmpty()) {
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
    if (!core_.SendChatPresence(targetId.trimmed().toStdString(), online)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送在线状态失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(false);
        maybeEmitServerTrustRequired(false);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::saveReceivedFile(const QString &convId, const QString &messageId, const QString &outPath, QString &err) {
    if (convId.trimmed().isEmpty() || messageId.trimmed().isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (outPath.trimmed().isEmpty()) {
        err = QStringLiteral("输出路径为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    const std::string key = convId.toStdString() + "|" + messageId.toStdString();
    const auto it = receivedFiles_.find(key);
    if (it == receivedFiles_.end()) {
        err = QStringLiteral("未找到该文件（可能已过期）");
        return false;
    }

    const QFileInfo fi(outPath);
    if (fi.isDir()) {
        err = QStringLiteral("输出路径是目录");
        return false;
    }

    startAsyncFileSave(convId.trimmed(), messageId.trimmed(), it->second, outPath);
    err.clear();
    return true;
}

bool BackendAdapter::loadReceivedFileBytes(const QString &convId, const QString &messageId, QByteArray &outBytes,
                                           qint64 maxBytes, bool wipeAfterRead, QString &err) {
    outBytes.clear();
    if (convId.trimmed().isEmpty() || messageId.trimmed().isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    const std::string key = convId.toStdString() + "|" + messageId.toStdString();
    const auto it = receivedFiles_.find(key);
    if (it == receivedFiles_.end()) {
        err = QStringLiteral("未找到该文件（可能已过期）");
        return false;
    }
    if (maxBytes > 0 && it->second.file_size > static_cast<std::uint64_t>(maxBytes)) {
        err = QStringLiteral("文件过大，无法预览（%1 MB 上限）")
                  .arg(static_cast<double>(maxBytes) / (1024.0 * 1024.0), 0, 'f', 1);
        return false;
    }

    std::vector<std::uint8_t> plain;
    if (!core_.DownloadChatFileToBytes(it->second, plain, wipeAfterRead)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("下载失败") : QString::fromStdString(coreErr);
        return false;
    }
    if (maxBytes > 0 && plain.size() > static_cast<std::size_t>(maxBytes)) {
        err = QStringLiteral("文件过大，无法预览");
        return false;
    }
    outBytes = QByteArray(reinterpret_cast<const char *>(plain.data()),
                          static_cast<int>(plain.size()));
    err.clear();
    return true;
}

bool BackendAdapter::loadChatHistory(const QString &convId, bool isGroup, int limit,
                                     QVector<HistoryMessageEntry> &outEntries, QString &err) {
    outEntries.clear();
    const QString cid = convId.trimmed();
    if (cid.isEmpty()) {
        err = QStringLiteral("会话 ID 为空");
        return false;
    }
    if (limit < 0) {
        err = QStringLiteral("limit 非法");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    const auto entries = core_.LoadChatHistory(cid.toStdString(), isGroup,
                                               static_cast<std::size_t>(limit));
    const std::string coreErr = core_.last_error();
    if (!coreErr.empty()) {
        err = QString::fromStdString(coreErr);
        return false;
    }

    outEntries.reserve(static_cast<int>(entries.size()));
    for (const auto &e : entries) {
        HistoryMessageEntry h;
        h.outgoing = e.outgoing;
        h.timestampSec = static_cast<quint64>(e.timestamp_sec);
        h.convId = cid;
        h.sender = QString::fromStdString(e.sender);
        h.messageId = QString::fromStdString(e.message_id_hex);

        switch (e.status) {
        case mi::client::ClientCore::HistoryStatus::kSent:
            h.status = 0;
            break;
        case mi::client::ClientCore::HistoryStatus::kDelivered:
            h.status = 1;
            break;
        case mi::client::ClientCore::HistoryStatus::kRead:
            h.status = 2;
            break;
        case mi::client::ClientCore::HistoryStatus::kFailed:
            h.status = 3;
            break;
        }

        switch (e.kind) {
        case mi::client::ClientCore::HistoryKind::kText:
            h.kind = 1;
            h.text = QString::fromUtf8(e.text_utf8.data(), static_cast<int>(e.text_utf8.size()));
            break;
        case mi::client::ClientCore::HistoryKind::kFile: {
            h.kind = 2;
            h.fileName = QString::fromUtf8(e.file_name.data(), static_cast<int>(e.file_name.size()));
            h.fileSize = static_cast<qint64>(e.file_size);
            mi::client::ClientCore::ChatFileMessage f;
            f.from_username = cid.toStdString();
            f.message_id_hex = e.message_id_hex;
            f.file_id = e.file_id;
            f.file_key = e.file_key;
            f.file_name = e.file_name;
            f.file_size = e.file_size;
            const std::string key = cid.toStdString() + "|" + e.message_id_hex;
            receivedFiles_[key] = std::move(f);
            break;
        }
        case mi::client::ClientCore::HistoryKind::kSticker:
            h.kind = 3;
            h.stickerId = QString::fromStdString(e.sticker_id);
            break;
        case mi::client::ClientCore::HistoryKind::kSystem:
            h.kind = 4;
            h.text = QString::fromUtf8(e.text_utf8.data(), static_cast<int>(e.text_utf8.size()));
            break;
        }

        outEntries.push_back(std::move(h));
    }

    err.clear();
    return true;
}

bool BackendAdapter::createGroup(QString &outGroupId, QString &err) {
    outGroupId.clear();
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    std::string groupId;
    if (!core_.CreateGroup(groupId)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("创建群聊失败") : QString::fromStdString(coreErr);
        return false;
    }
    outGroupId = QString::fromStdString(groupId);
    err.clear();
    return true;
}

bool BackendAdapter::joinGroup(const QString &groupId, QString &err) {
    const QString gid = groupId.trimmed();
    if (gid.isEmpty()) {
        err = QStringLiteral("群 ID 为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.JoinGroup(gid.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("加入群聊失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::leaveGroup(const QString &groupId, QString &err) {
    const QString gid = groupId.trimmed();
    if (gid.isEmpty()) {
        err = QStringLiteral("群 ID 为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.LeaveGroup(gid.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("退出群聊失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

QVector<QString> BackendAdapter::listGroupMembers(const QString &groupId, QString &err) {
    QVector<QString> out;
    const QString gid = groupId.trimmed();
    if (gid.isEmpty()) {
        err = QStringLiteral("群 ID 为空");
        return out;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return out;
    }
    if (!ensureInited(err)) {
        return out;
    }
    const auto members = core_.ListGroupMembers(gid.toStdString());
    if (members.empty()) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("获取成员列表失败") : QString::fromStdString(coreErr);
        return out;
    }
    out.reserve(static_cast<int>(members.size()));
    for (const auto &m : members) {
        out.push_back(QString::fromStdString(m));
    }
    err.clear();
    return out;
}

QVector<BackendAdapter::GroupMemberRoleEntry> BackendAdapter::listGroupMembersInfo(const QString &groupId, QString &err) {
    QVector<GroupMemberRoleEntry> out;
    const QString gid = groupId.trimmed();
    if (gid.isEmpty()) {
        err = QStringLiteral("群 ID 为空");
        return out;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return out;
    }
    if (!ensureInited(err)) {
        return out;
    }
    const auto members = core_.ListGroupMembersInfo(gid.toStdString());
    if (members.empty()) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("获取成员信息失败") : QString::fromStdString(coreErr);
        maybeEmitServerTrustRequired(true);
        return out;
    }
    out.reserve(static_cast<int>(members.size()));
    for (const auto &m : members) {
        GroupMemberRoleEntry e;
        e.username = QString::fromStdString(m.username);
        e.role = static_cast<int>(m.role);
        out.push_back(std::move(e));
    }
    err.clear();
    return out;
}

bool BackendAdapter::setGroupMemberRole(const QString &groupId, const QString &member, int role, QString &err) {
    const QString gid = groupId.trimmed();
    const QString who = member.trimmed();
    if (gid.isEmpty() || who.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    mi::client::ClientCore::GroupMemberRole r = mi::client::ClientCore::GroupMemberRole::kMember;
    if (role == 1) {
        r = mi::client::ClientCore::GroupMemberRole::kAdmin;
    } else if (role == 2) {
        r = mi::client::ClientCore::GroupMemberRole::kMember;
    } else {
        err = QStringLiteral("角色无效");
        return false;
    }

    if (!core_.SetGroupMemberRole(gid.toStdString(), who.toStdString(), r)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("设置角色失败") : QString::fromStdString(coreErr);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::kickGroupMember(const QString &groupId, const QString &member, QString &err) {
    const QString gid = groupId.trimmed();
    const QString who = member.trimmed();
    if (gid.isEmpty() || who.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.KickGroupMember(gid.toStdString(), who.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("踢人失败") : QString::fromStdString(coreErr);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendGroupInvite(const QString &groupId, const QString &peer, QString &outMessageId, QString &err) {
    outMessageId.clear();
    const QString gid = groupId.trimmed();
    const QString to = peer.trimmed();
    if (gid.isEmpty() || to.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    std::string mid;
    if (!core_.SendGroupInvite(gid.toStdString(), to.toStdString(), mid)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("邀请失败") : QString::fromStdString(coreErr);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    outMessageId = QString::fromStdString(mid);
    if (!outMessageId.isEmpty()) {
        const std::string id = outMessageId.toStdString();
        groupPendingDeliveries_[id] = gid.toStdString();
        groupPendingOrder_.push_back(id);
        if (groupPendingOrder_.size() > 4096) {
            groupPendingDeliveries_.clear();
            groupPendingOrder_.clear();
        }
    }
    err.clear();
    return true;
}

bool BackendAdapter::sendGroupText(const QString &groupId, const QString &text, QString &outMessageId, QString &err) {
    outMessageId.clear();
    const QString gid = groupId.trimmed();
    const QString t = text;
    if (gid.isEmpty() || t.trimmed().isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    std::string mid;
    if (!core_.SendGroupChatText(gid.toStdString(), t.toUtf8().toStdString(), mid)) {
        outMessageId = QString::fromStdString(mid);
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("发送失败") : QString::fromStdString(coreErr);
        if (!outMessageId.trimmed().isEmpty()) {
            PendingOutgoing p;
            p.convId = gid;
            p.messageId = outMessageId;
            p.isGroup = true;
            p.isFile = false;
            p.text = text;
            pendingOutgoing_[mid] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        if (!outMessageId.isEmpty()) {
            const std::string id = outMessageId.toStdString();
            groupPendingDeliveries_[id] = gid.toStdString();
            groupPendingOrder_.push_back(id);
            if (groupPendingOrder_.size() > 4096) {
                groupPendingDeliveries_.clear();
                groupPendingOrder_.clear();
            }
        }
        return false;
    }
    outMessageId = QString::fromStdString(mid);
    if (!outMessageId.isEmpty()) {
        const std::string id = outMessageId.toStdString();
        groupPendingDeliveries_[id] = gid.toStdString();
        groupPendingOrder_.push_back(id);
        if (groupPendingOrder_.size() > 4096) {
            groupPendingDeliveries_.clear();
            groupPendingOrder_.clear();
        }
    }
    const std::string coreErr = core_.last_error();
    if (!coreErr.empty()) {
        err = QString::fromStdString(coreErr);  // partial failure warning
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return true;
    }
    err.clear();
    return true;
}

bool BackendAdapter::resendGroupText(const QString &groupId, const QString &messageId, const QString &text, QString &err) {
    const QString gid = groupId.trimmed();
    const QString mid = messageId.trimmed();
    if (gid.isEmpty() || mid.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.ResendGroupChatText(gid.toStdString(), mid.toStdString(), text.toUtf8().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("重试失败") : QString::fromStdString(coreErr);
        PendingOutgoing p;
        p.convId = gid;
        p.messageId = mid;
        p.isGroup = true;
        p.isFile = false;
        p.text = text;
        pendingOutgoing_[mid.toStdString()] = std::move(p);
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
    }
    const std::string coreErr = core_.last_error();
    if (!coreErr.empty()) {
        err = QString::fromStdString(coreErr);  // partial failure warning
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        pendingOutgoing_.erase(mid.toStdString());
        emit messageResent(gid, mid);
        return true;
    }
    pendingOutgoing_.erase(mid.toStdString());
    emit messageResent(gid, mid);
    err.clear();
    return true;
}

bool BackendAdapter::sendGroupFile(const QString &groupId, const QString &filePath, QString &outMessageId, QString &err) {
    outMessageId.clear();
    const QString gid = groupId.trimmed();
    const QString path = filePath.trimmed();
    if (gid.isEmpty() || path.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        err = QStringLiteral("文件不存在");
        return false;
    }

    outMessageId = GenerateMessageIdHex();
    if (!outMessageId.isEmpty()) {
        const std::string id = outMessageId.toStdString();
        groupPendingDeliveries_[id] = gid.toStdString();
        groupPendingOrder_.push_back(id);
        if (groupPendingOrder_.size() > 4096) {
            groupPendingDeliveries_.clear();
            groupPendingOrder_.clear();
        }
    }

    startAsyncFileSend(gid, true, outMessageId, filePath, false);
    err.clear();
    return true;
}

bool BackendAdapter::resendGroupFile(const QString &groupId, const QString &messageId, const QString &filePath, QString &err) {
    const QString gid = groupId.trimmed();
    const QString mid = messageId.trimmed();
    const QString path = filePath.trimmed();
    if (gid.isEmpty() || mid.isEmpty() || path.isEmpty()) {
        err = QStringLiteral("参数为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }

    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        err = QStringLiteral("文件不存在");
        return false;
    }

    startAsyncFileSend(gid, true, mid, filePath, true);
    err.clear();
    return true;
}

bool BackendAdapter::trustPendingPeer(const QString &pin, QString &err) {
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.TrustPendingPeer(pin.trimmed().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("信任失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::trustPendingServer(const QString &pin, QString &err) {
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.TrustPendingServer(pin.trimmed().toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("信任失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

QString BackendAdapter::currentDeviceId() const {
    if (fileTransferActive_.load()) {
        return {};
    }
    const std::string id = core_.device_id();
    return QString::fromStdString(id);
}

QVector<BackendAdapter::DeviceEntry> BackendAdapter::listDevices(QString &err) {
    QVector<DeviceEntry> out;
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return out;
    }
    if (!ensureInited(err)) {
        return out;
    }
    const auto devices = core_.ListDevices();
    if (devices.empty()) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("获取设备列表失败") : QString::fromStdString(coreErr);
        return out;
    }
    out.reserve(static_cast<int>(devices.size()));
    for (const auto &d : devices) {
        DeviceEntry e;
        e.deviceId = QString::fromStdString(d.device_id);
        e.lastSeenSec = static_cast<quint32>(d.last_seen_sec);
        out.push_back(std::move(e));
    }
    err.clear();
    return out;
}

bool BackendAdapter::kickDevice(const QString &deviceId, QString &err) {
    const QString target = deviceId.trimmed();
    if (target.isEmpty()) {
        err = QStringLiteral("设备 ID 为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.KickDevice(target.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("踢下线失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::beginDevicePairingPrimary(QString &outPairingCode, QString &err) {
    outPairingCode.clear();
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    std::string code;
    if (!core_.BeginDevicePairingPrimary(code)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("生成配对码失败") : QString::fromStdString(coreErr);
        return false;
    }
    outPairingCode = QString::fromStdString(code);
    err.clear();
    return true;
}

bool BackendAdapter::pollDevicePairingRequests(QVector<DevicePairingRequestEntry> &outRequests,
                                               QString &err) {
    outRequests.clear();
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    const auto requests = core_.PollDevicePairingRequests();
    const std::string coreErr = core_.last_error();
    if (!coreErr.empty() && requests.empty()) {
        err = QString::fromStdString(coreErr);
        return false;
    }
    outRequests.reserve(static_cast<int>(requests.size()));
    for (const auto &r : requests) {
        DevicePairingRequestEntry e;
        e.deviceId = QString::fromStdString(r.device_id);
        e.requestIdHex = QString::fromStdString(r.request_id_hex);
        outRequests.push_back(std::move(e));
    }
    err.clear();
    return true;
}

bool BackendAdapter::approveDevicePairingRequest(const DevicePairingRequestEntry &request, QString &err) {
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    mi::client::ClientCore::DevicePairingRequest r;
    r.device_id = request.deviceId.trimmed().toStdString();
    r.request_id_hex = request.requestIdHex.trimmed().toStdString();
    if (!core_.ApproveDevicePairingRequest(r)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("确认配对失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::beginDevicePairingLinked(const QString &pairingCode, QString &err) {
    const QString code = pairingCode.trimmed();
    if (code.isEmpty()) {
        err = QStringLiteral("配对码为空");
        return false;
    }
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.BeginDevicePairingLinked(code.toStdString())) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("开始配对失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::pollDevicePairingLinked(bool &outCompleted, QString &err) {
    outCompleted = false;
    if (!loggedIn_) {
        err = QStringLiteral("尚未登录");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!core_.PollDevicePairingLinked(outCompleted)) {
        const std::string coreErr = core_.last_error();
        err = coreErr.empty() ? QStringLiteral("配对轮询失败") : QString::fromStdString(coreErr);
        return false;
    }
    err.clear();
    return true;
}

void BackendAdapter::cancelDevicePairing() {
    if (fileTransferActive_.load()) {
        return;
    }
    core_.CancelDevicePairing();
}

void BackendAdapter::startPolling(int intervalMs) {
    basePollIntervalMs_ = intervalMs;
    if (!pollTimer_) {
        pollTimer_ = std::make_unique<QTimer>(this);
        connect(pollTimer_.get(), &QTimer::timeout, this, &BackendAdapter::pollMessages);
    }
    currentPollIntervalMs_ = intervalMs;
    pollTimer_->start(currentPollIntervalMs_);
    updateConnectionState();
}

void BackendAdapter::maybeEmitPeerTrustRequired(bool force) {
    if (!core_.HasPendingPeerTrust()) {
        lastPeerTrustUser_.clear();
        lastPeerTrustFingerprint_.clear();
        return;
    }

    const auto &p = core_.pending_peer_trust();
    const QString peer = QString::fromStdString(p.peer_username);
    const QString fingerprint = QString::fromStdString(p.fingerprint_hex);
    const QString pin = QString::fromStdString(p.pin6);

    if (!force && peer == lastPeerTrustUser_ && fingerprint == lastPeerTrustFingerprint_) {
        return;
    }
    lastPeerTrustUser_ = peer;
    lastPeerTrustFingerprint_ = fingerprint;
    emit peerTrustRequired(peer, fingerprint, pin);
}

void BackendAdapter::maybeEmitServerTrustRequired(bool force) {
    if (!core_.HasPendingServerTrust()) {
        lastServerTrustFingerprint_.clear();
        return;
    }

    const QString fingerprint = QString::fromStdString(core_.pending_server_fingerprint());
    const QString pin = QString::fromStdString(core_.pending_server_pin());
    if (!force && fingerprint == lastServerTrustFingerprint_) {
        return;
    }
    lastServerTrustFingerprint_ = fingerprint;
    emit serverTrustRequired(fingerprint, pin);
}

void BackendAdapter::maybeRetryPendingOutgoing() {
    if (!loggedIn_ || !online_ || pendingOutgoing_.empty()) {
        return;
    }
    QString initErr;
    if (!ensureInited(initErr)) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int sent = 0;
    const int kMaxPerTick = 3;

    for (auto it = pendingOutgoing_.begin();
         it != pendingOutgoing_.end() && sent < kMaxPerTick; ) {
        PendingOutgoing &p = it->second;
        if (p.messageId.trimmed().isEmpty() || p.convId.trimmed().isEmpty()) {
            it = pendingOutgoing_.erase(it);
            continue;
        }

        const int cappedExp = qMin(p.attempts, 5);
        const qint64 waitMs = qMin<qint64>(30000, 1000LL << cappedExp);
        if (p.lastAttemptMs != 0 && now - p.lastAttemptMs < waitMs) {
            ++it;
            continue;
        }

        p.lastAttemptMs = now;
        p.attempts++;

        bool ok = false;
        if (p.isFile) {
            if (p.filePath.trimmed().isEmpty()) {
                ++it;
                continue;
            }

            startAsyncFileSend(p.convId, p.isGroup, p.messageId, p.filePath, true);
            ++it;
            return;
        } else {
            if (p.isGroup) {
                ok = core_.ResendGroupChatText(p.convId.toStdString(),
                                              p.messageId.toStdString(),
                                              p.text.toUtf8().toStdString());
            } else if (p.kind == PendingOutgoing::Kind::ReplyText) {
                ok = core_.ResendChatTextWithReply(p.convId.toStdString(),
                                                  p.messageId.toStdString(),
                                                  p.text.toUtf8().toStdString(),
                                                  p.replyToMessageId.trimmed().toStdString(),
                                                  p.replyPreview.toUtf8().toStdString());
            } else if (p.kind == PendingOutgoing::Kind::Location) {
                ok = core_.ResendChatLocation(p.convId.toStdString(),
                                             p.messageId.toStdString(),
                                             static_cast<std::int32_t>(p.latE7),
                                             static_cast<std::int32_t>(p.lonE7),
                                             p.locationLabel.toUtf8().toStdString());
            } else if (p.kind == PendingOutgoing::Kind::ContactCard) {
                ok = core_.ResendChatContactCard(p.convId.toStdString(),
                                                p.messageId.toStdString(),
                                                p.cardUsername.trimmed().toStdString(),
                                                p.cardDisplay.toUtf8().toStdString());
            } else if (p.kind == PendingOutgoing::Kind::Sticker) {
                ok = core_.ResendChatSticker(p.convId.toStdString(),
                                            p.messageId.toStdString(),
                                            p.stickerId.trimmed().toStdString());
            } else {
                ok = core_.ResendChatText(p.convId.toStdString(),
                                         p.messageId.toStdString(),
                                         p.text.toUtf8().toStdString());
            }
        }

        if (ok) {
            it = pendingOutgoing_.erase(it);
            emit messageResent(p.convId, p.messageId);
            sent++;
            continue;
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        ++it;
        sent++;
    }
}

void BackendAdapter::updateConnectionState() {
    const bool wasOnline = online_;
    QString detail;

    if (!loggedIn_) {
        online_ = false;
        detail = QStringLiteral("未登录");
    } else if (core_.HasPendingServerTrust()) {
        online_ = false;
        detail = QStringLiteral("需信任服务器（TLS）");
    } else if (core_.token().empty()) {
        online_ = false;
        detail = QStringLiteral("会话失效（正在重连）");
    } else if (!core_.is_remote_mode()) {
        online_ = true;
        detail = QStringLiteral("本地模式");
    } else if (core_.remote_ok()) {
        online_ = true;
        detail = QStringLiteral("在线");
    } else {
        online_ = false;
        const QString err = QString::fromStdString(core_.remote_error());
        detail = err.trimmed().isEmpty() ? QStringLiteral("离线") : QStringLiteral("离线：%1").arg(err);
    }

    if (pollTimer_) {
        int nextInterval = basePollIntervalMs_;
        if (!online_ && core_.HasPendingServerTrust()) {
            backoffExp_ = 0;
            nextInterval = qMax(basePollIntervalMs_, 5000);
        } else if (!online_ && loggedIn_ && core_.token().empty()) {
            backoffExp_ = qMin(backoffExp_ + 1, 5);
            nextInterval = qMin(30000, basePollIntervalMs_ * (1 << backoffExp_));
            nextInterval = qMax(nextInterval, 5000);
        } else if (!online_ && loggedIn_ && core_.is_remote_mode()) {
            backoffExp_ = qMin(backoffExp_ + 1, 5);
            nextInterval = qMin(30000, basePollIntervalMs_ * (1 << backoffExp_));
        } else {
            backoffExp_ = 0;
        }

        if (nextInterval != currentPollIntervalMs_) {
            currentPollIntervalMs_ = nextInterval;
            pollTimer_->start(currentPollIntervalMs_);
        }
    }

    if (wasOnline != online_) {
        emit connectionStateChanged(online_, detail);
        if (online_) {
            maybeRetryPendingOutgoing();
        }
        return;
    }

    emit connectionStateChanged(online_, detail);
}

void BackendAdapter::pollMessages() {
    if (!loggedIn_) {
        return;
    }
    QString err;
    if (!ensureInited(err)) {
        return;
    }

    if (core_.token().empty() && !core_.HasPendingServerTrust()) {
        core_.Relogin();
    }

    const auto events = core_.PollChat();
    updateConnectionState();
    for (const auto &t : events.outgoing_texts) {
        emit syncedOutgoingMessage(QString::fromStdString(t.peer_username),
                                   false,
                                   QString(),
                                   QString::fromStdString(t.message_id_hex),
                                   QString::fromUtf8(t.text_utf8.data(), static_cast<int>(t.text_utf8.size())),
                                   false,
                                   0);
    }
    for (const auto &f : events.outgoing_files) {
        mi::client::ClientCore::ChatFileMessage asFile;
        asFile.from_username = f.peer_username;
        asFile.message_id_hex = f.message_id_hex;
        asFile.file_id = f.file_id;
        asFile.file_key = f.file_key;
        asFile.file_name = f.file_name;
        asFile.file_size = f.file_size;
        const std::string k = f.peer_username + "|" + f.message_id_hex;
        receivedFiles_[k] = asFile;
        emit syncedOutgoingMessage(QString::fromStdString(f.peer_username),
                                   false,
                                   QString(),
                                   QString::fromStdString(f.message_id_hex),
                                   QString::fromUtf8(f.file_name.data(), static_cast<int>(f.file_name.size())),
                                    true,
                                    static_cast<qint64>(f.file_size));
    }
    for (const auto &s : events.outgoing_stickers) {
        emit syncedOutgoingSticker(QString::fromStdString(s.peer_username),
                                   QString::fromStdString(s.message_id_hex),
                                   QString::fromStdString(s.sticker_id));
    }
    for (const auto &t : events.outgoing_group_texts) {
        const std::string id = t.message_id_hex;
        groupPendingDeliveries_[id] = t.group_id;
        groupPendingOrder_.push_back(id);
        if (groupPendingOrder_.size() > 4096) {
            groupPendingDeliveries_.clear();
            groupPendingOrder_.clear();
        }
        emit syncedOutgoingMessage(QString::fromStdString(t.group_id),
                                   true,
                                   QString(),
                                   QString::fromStdString(t.message_id_hex),
                                   QString::fromUtf8(t.text_utf8.data(), static_cast<int>(t.text_utf8.size())),
                                   false,
                                   0);
    }
    for (const auto &f : events.outgoing_group_files) {
        mi::client::ClientCore::ChatFileMessage asFile;
        asFile.from_username = f.group_id;
        asFile.message_id_hex = f.message_id_hex;
        asFile.file_id = f.file_id;
        asFile.file_key = f.file_key;
        asFile.file_name = f.file_name;
        asFile.file_size = f.file_size;
        const std::string k = f.group_id + "|" + f.message_id_hex;
        receivedFiles_[k] = asFile;

        const std::string id = f.message_id_hex;
        groupPendingDeliveries_[id] = f.group_id;
        groupPendingOrder_.push_back(id);
        if (groupPendingOrder_.size() > 4096) {
            groupPendingDeliveries_.clear();
            groupPendingOrder_.clear();
        }
        emit syncedOutgoingMessage(QString::fromStdString(f.group_id),
                                   true,
                                   QString(),
                                   QString::fromStdString(f.message_id_hex),
                                   QString::fromUtf8(f.file_name.data(), static_cast<int>(f.file_name.size())),
                                   true,
                                   static_cast<qint64>(f.file_size));
    }
    for (const auto &d : events.deliveries) {
        QString convId = QString::fromStdString(d.from_username);
        const auto it = groupPendingDeliveries_.find(d.message_id_hex);
        if (it != groupPendingDeliveries_.end()) {
            convId = QString::fromStdString(it->second);
        }
        emit delivered(convId, QString::fromStdString(d.message_id_hex));
    }
    for (const auto &r : events.read_receipts) {
        emit read(QString::fromStdString(r.from_username),
                  QString::fromStdString(r.message_id_hex));
    }
    for (const auto &t : events.typing_events) {
        emit typingChanged(QString::fromStdString(t.from_username), t.typing);
    }
    for (const auto &p : events.presence_events) {
        emit presenceChanged(QString::fromStdString(p.from_username), p.online);
    }
    for (const auto &s : events.stickers) {
        emit incomingSticker(QString::fromStdString(s.from_username),
                             QString(),
                             QString::fromStdString(s.message_id_hex),
                             QString::fromStdString(s.sticker_id));
    }
    for (const auto &t : events.texts) {
        emit incomingMessage(QString::fromStdString(t.from_username),
                             false,
                             QString(),
                             QString::fromStdString(t.message_id_hex),
                             QString::fromUtf8(t.text_utf8.data(), static_cast<int>(t.text_utf8.size())),
                             false,
                             0);
    }
    for (const auto &f : events.files) {
        const std::string k = f.from_username + "|" + f.message_id_hex;
        receivedFiles_[k] = f;
        emit incomingMessage(QString::fromStdString(f.from_username),
                             false,
                             QString(),
                             QString::fromStdString(f.message_id_hex),
                             QString::fromUtf8(f.file_name.data(), static_cast<int>(f.file_name.size())),
                             true,
                             static_cast<qint64>(f.file_size));
    }
    for (const auto &t : events.group_texts) {
        emit incomingMessage(QString::fromStdString(t.group_id),
                             true,
                             QString::fromStdString(t.from_username),
                             QString::fromStdString(t.message_id_hex),
                             QString::fromUtf8(t.text_utf8.data(), static_cast<int>(t.text_utf8.size())),
                             false,
                             0);
    }
    for (const auto &f : events.group_files) {
        mi::client::ClientCore::ChatFileMessage asFile;
        asFile.from_username = f.from_username;
        asFile.message_id_hex = f.message_id_hex;
        asFile.file_id = f.file_id;
        asFile.file_key = f.file_key;
        asFile.file_name = f.file_name;
        asFile.file_size = f.file_size;
        const std::string k = f.group_id + "|" + f.message_id_hex;
        receivedFiles_[k] = std::move(asFile);
        emit incomingMessage(QString::fromStdString(f.group_id),
                             true,
                             QString::fromStdString(f.from_username),
                             QString::fromStdString(f.message_id_hex),
                             QString::fromUtf8(f.file_name.data(), static_cast<int>(f.file_name.size())),
                             true,
                             static_cast<qint64>(f.file_size));
    }
    for (const auto &inv : events.group_invites) {
        emit groupInviteReceived(QString::fromStdString(inv.group_id),
                                 QString::fromStdString(inv.from_username),
                                 QString::fromStdString(inv.message_id_hex));
    }
    for (const auto &n : events.group_notices) {
        const QString groupId = QString::fromStdString(n.group_id);
        const QString actor = QString::fromStdString(n.actor_username);
        const QString target = QString::fromStdString(n.target_username);
        QString text;
        switch (n.kind) {
        case 1:
            text = QStringLiteral("%1 加入群聊").arg(target);
            break;
        case 2:
            text = QStringLiteral("%1 退出群聊").arg(target);
            break;
        case 3:
            text = QStringLiteral("%1 将 %2 移出群聊").arg(actor, target);
            break;
        case 4: {
            QString roleText = QStringLiteral("成员");
            if (n.role == mi::client::ClientCore::GroupMemberRole::kOwner) {
                roleText = QStringLiteral("群主");
            } else if (n.role == mi::client::ClientCore::GroupMemberRole::kAdmin) {
                roleText = QStringLiteral("管理员");
            }
            text = QStringLiteral("%1 将 %2 设为 %3").arg(actor, target, roleText);
            break;
        }
        default:
            continue;
        }
        emit groupNoticeReceived(groupId, text);
    }

    {
        const auto reqs = core_.ListFriendRequests();
        std::unordered_set<std::string> current;
        current.reserve(reqs.size());
        for (const auto &r : reqs) {
            current.insert(r.requester_username);
            if (seenFriendRequests_.insert(r.requester_username).second) {
                emit friendRequestReceived(QString::fromStdString(r.requester_username),
                                           QString::fromStdString(r.requester_remark));
            }
        }
        for (auto it = seenFriendRequests_.begin(); it != seenFriendRequests_.end(); ) {
            if (current.count(*it) == 0) {
                it = seenFriendRequests_.erase(it);
            } else {
                ++it;
            }
        }
    }

    maybeEmitPeerTrustRequired(false);
    maybeEmitServerTrustRequired(false);
    if (online_) {
        maybeRetryPendingOutgoing();
    }
}
