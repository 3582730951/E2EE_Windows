#include "BackendAdapter.h"
#include "TrustPromptDialog.h"
#include "CallInviteUtils.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QEventLoop>
#include <QSettings>
#include <QMessageBox>
#include <QMutexLocker>
#include <QPointer>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>
#include <QRunnable>
#include <QUrl>
#include <QDateTime>
#include <QByteArray>
#include <QProcess>
#include <algorithm>
#include <thread>
#include <cstring>
#include <filesystem>
#if defined(MI_UI_HAS_QT_MULTIMEDIA)
#include <QMediaPlayer>
#include <QVideoFrame>
#include <QVideoSink>
#endif

#include "c_api_client.h"
#include "cpp_client_adapter.h"
#include "../common/UiSettings.h"
#include "../common/UiRuntimePaths.h"
#include "key_transparency.h"
#include "platform_time.h"

BackendAdapter::BackendAdapter(QObject *parent) : QObject(parent) {
    core_pool_.setMaxThreadCount(1);
    core_pool_.setExpiryTimeout(30000);
}

BackendAdapter::~BackendAdapter() {
    core_pool_.clear();
    core_pool_.waitForDone();
    if (c_api_) {
        mi_client_destroy(c_api_);
        c_api_ = nullptr;
    }
}

namespace {
QString ResolveConfigPath(const QString& name) {
    if (name.isEmpty()) {
        return {};
    }
    const QFileInfo info(name);
    const QString appRoot = UiRuntimePaths::AppRootDir();
    const QString baseDir = appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
    const QString configDir = baseDir + QStringLiteral("/config");
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    if (info.path() != QStringLiteral(".") && !info.path().isEmpty()) {
        const QString candidate = baseDir + QStringLiteral("/") + name;
        if (QFile::exists(candidate)) {
            return candidate;
        }
        if (QFile::exists(name)) {
            return QFileInfo(name).absoluteFilePath();
        }
        return candidate;
    }
    const QString configCandidate = configDir + QStringLiteral("/") + name;
    if (QFile::exists(configCandidate)) {
        return configCandidate;
    }
    const QString appCandidate = baseDir + QStringLiteral("/") + name;
    if (QFile::exists(appCandidate)) {
        return appCandidate;
    }
    if (QFile::exists(name)) {
        return QFileInfo(name).absoluteFilePath();
    }
    return configCandidate;
}

QString GenerateMessageIdHex() {
    QByteArray bytes;
    bytes.resize(16);
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }
    return QString::fromLatin1(bytes.toHex());
}

constexpr int kPreviewMaxBytes = 240 * 1024;
constexpr int kPreviewMaxDim = 256;
constexpr int kPreviewMinDim = 64;
constexpr std::uint32_t kMaxFriendEntries = 512;
constexpr std::uint32_t kMaxFriendRequestEntries = 256;
constexpr std::uint32_t kMaxDeviceEntries = 128;
constexpr std::uint32_t kMaxGroupMemberEntries = 256;
constexpr std::uint32_t kMaxDevicePairingRequests = 64;

bool IsImageExtension(const QString &suffix) {
    static const QStringList kImageExt = {
        QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"),  QStringLiteral("gif"), QStringLiteral("webp"),
        QStringLiteral("ico"),  QStringLiteral("heic")
    };
    return kImageExt.contains(suffix, Qt::CaseInsensitive);
}

bool IsVideoExtension(const QString &suffix) {
    static const QStringList kVideoExt = {
        QStringLiteral("mp4"),  QStringLiteral("mov"), QStringLiteral("mkv"),
        QStringLiteral("webm"), QStringLiteral("avi"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("m4v"), QStringLiteral("3gp")
    };
    return kVideoExt.contains(suffix, Qt::CaseInsensitive);
}

bool EncodePreviewImage(const QImage &source, QByteArray &outBytes) {
    outBytes.clear();
    if (source.isNull()) {
        return false;
    }
    int dim = kPreviewMaxDim;
    for (int attempt = 0; attempt < 4; ++attempt) {
        QImage scaled = source;
        if (scaled.width() > dim || scaled.height() > dim) {
            scaled = scaled.scaled(dim, dim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        bool ok = false;
        if (scaled.hasAlphaChannel()) {
            ok = scaled.save(&buffer, "PNG");
            if ((!ok || encoded.size() > kPreviewMaxBytes) && !scaled.isNull()) {
                encoded.clear();
                QBuffer jpegBuffer(&encoded);
                jpegBuffer.open(QIODevice::WriteOnly);
                const QImage rgb = scaled.convertToFormat(QImage::Format_RGB32);
                ok = rgb.save(&jpegBuffer, "JPG", 70);
            }
        } else {
            ok = scaled.save(&buffer, "JPG", 80);
        }
        if (ok && !encoded.isEmpty() && encoded.size() <= kPreviewMaxBytes) {
            outBytes = encoded;
            return true;
        }
        dim = std::max(kPreviewMinDim, dim * 3 / 4);
    }
    return false;
}

bool BuildImagePreviewBytes(const QString &filePath, QByteArray &outBytes) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) {
        const int maxDim = std::max(kPreviewMinDim, kPreviewMaxDim);
        const QSize target = size.scaled(maxDim, maxDim, Qt::KeepAspectRatio);
        if (target.isValid()) {
            reader.setScaledSize(target);
        }
    }
    const QImage image = reader.read();
    return EncodePreviewImage(image, outBytes);
}

#if defined(MI_UI_HAS_QT_MULTIMEDIA)
bool ExtractVideoFrame(const QString &filePath, QImage &outImage) {
    outImage = QImage();
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoOutput(&sink);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop,
                     [&](const QVideoFrame &frame) {
                         if (!frame.isValid()) {
                             return;
                         }
                         const QImage image = frame.toImage();
                         if (image.isNull()) {
                             return;
                         }
                         outImage = image;
                         loop.quit();
                     });
    player.setSource(QUrl::fromLocalFile(filePath));
    player.play();
    timer.start(600);
    loop.exec();
    player.stop();
    return !outImage.isNull();
}

bool BuildVideoPreviewBytes(const QString &filePath, QByteArray &outBytes) {
    QImage frame;
    if (!ExtractVideoFrame(filePath, frame)) {
        return false;
    }
    return EncodePreviewImage(frame, outBytes);
}
#else
bool BuildVideoPreviewBytes(const QString &, QByteArray &) {
    return false;
}
#endif

bool BuildRawPreviewBytes(const QString &filePath, QByteArray &outBytes) {
    outBytes.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    outBytes = file.read(static_cast<qint64>(kPreviewMaxBytes));
    return !outBytes.isEmpty();
}

bool BuildAttachmentPreviewBytes(const QString &filePath, QByteArray &outBytes) {
    outBytes.clear();
    const QFileInfo info(filePath);
    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix.isEmpty()) {
        return BuildRawPreviewBytes(filePath, outBytes);
    }
    if (IsImageExtension(suffix)) {
        if (BuildImagePreviewBytes(filePath, outBytes)) {
            return true;
        }
    }
    if (IsVideoExtension(suffix)) {
        if (BuildVideoPreviewBytes(filePath, outBytes)) {
            return true;
        }
    }
    return BuildRawPreviewBytes(filePath, outBytes);
}

struct ServerEndpoint {
    QString host;
    quint16 port{0};
};

ServerEndpoint ReadClientEndpoint(const QString &configPath) {
    ServerEndpoint out;
    const QString path = configPath.trimmed();
    if (path.isEmpty()) {
        return out;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }
        if (line.startsWith('[')) {
            continue;
        }
        int comment = line.indexOf('#');
        if (comment >= 0) {
            line = line.left(comment).trimmed();
        }
        comment = line.indexOf(';');
        if (comment >= 0) {
            line = line.left(comment).trimmed();
        }
        const int eq = line.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (key == QStringLiteral("server_ip")) {
            out.host = val;
        } else if (key == QStringLiteral("server_port")) {
            bool ok = false;
            const uint p = val.toUInt(&ok);
            if (ok && p <= 65535) {
                out.port = static_cast<quint16>(p);
            }
        }
    }
    return out;
}

bool IsLoopbackHost(const QString &host) {
    const QString h = host.trimmed().toLower();
    return h == QStringLiteral("127.0.0.1") || h == QStringLiteral("localhost") || h == QStringLiteral("::1");
}

QString FindBundledServerExe() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/mi_e2ee_server.exe"),
        appDir + QStringLiteral("/../s/mi_e2ee_server.exe"),
        appDir + QStringLiteral("/../server/mi_e2ee_server.exe"),
        appDir + QStringLiteral("/../mi_e2ee_server.exe"),
    };
    for (const auto &p : candidates) {
        const QString cleaned = QFileInfo(p).absoluteFilePath();
        if (QFile::exists(cleaned)) {
            return cleaned;
        }
    }
    return {};
}

QString GroupHex4(const QString &hex) {
    if (hex.isEmpty()) {
        return {};
    }
    QString out;
    out.reserve(hex.size() + (hex.size() / 4));
    for (int i = 0; i < hex.size(); ++i) {
        if (i != 0 && (i % 4) == 0) {
            out.append('-');
        }
        out.append(hex.at(i));
    }
    return out;
}

QString KtRootFingerprintHex(const QString &path, QString &err) {
    err.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("kt root pubkey not found");
        return {};
    }
    const QByteArray bytes = f.readAll();
    if (bytes.isEmpty()) {
        err = QStringLiteral("kt root pubkey empty");
        return {};
    }
    if (bytes.size() != static_cast<int>(mi::server::kKtSthSigPublicKeyBytes)) {
        err = QStringLiteral("kt root pubkey size invalid");
        return {};
    }
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

QString KtRootSasHex(const QString &fingerprintHex) {
    const QByteArray fp = QByteArray::fromHex(fingerprintHex.toLatin1());
    if (fp.size() != 32) {
        return {};
    }
    QByteArray msg("MI_KT_ROOT_SAS_V1");
    msg.append(fp);
    const QByteArray digest = QCryptographicHash::hash(msg, QCryptographicHash::Sha256);
    return GroupHex4(QString::fromLatin1(digest.toHex().left(20)));
}

bool IsKtRootError(const QString &err) {
    const QString e = err.trimmed().toLower();
    return e.startsWith(QStringLiteral("kt root pubkey"));
}

bool WriteKtRootPath(const QString &configPath, const QString &keyPath, QString &err) {
    err.clear();
    const QFileInfo cfgInfo(configPath);
    const QDir cfgDir(cfgInfo.absolutePath());
    if (!cfgInfo.absolutePath().isEmpty()) {
        QDir().mkpath(cfgInfo.absolutePath());
    }
    QString storePath = keyPath;
    if (!cfgInfo.absolutePath().isEmpty()) {
        storePath = cfgDir.relativeFilePath(QFileInfo(keyPath).absoluteFilePath());
    }
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("kt"));
    settings.setValue(QStringLiteral("require_signature"), 1);
    settings.setValue(QStringLiteral("root_pubkey_path"), storePath);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        err = QStringLiteral("write client_config failed");
        return false;
    }
    return true;
}

QString AugmentTransportErrorHint(const QString &coreErr) {
    const QString e = coreErr.trimmed();
    if (e == QStringLiteral("tcp recv failed") ||
        e == QStringLiteral("tcp request failed") ||
        e == QStringLiteral("tcp send failed")) {
        return e + UiSettings::Tr(
                       QStringLiteral("（可能 TLS 配置不一致：服务端启用 TLS 时，请在 config/client_config.ini 设置 use_tls=1）"),
                       QStringLiteral(" (possible TLS mismatch: if the server uses TLS, set use_tls=1 in config/client_config.ini)"));
    }
    if (e == QStringLiteral("tls recv failed") ||
        e == QStringLiteral("tls request failed") ||
        e == QStringLiteral("tls handshake failed") ||
        e == QStringLiteral("tls connect failed")) {
        return e + UiSettings::Tr(
                       QStringLiteral("（可能 TLS 配置不一致：若服务端未启用 TLS，可在 config/client_config.ini 设置 use_tls=0）"),
                       QStringLiteral(" (possible TLS mismatch: if the server does not use TLS, set use_tls=0 in config/client_config.ini)"));
    }
    if (e.contains(QStringLiteral("mysql provider not built"), Qt::CaseInsensitive)) {
        if (e.contains(QStringLiteral("-DMI_E2EE_ENABLE_MYSQL"), Qt::CaseInsensitive) ||
            e.contains(QStringLiteral("set [mode] mode=1"), Qt::CaseInsensitive) ||
            e.contains(QStringLiteral("mode=1"), Qt::CaseInsensitive)) {
            return e;
        }
        return e + UiSettings::Tr(
                       QStringLiteral("（服务端未编译 MySQL：请用 -DMI_E2EE_ENABLE_MYSQL=ON 重新构建服务端，或将服务端 config.ini 的 [mode] mode=1 使用 test_user.txt）"),
                       QStringLiteral(" (MySQL not enabled on the server: rebuild with -DMI_E2EE_ENABLE_MYSQL=ON, or set [mode] mode=1 to use test_user.txt)"));
    }
    if (e == QStringLiteral("pinned fingerprint required")) {
        return e + UiSettings::Tr(
                       QStringLiteral("（需预置服务器指纹：在 config/client_config.ini 填写 pinned_fingerprint）"),
                       QStringLiteral(" (Preloaded server pin required: set pinned_fingerprint in config/client_config.ini)"));
    }
    if (e == QStringLiteral("server fingerprint mismatch")) {
        return e + UiSettings::Tr(
                       QStringLiteral("（指纹不匹配：请通过可信渠道更新 config/client_config.ini 的 pinned_fingerprint）"),
                       QStringLiteral(" (Fingerprint mismatch: update pinned_fingerprint in config/client_config.ini after out-of-band verification)"));
    }
    return e;
}

bool IsNonRetryableSendError(const QString &coreErr) {
    const QString e = coreErr.trimmed().toLower();
    if (e.isEmpty()) {
        return false;
    }
    if (e.contains(QStringLiteral("not friends"))) {
        return true;
    }
    if (e.contains(QStringLiteral("recipient not found")) ||
        e.contains(QStringLiteral("invalid recipient")) ||
        e.contains(QStringLiteral("recipient empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("payload too large")) ||
        e.contains(QStringLiteral("payload empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("peer empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("not in group"))) {
        return true;
    }
    return false;
}

QVector<BackendAdapter::FriendEntry> ToFriendEntries(
    const std::vector<mi::sdk::FriendEntry> &friends) {
    QVector<BackendAdapter::FriendEntry> out;
    out.reserve(static_cast<int>(friends.size()));
    for (const auto &f : friends) {
        BackendAdapter::FriendEntry e;
        e.username = QString::fromStdString(f.username);
        e.remark = QString::fromStdString(f.remark);
        out.push_back(std::move(e));
    }
    return out;
}

QVector<BackendAdapter::FriendEntry> ToFriendEntries(
    const mi_friend_entry_t* entries,
    std::uint32_t count) {
    QVector<BackendAdapter::FriendEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        BackendAdapter::FriendEntry e;
        if (entries[i].username) {
            e.username = QString::fromUtf8(entries[i].username);
        }
        if (entries[i].remark) {
            e.remark = QString::fromUtf8(entries[i].remark);
        }
        out.push_back(std::move(e));
    }
    return out;
}

QVector<BackendAdapter::FriendRequestEntry> ToFriendRequestEntries(
    const mi_friend_request_entry_t* entries,
    std::uint32_t count) {
    QVector<BackendAdapter::FriendRequestEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        BackendAdapter::FriendRequestEntry e;
        if (entries[i].requester_username) {
            e.requesterUsername = QString::fromUtf8(entries[i].requester_username);
        }
        if (entries[i].requester_remark) {
            e.requesterRemark = QString::fromUtf8(entries[i].requester_remark);
        }
        out.push_back(std::move(e));
    }
    return out;
}

QVector<BackendAdapter::GroupMemberRoleEntry> ToGroupMemberRoleEntries(
    const mi_group_member_entry_t* entries,
    std::uint32_t count) {
    QVector<BackendAdapter::GroupMemberRoleEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        BackendAdapter::GroupMemberRoleEntry e;
        if (entries[i].username) {
            e.username = QString::fromUtf8(entries[i].username);
        }
        e.role = static_cast<int>(entries[i].role);
        out.push_back(std::move(e));
    }
    return out;
}

QVector<QString> ToGroupMemberNames(const mi_group_member_entry_t* entries,
                                    std::uint32_t count) {
    QVector<QString> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        if (entries[i].username) {
            out.push_back(QString::fromUtf8(entries[i].username));
        }
    }
    return out;
}

QVector<BackendAdapter::DevicePairingRequestEntry> ToDevicePairingRequests(
    const mi_device_pairing_request_t* entries,
    std::uint32_t count) {
    QVector<BackendAdapter::DevicePairingRequestEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        BackendAdapter::DevicePairingRequestEntry e;
        if (entries[i].device_id) {
            e.deviceId = QString::fromUtf8(entries[i].device_id);
        }
        if (entries[i].request_id_hex) {
            e.requestIdHex = QString::fromUtf8(entries[i].request_id_hex);
        }
        out.push_back(std::move(e));
    }
    return out;
}

QVector<BackendAdapter::DeviceEntry> ToDeviceEntries(
    const mi_device_entry_t* entries,
    std::uint32_t count) {
    QVector<BackendAdapter::DeviceEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        BackendAdapter::DeviceEntry e;
        if (entries[i].device_id) {
            e.deviceId = QString::fromUtf8(entries[i].device_id);
        }
        e.lastSeenSec = static_cast<quint32>(entries[i].last_seen_sec);
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<mi::sdk::FriendEntry> ToFriendVector(
    const mi_friend_entry_t* entries,
    std::uint32_t count) {
    std::vector<mi::sdk::FriendEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        mi::sdk::FriendEntry e;
        if (entries[i].username) {
            e.username = entries[i].username;
        }
        if (entries[i].remark) {
            e.remark = entries[i].remark;
        }
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<mi::sdk::FriendRequestEntry> ToFriendRequestVector(
    const mi_friend_request_entry_t* entries,
    std::uint32_t count) {
    std::vector<mi::sdk::FriendRequestEntry> out;
    if (!entries || count == 0) {
        return out;
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        mi::sdk::FriendRequestEntry e;
        if (entries[i].requester_username) {
            e.requester_username = entries[i].requester_username;
        }
        if (entries[i].requester_remark) {
            e.requester_remark = entries[i].requester_remark;
        }
        out.push_back(std::move(e));
    }
    return out;
}

}  // namespace

bool BackendAdapter::init(const QString &configPath) {
    if (inited_) {
        if (!configPath.isEmpty() && configPath != configPath_) {
            // 允许在首次之后更新配置路径
            configPath_ = ResolveConfigPath(configPath);
            loadDeviceSyncSettings();
            if (c_api_) {
                mi_client_destroy(c_api_);
                c_api_ = nullptr;
            }
            c_api_ = mi_client_create(configPath_.toStdString().c_str());
            inited_ = c_api_ != nullptr;
        }
        return inited_;
    }
    // 兼容旧版配置文件名：优先 config/client_config.ini，若不存在则回落旧名
    if (!configPath.isEmpty()) {
        configPath_ = ResolveConfigPath(configPath);
    } else if (!ResolveConfigPath(QStringLiteral("config/client_config.ini")).isEmpty() &&
               QFile::exists(ResolveConfigPath(QStringLiteral("config/client_config.ini")))) {
        configPath_ = ResolveConfigPath(QStringLiteral("config/client_config.ini"));
    } else if (!ResolveConfigPath(QStringLiteral("client_config.ini")).isEmpty() &&
               QFile::exists(ResolveConfigPath(QStringLiteral("client_config.ini")))) {
        configPath_ = ResolveConfigPath(QStringLiteral("client_config.ini"));
    } else if (!ResolveConfigPath(QStringLiteral("config.ini")).isEmpty() &&
               QFile::exists(ResolveConfigPath(QStringLiteral("config.ini")))) {
        configPath_ = ResolveConfigPath(QStringLiteral("config.ini"));
    } else {
        configPath_ = ResolveConfigPath(QStringLiteral("config/client_config.ini"));
    }
    loadDeviceSyncSettings();
    if (c_api_) {
        mi_client_destroy(c_api_);
        c_api_ = nullptr;
    }
    c_api_ = mi_client_create(configPath_.toStdString().c_str());
    inited_ = c_api_ != nullptr;
    if (!inited_ && !promptedKtRoot_) {
        const char* createErr = mi_client_last_create_error();
        const QString apiErr = createErr ? QString::fromUtf8(createErr) : QString();
        if (IsKtRootError(apiErr)) {
            promptedKtRoot_ = true;
            bool ktApplied = false;
            const QString baseDir = QFileInfo(configPath_).absolutePath();
            const QString pick = QFileDialog::getOpenFileName(
                nullptr,
                UiSettings::Tr(QStringLiteral("选择 KT 根公钥"),
                               QStringLiteral("Select KT root pubkey")),
                baseDir,
                QStringLiteral("KT Root Pubkey (kt_root_pub.bin);;All Files (*)"));
            if (!pick.isEmpty()) {
                QString fpErr;
                const QString fp = KtRootFingerprintHex(pick, fpErr);
                if (!fp.isEmpty()) {
                    const QString sas = KtRootSasHex(fp);
                    const QString desc = UiSettings::Tr(
                        QStringLiteral("请通过可信渠道核对指纹/安全码后再继续。"),
                        QStringLiteral("Verify the fingerprint/SAS via a trusted channel before continuing."));
                    QString input;
                    if (PromptTrustWithSas(nullptr,
                                           UiSettings::Tr(QStringLiteral("验证 KT 根公钥"),
                                                          QStringLiteral("Verify KT root pubkey")),
                                           desc,
                                           fp,
                                           sas,
                                           input)) {
                        QString writeErr;
                        if (WriteKtRootPath(configPath_, pick, writeErr)) {
                            if (c_api_) {
                                mi_client_destroy(c_api_);
                                c_api_ = nullptr;
                            }
                            c_api_ = mi_client_create(configPath_.toStdString().c_str());
                            inited_ = c_api_ != nullptr;
                            ktApplied = inited_;
                        } else {
                            QMessageBox::warning(nullptr,
                                                 UiSettings::Tr(QStringLiteral("写入失败"),
                                                                QStringLiteral("Write failed")),
                                                 writeErr);
                        }
                    }
                } else {
                    QMessageBox::warning(nullptr,
                                         UiSettings::Tr(QStringLiteral("无效公钥"),
                                                        QStringLiteral("Invalid pubkey")),
                                         fpErr);
                }
            }
            if (!ktApplied) {
                promptedKtRoot_ = false;
            }
        }
    }
    return inited_;
}

void BackendAdapter::loadDeviceSyncSettings() {
    device_sync_enabled_ = false;
    device_sync_primary_ = true;
    if (configPath_.isEmpty()) {
        return;
    }
    QSettings settings(configPath_, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("device_sync"));
    device_sync_enabled_ =
        settings.value(QStringLiteral("enabled"), 0).toInt() != 0;
    const QString role =
        settings.value(QStringLiteral("role"), QStringLiteral("primary"))
            .toString()
            .trimmed()
            .toLower();
    device_sync_primary_ = (role != QStringLiteral("linked"));
    settings.endGroup();
}

bool BackendAdapter::ensureInited(QString &err) {
    if (coreWorkActive_.load()) {
        err = QStringLiteral("同步中，请稍后");
        return false;
    }
    if (fileTransferActive_.load()) {
        err = QStringLiteral("文件传输中，请稍后");
        return false;
    }
    if (!inited_) {
        if (!init(configPath_)) {
            const char* createErr = mi_client_last_create_error();
            const QString coreErr =
                createErr ? QString::fromUtf8(createErr) : QString();
            const QString pathHint = configPath_.isEmpty()
                                         ? QStringLiteral("config/client_config.ini")
                                         : configPath_;
            err = coreErr.isEmpty()
                      ? QStringLiteral("后端初始化失败（检查 %1）").arg(pathHint)
                      : QStringLiteral("后端初始化失败：%1（检查 %2）").arg(coreErr, pathHint);
            return false;
        }
    }
    return true;
}

bool BackendAdapter::login(const QString &account, const QString &password, QString &err) {
    const QString user = account.trimmed();
    if (user.isEmpty() || password.isEmpty()) {
        err = QStringLiteral("账号或密码为空");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    QString rawErr;
    const auto loginOnce = [&](QString& outErr) -> bool {
        outErr.clear();
        if (!c_api_) {
            outErr = QStringLiteral("未初始化");
            return false;
        }
        const bool ok = mi_client_login(c_api_, user.toStdString().c_str(),
                                        password.toStdString().c_str()) != 0;
        if (!ok) {
            const char* apiErr = mi_client_last_error(c_api_);
            outErr = apiErr ? QString::fromUtf8(apiErr) : QString();
        }
        return ok;
    };

    if (!loginOnce(rawErr)) {
        QString coreErr = AugmentTransportErrorHint(rawErr);

        if (!attemptedAutoStartServer_ &&
            (coreErr == QStringLiteral("connect failed") || coreErr == QStringLiteral("dns resolve failed")) &&
            (c_api_ && (mi_client_is_remote_mode(c_api_) != 0))) {
            const ServerEndpoint ep = ReadClientEndpoint(configPath_);
            if (IsLoopbackHost(ep.host) && ep.port != 0) {
                const QString serverExe = FindBundledServerExe();
                if (!serverExe.isEmpty()) {
                    const QString serverDir = QFileInfo(serverExe).absolutePath();
                    if (QProcess::startDetached(serverExe, {}, serverDir)) {
                        attemptedAutoStartServer_ = true;
                        mi::platform::SleepMs(250);
                        if (loginOnce(rawErr)) {
                            loggedIn_ = true;
                            currentUser_ = user;
                            lastFriends_.clear();
                            friendSyncForced_.store(true);
                            lastFriendSyncAtMs_.store(0);
                            err.clear();
                            online_ = true;
                            startPolling(basePollIntervalMs_);
                            return true;
                        }
                        coreErr = rawErr.trimmed();
                    }
                }
            }
        }

        if (hasPendingServerTrust()) {
            err = QStringLiteral("首次连接/证书变更：需先信任服务器（TLS）");
        } else if (!coreErr.isEmpty()) {
            const ServerEndpoint ep = ReadClientEndpoint(configPath_);
            if (!ep.host.isEmpty() && ep.port != 0) {
                err = QStringLiteral("%1（%2:%3）").arg(coreErr, ep.host).arg(ep.port);
            } else {
                err = coreErr;
            }
        } else {
            err = QStringLiteral("登录失败：请检查账号/密码或服务器状态");
        }
        loggedIn_ = false;
        online_ = false;
        return false;
    }
    loggedIn_ = true;
    currentUser_ = user;
    lastFriends_.clear();
    friendSyncForced_.store(true);
    lastFriendSyncAtMs_.store(0);
    err.clear();
    online_ = true;
    startPolling(basePollIntervalMs_);
    return true;
}

void BackendAdapter::loginAsync(const QString &account, const QString &password) {
    const QString acc = account.trimmed();
    const QString pwd = password;
    QString initErr;
    if (acc.isEmpty() || pwd.isEmpty()) {
        emit loginFinished(false, QStringLiteral("账号或密码为空"));
        return;
    }
    if (fileTransferActive_.load()) {
        emit loginFinished(false, QStringLiteral("文件传输中，请稍后"));
        return;
    }
    bool expected = false;
    if (!coreWorkActive_.compare_exchange_strong(expected, true)) {
        emit loginFinished(false, QStringLiteral("同步中，请稍后"));
        return;
    }
    if (!inited_) {
        if (!init(configPath_)) {
            const QString path = configPath_.isEmpty()
                                     ? QStringLiteral("config/client_config.ini")
                                     : configPath_;
            const char* createErr = mi_client_last_create_error();
            const QString coreErr =
                createErr ? QString::fromUtf8(createErr) : QString();
            coreWorkActive_.store(false);
            if (coreErr.isEmpty()) {
                emit loginFinished(false, QStringLiteral("后端初始化失败（检查 %1）").arg(path));
            } else {
                emit loginFinished(false, QStringLiteral("后端初始化失败：%1（检查 %2）").arg(coreErr, path));
            }
            return;
        }
    }

    const bool allowAutoStart = !attemptedAutoStartServer_;
    QPointer<BackendAdapter> self(this);
    std::thread([self, acc, pwd, allowAutoStart]() {
        if (!self) {
            return;
        }

        QString rawErr;
        const auto loginOnce = [&](QString& outErr) -> bool {
            outErr.clear();
            if (!self->c_api_) {
                outErr = QStringLiteral("未初始化");
                return false;
            }
            const bool ok = mi_client_login(self->c_api_, acc.toStdString().c_str(),
                                            pwd.toStdString().c_str()) != 0;
            if (!ok) {
                const char* apiErr = mi_client_last_error(self->c_api_);
                outErr = apiErr ? QString::fromUtf8(apiErr) : QString();
            }
            return ok;
        };

        bool success = loginOnce(rawErr);
        bool autoStartAttempted = false;
        QString err;

        if (!success) {
            QString coreErr = AugmentTransportErrorHint(rawErr);

            if (allowAutoStart &&
                !autoStartAttempted &&
                (coreErr == QStringLiteral("connect failed") || coreErr == QStringLiteral("dns resolve failed")) &&
                (self->c_api_ && (mi_client_is_remote_mode(self->c_api_) != 0))) {
                const ServerEndpoint ep = ReadClientEndpoint(self->configPath_);
                if (IsLoopbackHost(ep.host) && ep.port != 0) {
                    const QString serverExe = FindBundledServerExe();
                    if (!serverExe.isEmpty()) {
                        const QString serverDir = QFileInfo(serverExe).absolutePath();
                        if (QProcess::startDetached(serverExe, {}, serverDir)) {
                            autoStartAttempted = true;
                            mi::platform::SleepMs(250);
                            if (loginOnce(rawErr)) {
                                success = true;
                            } else {
                                coreErr = rawErr.trimmed();
                            }
                        }
                    }
                }
            }

            if (!success) {
                if (self->hasPendingServerTrust()) {
                    err = QStringLiteral("首次连接/证书变更：需先信任服务器（TLS）");
                } else if (!coreErr.isEmpty()) {
                    const ServerEndpoint ep = ReadClientEndpoint(self->configPath_);
                    if (!ep.host.isEmpty() && ep.port != 0) {
                        err = QStringLiteral("%1（%2:%3）").arg(coreErr, ep.host).arg(ep.port);
                    } else {
                        err = coreErr;
                    }
                } else {
                    err = QStringLiteral("登录失败：请检查账号/密码或服务器状态");
                }
            }
        }

        QMetaObject::invokeMethod(self, [self, success, err, acc, autoStartAttempted]() {
            if (!self) {
                return;
            }

            if (autoStartAttempted) {
                self->attemptedAutoStartServer_ = true;
            }
            self->coreWorkActive_.store(false);

            if (!success) {
                self->loggedIn_ = false;
                self->online_ = false;
                if (self->pollTimer_) {
                    self->pollTimer_->stop();
                }
                emit self->loginFinished(false, err);
                return;
            }

            self->loggedIn_ = true;
            self->currentUser_ = acc;
            self->lastFriends_.clear();
            self->friendSyncForced_.store(true);
            self->lastFriendSyncAtMs_.store(0);
            self->online_ = true;
            self->startPolling(self->basePollIntervalMs_);
            emit self->loginFinished(true, QString());
        }, Qt::QueuedConnection);
    }).detach();
}

bool BackendAdapter::registerUser(const QString &account, const QString &password, QString &err) {
    const QString acc = account.trimmed();
    const QString pwd = password;
    if (acc.isEmpty() || pwd.isEmpty()) {
        err = QStringLiteral("账号或密码为空");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    QString rawErr;
    const auto registerOnce = [&](QString& outErr) -> bool {
        outErr.clear();
        if (!c_api_) {
            outErr = QStringLiteral("未初始化");
            return false;
        }
        const bool ok = mi_client_register(c_api_, acc.toStdString().c_str(),
                                           pwd.toStdString().c_str()) != 0;
        if (!ok) {
            const char* apiErr = mi_client_last_error(c_api_);
            outErr = apiErr ? QString::fromUtf8(apiErr) : QString();
        }
        return ok;
    };

    if (!registerOnce(rawErr)) {
        QString coreErr = AugmentTransportErrorHint(rawErr);
        if (!attemptedAutoStartServer_ &&
            (coreErr == QStringLiteral("connect failed") || coreErr == QStringLiteral("dns resolve failed")) &&
            (c_api_ && (mi_client_is_remote_mode(c_api_) != 0))) {
            const ServerEndpoint ep = ReadClientEndpoint(configPath_);
            if (IsLoopbackHost(ep.host) && ep.port != 0) {
                const QString serverExe = FindBundledServerExe();
                if (!serverExe.isEmpty()) {
                    const QString serverDir = QFileInfo(serverExe).absolutePath();
                    if (QProcess::startDetached(serverExe, {}, serverDir)) {
                        attemptedAutoStartServer_ = true;
                        mi::platform::SleepMs(250);
                        if (registerOnce(rawErr)) {
                            err.clear();
                            return true;
                        }
                        coreErr = rawErr.trimmed();
                    }
                }
            }
        }

        if (hasPendingServerTrust()) {
            err = QStringLiteral("首次连接/证书变更：需先信任服务器（TLS）");
        } else if (!coreErr.isEmpty()) {
            const ServerEndpoint ep = ReadClientEndpoint(configPath_);
            if (!ep.host.isEmpty() && ep.port != 0) {
                err = QStringLiteral("%1（%2:%3）").arg(coreErr, ep.host).arg(ep.port);
            } else {
                err = coreErr;
            }
        } else {
            err = QStringLiteral("注册失败：请检查账号/密码或服务器状态");
        }
        return false;
    }
    err.clear();
    return true;
}

void BackendAdapter::registerUserAsync(const QString &account, const QString &password) {
    const QString acc = account.trimmed();
    const QString pwd = password;
    if (acc.isEmpty() || pwd.isEmpty()) {
        emit registerFinished(false, QStringLiteral("账号或密码为空"));
        return;
    }
    if (fileTransferActive_.load()) {
        emit registerFinished(false, QStringLiteral("文件传输中，请稍后"));
        return;
    }
    bool expected = false;
    if (!coreWorkActive_.compare_exchange_strong(expected, true)) {
        emit registerFinished(false, QStringLiteral("同步中，请稍后"));
        return;
    }
    if (!inited_) {
        if (!init(configPath_)) {
            const QString path = configPath_.isEmpty()
                                     ? QStringLiteral("config/client_config.ini")
                                     : configPath_;
            const char* createErr = mi_client_last_create_error();
            const QString coreErr =
                createErr ? QString::fromUtf8(createErr) : QString();
            coreWorkActive_.store(false);
            if (coreErr.isEmpty()) {
                emit registerFinished(false, QStringLiteral("后端初始化失败（检查 %1）").arg(path));
            } else {
                emit registerFinished(false, QStringLiteral("后端初始化失败：%1（检查 %2）").arg(coreErr, path));
            }
            return;
        }
    }

    const bool allowAutoStart = !attemptedAutoStartServer_;
    QPointer<BackendAdapter> self(this);
    std::thread([self, acc, pwd, allowAutoStart]() {
        if (!self) {
            return;
        }

        QString rawErr;
        const auto registerOnce = [&](QString& outErr) -> bool {
            outErr.clear();
            if (!self->c_api_) {
                outErr = QStringLiteral("未初始化");
                return false;
            }
            const bool ok = mi_client_register(self->c_api_, acc.toStdString().c_str(),
                                               pwd.toStdString().c_str()) != 0;
            if (!ok) {
                const char* apiErr = mi_client_last_error(self->c_api_);
                outErr = apiErr ? QString::fromUtf8(apiErr) : QString();
            }
            return ok;
        };

        bool success = registerOnce(rawErr);
        bool autoStartAttempted = false;
        QString err;

        if (!success) {
            QString coreErr = AugmentTransportErrorHint(rawErr);
            if (allowAutoStart &&
                !autoStartAttempted &&
                (coreErr == QStringLiteral("connect failed") || coreErr == QStringLiteral("dns resolve failed")) &&
                (self->c_api_ && (mi_client_is_remote_mode(self->c_api_) != 0))) {
                const ServerEndpoint ep = ReadClientEndpoint(self->configPath_);
                if (IsLoopbackHost(ep.host) && ep.port != 0) {
                    const QString serverExe = FindBundledServerExe();
                    if (!serverExe.isEmpty()) {
                        const QString serverDir = QFileInfo(serverExe).absolutePath();
                        if (QProcess::startDetached(serverExe, {}, serverDir)) {
                            autoStartAttempted = true;
                            mi::platform::SleepMs(250);
                            if (registerOnce(rawErr)) {
                                success = true;
                            } else {
                                coreErr = rawErr.trimmed();
                            }
                        }
                    }
                }
            }

            if (!success) {
                if (self->hasPendingServerTrust()) {
                    err = QStringLiteral("首次连接/证书变更：需先信任服务器（TLS）");
                } else if (!coreErr.isEmpty()) {
                    const ServerEndpoint ep = ReadClientEndpoint(self->configPath_);
                    if (!ep.host.isEmpty() && ep.port != 0) {
                        err = QStringLiteral("%1（%2:%3）").arg(coreErr, ep.host).arg(ep.port);
                    } else {
                        err = coreErr;
                    }
                } else {
                    err = QStringLiteral("注册失败：请检查账号/密码或服务器状态");
                }
            }
        }

        QMetaObject::invokeMethod(self, [self, success, err, autoStartAttempted]() {
            if (!self) {
                return;
            }
            if (autoStartAttempted) {
                self->attemptedAutoStartServer_ = true;
            }
            self->coreWorkActive_.store(false);
            emit self->registerFinished(success, err);
        }, Qt::QueuedConnection);
    }).detach();
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return out;
    }
    std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
    const std::uint32_t count =
        mi_client_list_friends(c_api_, buffer.data(), kMaxFriendEntries);
    out = ToFriendEntries(buffer.data(), count);
    lastFriends_ = out;
    err.clear();
    return out;
}

void BackendAdapter::requestFriendList() {
    if (!loggedIn_) {
        emit friendListLoaded({}, QStringLiteral("尚未登录"));
        return;
    }
    QString err;
    if (!ensureInited(err)) {
        emit friendListLoaded({}, err);
        return;
    }

    bool expected = false;
    if (!coreWorkActive_.compare_exchange_strong(expected, true)) {
        emit friendListLoaded({}, QStringLiteral("同步中，请稍后"));
        return;
    }

    QPointer<BackendAdapter> self(this);
    std::thread([self]() {
        if (!self) {
            return;
        }
        QVector<BackendAdapter::FriendEntry> friends;
        bool changed = false;
        bool ok = false;
        std::string coreErr;
        if (!self->c_api_) {
            coreErr = "not initialized";
        } else {
            std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
            int changed_flag = 0;
            const std::uint32_t count =
                mi_client_sync_friends(self->c_api_, buffer.data(),
                                       kMaxFriendEntries, &changed_flag);
            const char* err = mi_client_last_error(self->c_api_);
            ok = !(err && *err);
            if (ok && changed_flag) {
                friends = ToFriendEntries(buffer.data(), count);
                changed = true;
            }
            if (!ok && err) {
                coreErr = err;
            }
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        self->lastFriendSyncAtMs_.store(now);
        self->friendSyncForced_.store(false);
        QMetaObject::invokeMethod(self, [self, friends = std::move(friends), coreErr, ok, changed]() mutable {
            if (!self) {
                return;
            }

            const QString err =
                coreErr.empty() ? QString() : QString::fromStdString(coreErr).trimmed();
            self->coreWorkActive_.store(false);
            if (!ok && self->lastFriends_.isEmpty()) {
                emit self->friendListLoaded({}, err);
                return;
            }
            if (ok && changed) {
                self->lastFriends_ = std::move(friends);
            }
            emit self->friendListLoaded(self->lastFriends_, err);
        }, Qt::QueuedConnection);
    }).detach();
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const QByteArray remarkUtf8 = remark.trimmed().toUtf8();
    const bool ok = mi_client_add_friend(
                        c_api_, target.toStdString().c_str(),
                        remarkUtf8.isEmpty() ? nullptr : remarkUtf8.constData()) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = apiErr && *apiErr ? QString::fromUtf8(apiErr)
                                : QStringLiteral("添加好友失败");
        return false;
    }
    friendSyncForced_.store(true);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const bool ok = mi_client_send_friend_request(
                        c_api_, target.toStdString().c_str(),
                        remark.trimmed().toUtf8().toStdString().c_str()) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = apiErr && *apiErr ? QString::fromUtf8(apiErr)
                                : QStringLiteral("发送好友申请失败");
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return out;
    }
    std::vector<mi_friend_request_entry_t> buffer(kMaxFriendRequestEntries);
    const std::uint32_t count =
        mi_client_list_friend_requests(c_api_, buffer.data(),
                                       kMaxFriendRequestEntries);
    out = ToFriendRequestEntries(buffer.data(), count);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const bool ok = mi_client_respond_friend_request(
                        c_api_, u.toStdString().c_str(), accept ? 1 : 0) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = apiErr && *apiErr ? QString::fromUtf8(apiErr)
                                : QStringLiteral("处理好友申请失败");
        return false;
    }
    if (accept) {
        friendSyncForced_.store(true);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const bool ok =
        mi_client_delete_friend(c_api_, target.toStdString().c_str()) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = apiErr && *apiErr ? QString::fromUtf8(apiErr)
                                : QStringLiteral("删除好友失败");
        return false;
    }
    friendSyncForced_.store(true);
    err.clear();
    return true;
}

bool BackendAdapter::deleteChatHistory(const QString &convId, bool isGroup,
                                       bool deleteAttachments, bool secureWipe,
                                       QString &err) {
    const QString cid = convId.trimmed();
    if (cid.isEmpty()) {
        err = QStringLiteral("会话 ID 为空");
        return false;
    }
    if (!ensureInited(err)) {
        return false;
    }
    bool ok = false;
    QString errMsg;
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    ok = mi_client_delete_chat_history(
        c_api_, cid.toStdString().c_str(), isGroup ? 1 : 0,
        deleteAttachments ? 1 : 0, secureWipe ? 1 : 0) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("删除聊天记录失败") : errMsg;
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
    bool ok = false;
    QString errMsg;
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    ok = mi_client_set_user_blocked(c_api_, target.toStdString().c_str(),
                                    blocked ? 1 : 0) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("操作失败") : errMsg;
        return false;
    }
    if (blocked) {
        friendSyncForced_.store(true);
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
    bool ok = false;
    QString errMsg;
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    ok = mi_client_set_friend_remark(
        c_api_, target.toStdString().c_str(),
        remark.trimmed().toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("备注更新失败：账号不存在或服务器异常") : errMsg;
        return false;
    }
    friendSyncForced_.store(true);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }

    std::string msgId;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_private_text(c_api_, targetId.toStdString().c_str(),
                                     text.toUtf8().toStdString().c_str(),
                                     &outId) != 0;
    if (outId) {
        msgId.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("后端发送失败") : errMsg;
        const bool retryable = !IsNonRetryableSendError(errMsg);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }

    std::string msgId;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_private_text_with_reply(
        c_api_, targetId.toStdString().c_str(),
        text.toUtf8().toStdString().c_str(),
        replyToMessageId.trimmed().toStdString().c_str(),
        replyPreview.toUtf8().toStdString().c_str(),
        &outId) != 0;
    if (outId) {
        msgId.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const QString errQ = errMsg.trimmed();
        err = errQ.isEmpty() ? QStringLiteral("后端发送失败") : errQ;
        const bool retryable = !IsNonRetryableSendError(errQ);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    QString errMsg;
    auto it = pendingOutgoing_.find(key);
    if (it != pendingOutgoing_.end() && !it->second.isFile && !it->second.isGroup) {
        const PendingOutgoing &p = it->second;
        if (p.kind == PendingOutgoing::Kind::ReplyText) {
            ok = mi_client_resend_private_text_with_reply(
                c_api_, targetId.toStdString().c_str(), key.c_str(),
                p.text.toUtf8().toStdString().c_str(),
                p.replyToMessageId.trimmed().toStdString().c_str(),
                p.replyPreview.toUtf8().toStdString().c_str()) != 0;
            const char* apiErr = mi_client_last_error(c_api_);
            if (apiErr && *apiErr) {
                errMsg = QString::fromUtf8(apiErr);
            }
        } else if (p.kind == PendingOutgoing::Kind::Location) {
            ok = mi_client_resend_private_location(
                c_api_, targetId.toStdString().c_str(), key.c_str(),
                static_cast<std::int32_t>(p.latE7),
                static_cast<std::int32_t>(p.lonE7),
                p.locationLabel.toUtf8().toStdString().c_str()) != 0;
            const char* apiErr = mi_client_last_error(c_api_);
            if (apiErr && *apiErr) {
                errMsg = QString::fromUtf8(apiErr);
            }
        } else if (p.kind == PendingOutgoing::Kind::ContactCard) {
            ok = mi_client_resend_private_contact(
                c_api_, targetId.toStdString().c_str(), key.c_str(),
                p.cardUsername.trimmed().toStdString().c_str(),
                p.cardDisplay.toUtf8().toStdString().c_str()) != 0;
            const char* apiErr = mi_client_last_error(c_api_);
            if (apiErr && *apiErr) {
                errMsg = QString::fromUtf8(apiErr);
            }
        }
    }
    if (!ok) {
        ok = mi_client_resend_private_text(
            c_api_, targetId.toStdString().c_str(), key.c_str(),
            text.toUtf8().toStdString().c_str()) != 0;
        const char* apiErr = mi_client_last_error(c_api_);
        if (apiErr && *apiErr) {
            errMsg = QString::fromUtf8(apiErr);
        }
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("重试失败") : errMsg;
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
    cacheAttachmentPreviewForSend(targetId, outMessageId, filePath);
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

    cacheAttachmentPreviewForSend(targetId, messageId, filePath);
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
        std::string errStr;
        if (!self->c_api_) {
            errStr = "not initialized";
        } else {
            const QByteArray pathUtf8 = pathStr.toUtf8();
            if (isGroup) {
                ok = mi_client_resend_group_file(self->c_api_,
                                                 cid.toStdString().c_str(),
                                                 mid.toStdString().c_str(),
                                                 pathUtf8.constData()) != 0;
            } else {
                ok = mi_client_resend_private_file(self->c_api_,
                                                   cid.toStdString().c_str(),
                                                   mid.toStdString().c_str(),
                                                   pathUtf8.constData()) != 0;
            }
            const char* apiErr = mi_client_last_error(self->c_api_);
            if (apiErr && *apiErr) {
                errStr = apiErr;
            }
        }

        const QString err = errStr.empty() ? QString() : QString::fromStdString(errStr);
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
                const bool retryable = !IsNonRetryableSendError(err);
                if (retryable) {
                    PendingOutgoing p;
                    p.convId = cid;
                    p.messageId = mid;
                    p.isGroup = isGroup;
                    p.isFile = true;
                    p.filePath = pathStr;
                    self->pendingOutgoing_[mid.toStdString()] = std::move(p);
                }
            }
            {
                const QString key = cid.trimmed() + QStringLiteral("|") + mid.trimmed();
                QMutexLocker locker(&self->pendingAttachmentPreviewLock_);
                self->pendingAttachmentPreviews_.remove(key);
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
                                        const ChatFileEntry &file,
                                        const QString &outPath) {
    bool expected = false;
    if (!fileTransferActive_.compare_exchange_strong(expected, true)) {
        emit fileSaveFinished(convId, messageId, false, QStringLiteral("已有文件传输在进行"), outPath);
        return;
    }

    const QString cid = convId.trimmed();
    const QString mid = messageId.trimmed();
    const QString outPathStr = outPath;
    const ChatFileEntry fileCopy = file;
    QPointer<BackendAdapter> self(this);

    std::thread([self, cid, mid, fileCopy, outPathStr]() {
        if (!self) {
            return;
        }
        bool ok = false;
        std::string errStr;
        if (!self->c_api_) {
            errStr = "not initialized";
        } else {
            const QByteArray pathUtf8 = outPathStr.toUtf8();
            ok = mi_client_download_chat_file_to_path(
                     self->c_api_, fileCopy.file_id.c_str(),
                     fileCopy.file_key.data(),
                     static_cast<std::uint32_t>(fileCopy.file_key.size()),
                     fileCopy.file_name.c_str(), fileCopy.file_size,
                     pathUtf8.constData(), 1, nullptr, nullptr) != 0;
            const char* apiErr = mi_client_last_error(self->c_api_);
            if (apiErr && *apiErr) {
                errStr = apiErr;
            }
        }

        const QString err = errStr.empty() ? QString() : QString::fromStdString(errStr);
        QMetaObject::invokeMethod(self, [self, cid, mid, outPathStr, ok, err, fileCopy]() {
            if (!self) {
                return;
            }
            self->fileTransferActive_.store(false);
            emit self->fileSaveFinished(cid, mid, ok,
                                        ok ? QString() : (err.isEmpty() ? QStringLiteral("保存失败") : err),
                                        outPathStr);
            if (ok) {
                self->storeAttachmentPreviewForPath(fileCopy, outPathStr);
            }
            if (!ok) {
                self->maybeEmitPeerTrustRequired(true);
                self->maybeEmitServerTrustRequired(true);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void BackendAdapter::cacheAttachmentPreviewForSend(const QString &convId,
                                                   const QString &messageId,
                                                   const QString &filePath) {
    const QString cid = convId.trimmed();
    const QString mid = messageId.trimmed();
    const QString path = filePath.trimmed();
    if (cid.isEmpty() || mid.isEmpty() || path.isEmpty()) {
        return;
    }
    QByteArray preview;
    if (!BuildAttachmentPreviewBytes(path, preview) || preview.isEmpty()) {
        return;
    }
    const QString key = cid + QStringLiteral("|") + mid;
    QMutexLocker locker(&pendingAttachmentPreviewLock_);
    if (pendingAttachmentPreviews_.size() > 64) {
        pendingAttachmentPreviews_.clear();
    }
    pendingAttachmentPreviews_.insert(key, preview);
}

void BackendAdapter::applyCachedAttachmentPreview(
    const QString &convId,
    const QString &messageId,
    const ChatFileEntry &file) {
    if (convId.trimmed().isEmpty() || messageId.trimmed().isEmpty() ||
        file.file_id.empty()) {
        return;
    }
    const QString key = convId.trimmed() + QStringLiteral("|") + messageId.trimmed();
    QByteArray preview;
    {
        QMutexLocker locker(&pendingAttachmentPreviewLock_);
        auto it = pendingAttachmentPreviews_.find(key);
        if (it == pendingAttachmentPreviews_.end()) {
            return;
        }
        preview = it.value();
        pendingAttachmentPreviews_.erase(it);
    }
    if (preview.isEmpty()) {
        return;
    }
    std::vector<std::uint8_t> bytes(preview.begin(), preview.end());
    if (!c_api_) {
        return;
    }
    mi_client_store_attachment_preview_bytes(
        c_api_, file.file_id.c_str(), file.file_name.c_str(),
        file.file_size, bytes.data(),
        static_cast<std::uint32_t>(bytes.size()));
}

void BackendAdapter::storeAttachmentPreviewForPath(
    const ChatFileEntry &file,
    const QString &filePath) {
    if (file.file_id.empty()) {
        return;
    }
    const QString path = filePath.trimmed();
    if (path.isEmpty()) {
        return;
    }
    QByteArray preview;
    if (!BuildAttachmentPreviewBytes(path, preview) || preview.isEmpty()) {
        return;
    }
    std::vector<std::uint8_t> bytes(preview.begin(), preview.end());
    if (!c_api_) {
        return;
    }
    mi_client_store_attachment_preview_bytes(
        c_api_, file.file_id.c_str(), file.file_name.c_str(),
        file.file_size, bytes.data(),
        static_cast<std::uint32_t>(bytes.size()));
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
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_private_location(
        c_api_, targetId.trimmed().toStdString().c_str(),
        static_cast<std::int32_t>(latE7),
        static_cast<std::int32_t>(lonE7),
        label.toUtf8().toStdString().c_str(),
        &outId) != 0;
    if (outId) {
        msgId.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const QString errQ = errMsg.trimmed();
        err = errQ.isEmpty() ? QStringLiteral("发送位置失败") : errQ;
        const bool retryable = !IsNonRetryableSendError(errQ);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }

    std::string msgId;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_private_contact(
        c_api_, targetId.trimmed().toStdString().c_str(),
        cardUsername.trimmed().toStdString().c_str(),
        cardDisplay.toUtf8().toStdString().c_str(),
        &outId) != 0;
    if (outId) {
        msgId.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const QString errQ = errMsg.trimmed();
        err = errQ.isEmpty() ? QStringLiteral("发送名片失败") : errQ;
        const bool retryable = !IsNonRetryableSendError(errQ);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }

    std::string msgId;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_private_sticker(
        c_api_, targetId.trimmed().toStdString().c_str(),
        stickerId.trimmed().toStdString().c_str(),
        &outId) != 0;
    if (outId) {
        msgId.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    outMessageId = QString::fromStdString(msgId);
    if (!ok) {
        const QString errQ = errMsg.trimmed();
        err = errQ.isEmpty() ? QStringLiteral("发送贴纸失败") : errQ;
        const bool retryable = !IsNonRetryableSendError(errQ);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const std::string mid = messageId.trimmed().toStdString();
    bool ok = false;
    QString errMsg;
    ok = mi_client_resend_private_sticker(
        c_api_, targetId.trimmed().toStdString().c_str(), mid.c_str(),
        stickerId.trimmed().toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("重试发送贴纸失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_send_read_receipt(c_api_,
                                     targetId.trimmed().toStdString().c_str(),
                                     messageId.trimmed().toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("发送已读回执失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_send_typing(c_api_,
                               targetId.trimmed().toStdString().c_str(),
                               typing ? 1 : 0) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("发送输入状态失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_send_presence(c_api_,
                                 targetId.trimmed().toStdString().c_str(),
                                 online ? 1 : 0) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("发送在线状态失败") : errMsg;
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

    std::uint8_t* plain = nullptr;
    std::uint64_t plainLen = 0;
    const ChatFileEntry& file = it->second;
    const bool ok = mi_client_download_chat_file_to_bytes(
                        c_api_, file.file_id.c_str(), file.file_key.data(),
                        static_cast<std::uint32_t>(file.file_key.size()),
                        file.file_name.c_str(), file.file_size,
                        wipeAfterRead ? 1 : 0, &plain, &plainLen) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = (apiErr && *apiErr) ? QString::fromUtf8(apiErr)
                                  : QStringLiteral("下载失败");
        return false;
    }
    if (maxBytes > 0 && plainLen > static_cast<std::uint64_t>(maxBytes)) {
        if (plain) {
            mi_client_free(plain);
        }
        err = QStringLiteral("文件过大，无法预览");
        return false;
    }
    if (plain && plainLen > 0) {
        outBytes = QByteArray(reinterpret_cast<const char *>(plain),
                              static_cast<int>(plainLen));
        mi_client_free(plain);
    } else {
        outBytes.clear();
    }
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }

    const std::size_t safeLimit = limit == 0 ? 200u : static_cast<std::size_t>(limit);
    std::vector<mi_history_entry_t> buffer(safeLimit);
    const std::uint32_t count =
        mi_client_load_chat_history(c_api_, cid.toStdString().c_str(),
                                    isGroup ? 1 : 0,
                                    static_cast<std::uint32_t>(safeLimit),
                                    buffer.data(),
                                    static_cast<std::uint32_t>(buffer.size()));
    if (count == 0) {
        const char* apiErr = mi_client_last_error(c_api_);
        if (apiErr && *apiErr) {
            err = QString::fromUtf8(apiErr);
            return false;
        }
        err.clear();
        return true;
    }
    outEntries.reserve(static_cast<int>(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        const mi_history_entry_t& e = buffer[i];
        HistoryMessageEntry h;
        h.outgoing = e.outgoing != 0;
        h.timestampSec = static_cast<quint64>(e.timestamp_sec);
        h.convId = cid;
        h.sender = e.sender ? QString::fromUtf8(e.sender) : QString();
        h.messageId = e.message_id ? QString::fromUtf8(e.message_id) : QString();

        switch (static_cast<mi::sdk::HistoryStatus>(e.status)) {
        case mi::sdk::HistoryStatus::kSent:
            h.status = 0;
            break;
        case mi::sdk::HistoryStatus::kDelivered:
            h.status = 1;
            break;
        case mi::sdk::HistoryStatus::kRead:
            h.status = 2;
            break;
        case mi::sdk::HistoryStatus::kFailed:
            h.status = 3;
            break;
        }

        switch (static_cast<mi::sdk::HistoryKind>(e.kind)) {
        case mi::sdk::HistoryKind::kText: {
            const QString text = e.text ? QString::fromUtf8(e.text) : QString();
            const auto invite = mi::ui::ParseCallInvite(text);
            if (!isGroup && invite.ok) {
                h.kind = 4;
                if (h.outgoing) {
                    h.text = invite.video
                                 ? UiSettings::Tr(QStringLiteral("已发起视频通话"),
                                                  QStringLiteral("Video call started"))
                                 : UiSettings::Tr(QStringLiteral("已发起语音通话"),
                                                  QStringLiteral("Voice call started"));
                } else {
                    h.text = invite.video
                                 ? UiSettings::Tr(QStringLiteral("视频通话邀请"),
                                                  QStringLiteral("Incoming video call"))
                                 : UiSettings::Tr(QStringLiteral("语音通话邀请"),
                                                  QStringLiteral("Incoming voice call"));
                }
            } else {
                h.kind = 1;
                h.text = text;
            }
            break;
        }
        case mi::sdk::HistoryKind::kFile: {
            h.kind = 2;
            h.fileName = e.file_name ? QString::fromUtf8(e.file_name) : QString();
            h.fileSize = static_cast<qint64>(e.file_size);
            ChatFileEntry f;
            if (e.file_id) {
                f.file_id = e.file_id;
            }
            if (e.file_key && e.file_key_len == f.file_key.size()) {
                std::memcpy(f.file_key.data(), e.file_key, f.file_key.size());
            }
            if (e.file_name) {
                f.file_name = e.file_name;
            }
            f.file_size = e.file_size;
            const std::string key =
                cid.toStdString() + "|" + h.messageId.toStdString();
            receivedFiles_[key] = std::move(f);
            break;
        }
        case mi::sdk::HistoryKind::kSticker:
            h.kind = 3;
            h.stickerId = e.sticker_id ? QString::fromUtf8(e.sticker_id) : QString();
            break;
        case mi::sdk::HistoryKind::kSystem:
            h.kind = 4;
            h.text = e.text ? QString::fromUtf8(e.text) : QString();
            break;
        default:
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    std::string groupId;
    bool ok = false;
    QString errMsg;
    char* outGroup = nullptr;
    ok = mi_client_create_group(c_api_, &outGroup) != 0;
    if (outGroup) {
        groupId.assign(outGroup);
        mi_client_free(outGroup);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("创建群聊失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_join_group(c_api_, gid.toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("加入群聊失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_leave_group(c_api_, gid.toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("退出群聊失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return out;
    }
    std::vector<mi_group_member_entry_t> buffer(kMaxGroupMemberEntries);
    const std::uint32_t count =
        mi_client_list_group_members_info(c_api_, gid.toStdString().c_str(),
                                          buffer.data(), kMaxGroupMemberEntries);
    if (count == 0) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = (apiErr && *apiErr) ? QString::fromUtf8(apiErr)
                                  : QStringLiteral("获取成员列表失败");
        return out;
    }
    out = ToGroupMemberNames(buffer.data(), count);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return out;
    }
    std::vector<mi_group_member_entry_t> buffer(kMaxGroupMemberEntries);
    const std::uint32_t count =
        mi_client_list_group_members_info(c_api_, gid.toStdString().c_str(),
                                          buffer.data(), kMaxGroupMemberEntries);
    if (count == 0) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = (apiErr && *apiErr) ? QString::fromUtf8(apiErr)
                                  : QStringLiteral("获取成员信息失败");
        maybeEmitServerTrustRequired(true);
        return out;
    }
    out = ToGroupMemberRoleEntries(buffer.data(), count);
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
    if (role != 1 && role != 2) {
        err = QStringLiteral("角色无效");
        return false;
    }
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_set_group_member_role(
        c_api_, gid.toStdString().c_str(),
        who.toStdString().c_str(),
        static_cast<std::uint32_t>(role)) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("设置角色失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_kick_group_member(c_api_, gid.toStdString().c_str(),
                                     who.toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("踢人失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    std::string mid;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_group_invite(c_api_, gid.toStdString().c_str(),
                                     to.toStdString().c_str(), &outId) != 0;
    if (outId) {
        mid.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("邀请失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    std::string mid;
    bool ok = false;
    QString errMsg;
    char* outId = nullptr;
    ok = mi_client_send_group_text(c_api_, gid.toStdString().c_str(),
                                   t.toUtf8().toStdString().c_str(),
                                   &outId) != 0;
    if (outId) {
        mid.assign(outId);
        mi_client_free(outId);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    if (!ok) {
        outMessageId = QString::fromStdString(mid);
        err = errMsg.isEmpty() ? QStringLiteral("发送失败") : errMsg;
        const bool retryable = !IsNonRetryableSendError(errMsg);
        if (retryable && !outMessageId.trimmed().isEmpty()) {
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
    if (apiErr && *apiErr) {
        err = QString::fromUtf8(apiErr);  // partial failure warning
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_resend_group_text(c_api_, gid.toStdString().c_str(),
                                     mid.toStdString().c_str(),
                                     text.toUtf8().toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr).trimmed();
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("重试失败") : errMsg;
        const bool retryable = !IsNonRetryableSendError(err);
        if (retryable) {
            PendingOutgoing p;
            p.convId = gid;
            p.messageId = mid;
            p.isGroup = true;
            p.isFile = false;
            p.text = text;
            pendingOutgoing_[mid.toStdString()] = std::move(p);
        }
        maybeEmitPeerTrustRequired(true);
        maybeEmitServerTrustRequired(true);
        return false;
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
    cacheAttachmentPreviewForSend(gid, outMessageId, path);
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

    cacheAttachmentPreviewForSend(gid, mid, path);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const bool ok =
        mi_client_trust_pending_peer(c_api_, pin.trimmed().toStdString().c_str()) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        const QString msg = apiErr ? QString::fromUtf8(apiErr) : QString();
        err = msg.isEmpty() ? QStringLiteral("信任失败") : msg;
        return false;
    }
    err.clear();
    return true;
}

bool BackendAdapter::trustPendingServer(const QString &pin, QString &err) {
    if (!ensureInited(err)) {
        return false;
    }
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    const bool ok =
        mi_client_trust_pending_server(c_api_, pin.trimmed().toStdString().c_str()) != 0;
    if (!ok) {
        const char* apiErr = mi_client_last_error(c_api_);
        const QString msg = apiErr ? QString::fromUtf8(apiErr) : QString();
        err = msg.isEmpty() ? QStringLiteral("信任失败") : msg;
        return false;
    }
    err.clear();
    return true;
}

QString BackendAdapter::currentDeviceId() const {
    if (fileTransferActive_.load()) {
        return {};
    }
    if (c_api_) {
        const char* value = mi_client_device_id(c_api_);
        return value ? QString::fromUtf8(value) : QString();
    }
    return {};
}

bool BackendAdapter::isPendingOutgoingMessage(const QString &messageId) const {
    const std::string key = messageId.trimmed().toStdString();
    if (key.empty()) {
        return false;
    }
    return pendingOutgoing_.find(key) != pendingOutgoing_.end();
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return out;
    }
    std::vector<mi_device_entry_t> buffer(kMaxDeviceEntries);
    const std::uint32_t count =
        mi_client_list_devices(c_api_, buffer.data(), kMaxDeviceEntries);
    if (count == 0) {
        const char* apiErr = mi_client_last_error(c_api_);
        err = (apiErr && *apiErr) ? QString::fromUtf8(apiErr)
                                  : QStringLiteral("获取设备列表失败");
        return out;
    }
    out = ToDeviceEntries(buffer.data(), count);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_kick_device(c_api_, target.toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("踢下线失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    std::string code;
    bool ok = false;
    QString errMsg;
    char* outCode = nullptr;
    ok = mi_client_begin_device_pairing_primary(c_api_, &outCode) != 0;
    if (outCode) {
        code.assign(outCode);
        mi_client_free(outCode);
    }
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("生成配对码失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    std::vector<mi_device_pairing_request_t> buffer(
        kMaxDevicePairingRequests);
    const std::uint32_t count =
        mi_client_poll_device_pairing_requests(
            c_api_, buffer.data(), kMaxDevicePairingRequests);
    if (count == 0) {
        const char* apiErr = mi_client_last_error(c_api_);
        if (apiErr && *apiErr) {
            err = QString::fromUtf8(apiErr);
            return false;
        }
    }
    outRequests = ToDevicePairingRequests(buffer.data(), count);
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_approve_device_pairing_request(
        c_api_, request.deviceId.trimmed().toStdString().c_str(),
        request.requestIdHex.trimmed().toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("确认配对失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    ok = mi_client_begin_device_pairing_linked(
             c_api_, code.toStdString().c_str()) != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("开始配对失败") : errMsg;
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
    if (!c_api_) {
        err = QStringLiteral("未初始化");
        return false;
    }
    bool ok = false;
    QString errMsg;
    int completed = 0;
    ok = mi_client_poll_device_pairing_linked(c_api_, &completed) != 0;
    outCompleted = completed != 0;
    const char* apiErr = mi_client_last_error(c_api_);
    if (apiErr && *apiErr) {
        errMsg = QString::fromUtf8(apiErr);
    }
    if (!ok) {
        err = errMsg.isEmpty() ? QStringLiteral("配对轮询失败") : errMsg;
        return false;
    }
    err.clear();
    return true;
}

void BackendAdapter::cancelDevicePairing() {
    if (fileTransferActive_.load()) {
        return;
    }
    if (c_api_) {
        mi_client_cancel_device_pairing(c_api_);
    }
}

void BackendAdapter::startPolling(int intervalMs) {
    basePollIntervalMs_ = intervalMs;
    friendSyncIntervalMs_ = intervalMs;
    if (!pollTimer_) {
        pollTimer_ = std::make_unique<QTimer>(this);
        connect(pollTimer_.get(), &QTimer::timeout, this, &BackendAdapter::pollMessages);
    }
    currentPollIntervalMs_ = intervalMs;
    pollTimer_->start(currentPollIntervalMs_);
    updateConnectionState();
}

void BackendAdapter::maybeEmitPeerTrustRequired(bool force) {
    QString peer;
    QString fingerprint;
    QString pin;
    if (c_api_) {
        if (mi_client_has_pending_peer_trust(c_api_) == 0) {
            lastPeerTrustUser_.clear();
            lastPeerTrustFingerprint_.clear();
            return;
        }
        const char* peer_c = mi_client_pending_peer_username(c_api_);
        const char* fp_c = mi_client_pending_peer_fingerprint(c_api_);
        const char* pin_c = mi_client_pending_peer_pin(c_api_);
        peer = peer_c ? QString::fromUtf8(peer_c) : QString();
        fingerprint = fp_c ? QString::fromUtf8(fp_c) : QString();
        pin = pin_c ? QString::fromUtf8(pin_c) : QString();
    } else {
        lastPeerTrustUser_.clear();
        lastPeerTrustFingerprint_.clear();
        return;
    }

    if (!force && peer == lastPeerTrustUser_ && fingerprint == lastPeerTrustFingerprint_) {
        return;
    }
    lastPeerTrustUser_ = peer;
    lastPeerTrustFingerprint_ = fingerprint;
    emit peerTrustRequired(peer, fingerprint, pin);
}

void BackendAdapter::maybeEmitServerTrustRequired(bool force) {
    QString fingerprint;
    QString pin;
    if (c_api_) {
        if (mi_client_has_pending_server_trust(c_api_) == 0) {
            lastServerTrustFingerprint_.clear();
            return;
        }
        const char* fp_c = mi_client_pending_server_fingerprint(c_api_);
        const char* pin_c = mi_client_pending_server_pin(c_api_);
        fingerprint = fp_c ? QString::fromUtf8(fp_c) : QString();
        pin = pin_c ? QString::fromUtf8(pin_c) : QString();
    } else {
        lastServerTrustFingerprint_.clear();
        return;
    }
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
    if (!c_api_) {
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

            cacheAttachmentPreviewForSend(p.convId, p.messageId, p.filePath);
            startAsyncFileSend(p.convId, p.isGroup, p.messageId, p.filePath, true);
            ++it;
            return;
        } else {
            if (p.isGroup) {
                ok = mi_client_resend_group_text(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    p.text.toUtf8().toStdString().c_str()) != 0;
            } else if (p.kind == PendingOutgoing::Kind::ReplyText) {
                ok = mi_client_resend_private_text_with_reply(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    p.text.toUtf8().toStdString().c_str(),
                    p.replyToMessageId.trimmed().toStdString().c_str(),
                    p.replyPreview.toUtf8().toStdString().c_str()) != 0;
            } else if (p.kind == PendingOutgoing::Kind::Location) {
                ok = mi_client_resend_private_location(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    static_cast<std::int32_t>(p.latE7),
                    static_cast<std::int32_t>(p.lonE7),
                    p.locationLabel.toUtf8().toStdString().c_str()) != 0;
            } else if (p.kind == PendingOutgoing::Kind::ContactCard) {
                ok = mi_client_resend_private_contact(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    p.cardUsername.trimmed().toStdString().c_str(),
                    p.cardDisplay.toUtf8().toStdString().c_str()) != 0;
            } else if (p.kind == PendingOutgoing::Kind::Sticker) {
                ok = mi_client_resend_private_sticker(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    p.stickerId.trimmed().toStdString().c_str()) != 0;
            } else {
                ok = mi_client_resend_private_text(
                    c_api_, p.convId.toStdString().c_str(),
                    p.messageId.toStdString().c_str(),
                    p.text.toUtf8().toStdString().c_str()) != 0;
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
    QString tokenValue;
    const bool remoteMode = c_api_ && (mi_client_is_remote_mode(c_api_) != 0);
    if (c_api_) {
        const char* token = mi_client_token(c_api_);
        tokenValue = token ? QString::fromUtf8(token) : QString();
    }

    if (!loggedIn_) {
        online_ = false;
        detail = QStringLiteral("未登录");
    } else if (hasPendingServerTrust()) {
        online_ = false;
        detail = QStringLiteral("需信任服务器（TLS）");
    } else if (tokenValue.isEmpty()) {
        online_ = false;
        detail = QStringLiteral("会话失效（正在重连）");
    } else if (!remoteMode) {
        online_ = true;
        detail = QStringLiteral("本地模式");
    } else {
        bool remoteOk = false;
        QString remoteErr;
        if (c_api_) {
            remoteOk = mi_client_remote_ok(c_api_) != 0;
            const char* value = mi_client_remote_error(c_api_);
            remoteErr = value ? QString::fromUtf8(value) : QString();
        }
        if (remoteOk) {
            online_ = true;
            detail = QStringLiteral("在线");
        } else {
            online_ = false;
            detail = remoteErr.trimmed().isEmpty() ? QStringLiteral("离线")
                                                   : QStringLiteral("离线：%1").arg(remoteErr);
        }
    }

    if (pollTimer_) {
        int nextInterval = basePollIntervalMs_;
        if (!online_ && hasPendingServerTrust()) {
            backoffExp_ = 0;
            nextInterval = qMax(basePollIntervalMs_, 5000);
        } else if (!online_ && loggedIn_ && tokenValue.isEmpty()) {
            backoffExp_ = qMin(backoffExp_ + 1, 5);
            nextInterval = qMin(30000, basePollIntervalMs_ * (1 << backoffExp_));
            nextInterval = qMax(nextInterval, 5000);
        } else if (!online_ && loggedIn_ && remoteMode) {
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
    if (pollingSuspended_) {
        return;
    }
    QString err;
    if (!ensureInited(err)) {
        return;
    }
    if (!c_api_) {
        return;
    }

    bool expected = false;
    if (!coreWorkActive_.compare_exchange_strong(expected, true)) {
        return;
    }

    QPointer<BackendAdapter> self(this);
    class PollTask final : public QRunnable {
    public:
        explicit PollTask(QPointer<BackendAdapter> target) : target_(std::move(target)) {
            setAutoDelete(true);
        }

        void run() override {
            if (!target_) {
                return;
            }

            if (!target_->c_api_) {
                return;
            }
            const char* token = mi_client_token(target_->c_api_);
            if ((!token || *token == '\0') && !target_->hasPendingServerTrust()) {
                mi_client_relogin(target_->c_api_);
            }
            mi::sdk::ChatPollResult events;
            std::string pollErr;
            mi::sdk::PollResult polled;
            (void)mi::sdk::PollEvents(target_->c_api_, 64, 0, polled, pollErr);
            events = std::move(polled.chat);
            std::vector<mi::sdk::FriendRequestEntry> reqs;
            std::vector<mi_friend_request_entry_t> req_buffer(kMaxFriendRequestEntries);
            const std::uint32_t req_count =
                mi_client_list_friend_requests(target_->c_api_, req_buffer.data(),
                                               kMaxFriendRequestEntries);
            reqs = ToFriendRequestVector(req_buffer.data(), req_count);
            std::vector<mi::sdk::FriendEntry> syncedFriends;
            bool syncChanged = false;
            std::string syncErr;
            bool didSync = false;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 last = target_->lastFriendSyncAtMs_.load();
            const bool force = target_->friendSyncForced_.load();
            if (force || (last == 0) || (now - last >= target_->friendSyncIntervalMs_)) {
                didSync = true;
                std::vector<mi_friend_entry_t> buffer(kMaxFriendEntries);
                int changed_flag = 0;
                const std::uint32_t count =
                    mi_client_sync_friends(target_->c_api_, buffer.data(),
                                           kMaxFriendEntries, &changed_flag);
                const char* err = mi_client_last_error(target_->c_api_);
                if (err && *err) {
                    syncErr = err;
                } else {
                    syncChanged = (changed_flag != 0);
                    if (syncChanged) {
                        syncedFriends = ToFriendVector(buffer.data(), count);
                    }
                }
                target_->lastFriendSyncAtMs_.store(now);
                target_->friendSyncForced_.store(false);
            }

            QMetaObject::invokeMethod(
                target_,
                [target = target_, events = std::move(events),
                 reqs = std::move(reqs),
                 syncedFriends = std::move(syncedFriends),
                 syncChanged, syncErr, didSync]() mutable {
                    if (!target) {
                        return;
                    }
                    target->handlePollResult(std::move(events), std::move(reqs));
                    if (didSync) {
                        const QString err =
                            syncErr.empty() ? QString() : QString::fromStdString(syncErr).trimmed();
                        target->applyFriendSync(syncedFriends, syncChanged, err, false);
                    }
                },
                Qt::QueuedConnection);
        }

    private:
        QPointer<BackendAdapter> target_;
    };

    core_pool_.start(new PollTask(self));
}

void BackendAdapter::handlePollResult(mi::sdk::ChatPollResult events,
                                      std::vector<mi::sdk::FriendRequestEntry> friendRequests) {
    coreWorkActive_.store(false);
    const bool prevSuspend = pollingSuspended_;
    pollingSuspended_ = true;

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
        ChatFileEntry asFile;
        asFile.file_id = f.file_id;
        asFile.file_key = f.file_key;
        asFile.file_name = f.file_name;
        asFile.file_size = f.file_size;
        const std::string k = f.peer_username + "|" + f.message_id_hex;
        receivedFiles_[k] = asFile;
        applyCachedAttachmentPreview(QString::fromStdString(f.peer_username),
                                     QString::fromStdString(f.message_id_hex),
                                     asFile);
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
        ChatFileEntry asFile;
        asFile.file_id = f.file_id;
        asFile.file_key = f.file_key;
        asFile.file_name = f.file_name;
        asFile.file_size = f.file_size;
        const std::string k = f.group_id + "|" + f.message_id_hex;
        receivedFiles_[k] = asFile;
        applyCachedAttachmentPreview(QString::fromStdString(f.group_id),
                                     QString::fromStdString(f.message_id_hex),
                                     asFile);

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
        ChatFileEntry entry;
        entry.file_id = f.file_id;
        entry.file_key = f.file_key;
        entry.file_name = f.file_name;
        entry.file_size = f.file_size;
        receivedFiles_[k] = std::move(entry);
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
        ChatFileEntry asFile;
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
            if (n.role == mi::sdk::GroupMemberRole::kOwner) {
                roleText = QStringLiteral("群主");
            } else if (n.role == mi::sdk::GroupMemberRole::kAdmin) {
                roleText = QStringLiteral("管理员");
            }
            text = QStringLiteral("%1 将 %2 设为 %3").arg(actor, target, roleText);
            break;
        }
        default:
            continue;
        }
        emit groupNoticeReceived(groupId, text);
        emit groupNoticeEvent(groupId, n.kind, actor, target);
    }

    std::unordered_set<std::string> current;
    current.reserve(friendRequests.size());
    for (const auto &r : friendRequests) {
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

    maybeEmitPeerTrustRequired(false);
    maybeEmitServerTrustRequired(false);
    if (online_) {
        maybeRetryPendingOutgoing();
    }

    pollingSuspended_ = prevSuspend;
}

void BackendAdapter::applyFriendSync(
    const std::vector<mi::sdk::FriendEntry> &friends,
    bool changed, const QString &err, bool emitEvenIfUnchanged) {
    if (!err.isEmpty()) {
        if (emitEvenIfUnchanged && lastFriends_.isEmpty()) {
            emit friendListLoaded({}, err);
        }
        return;
    }
    if (changed) {
        lastFriends_ = ToFriendEntries(friends);
        emit friendListLoaded(lastFriends_, QString());
        return;
    }
    if (emitEvenIfUnchanged) {
        emit friendListLoaded(lastFriends_, QString());
    }
}
