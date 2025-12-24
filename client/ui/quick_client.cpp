#include "quick_client.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <utility>
#include <fstream>

#include "common/UiRuntimePaths.h"

namespace mi::client::ui {

namespace {
QString FindConfigFile(const QString& name) {
  if (name.isEmpty()) {
    return {};
  }
  const QFileInfo info(name);
  const QString appRoot = UiRuntimePaths::AppRootDir();
  const QString baseDir = appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
  if (info.isAbsolute()) {
    return QFile::exists(name) ? name : QString();
  }
  if (info.path() != QStringLiteral(".") && !info.path().isEmpty()) {
    const QString candidate = baseDir + QStringLiteral("/") + name;
    if (QFile::exists(candidate)) {
      return candidate;
    }
    if (QFile::exists(name)) {
      return QFileInfo(name).absoluteFilePath();
    }
    return {};
  }
  const QString in_config = baseDir + QStringLiteral("/config/") + name;
  if (QFile::exists(in_config)) {
    return in_config;
  }
  const QString in_app = baseDir + QStringLiteral("/") + name;
  if (QFile::exists(in_app)) {
    return in_app;
  }
  if (QFile::exists(name)) {
    return QFileInfo(name).absoluteFilePath();
  }
  return {};
}
}  // namespace

QuickClient::QuickClient(QObject* parent) : QObject(parent) {}

QuickClient::~QuickClient() {
  core_.Logout();
}

bool QuickClient::init(const QString& configPath) {
  if (!configPath.isEmpty()) {
    config_path_ = configPath;
  } else {
    config_path_ = FindConfigFile(QStringLiteral("config/client_config.ini"));
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("client_config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("config.ini"));
    }
    if (config_path_.isEmpty()) {
      const QString appRoot = UiRuntimePaths::AppRootDir();
      const QString baseDir = appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
      config_path_ = baseDir +
                     QStringLiteral("/config/client_config.ini");
    }
  }
  const bool ok = core_.Init(config_path_.toStdString());
  if (!ok) {
    emit status(QStringLiteral("初始化失败"));
  }
  return ok;
}

bool QuickClient::login(const QString& user, const QString& pass) {
  const bool ok = core_.Login(user.toStdString(), pass.toStdString());
  if (!ok) {
    emit status(QStringLiteral("登录失败"));
    token_.clear();
  } else {
    token_ = QString::fromStdString(core_.token());
    emit status(QStringLiteral("登录成功"));
  }
  emit tokenChanged();
  return ok;
}

void QuickClient::logout() {
  core_.Logout();
  token_.clear();
  emit tokenChanged();
  emit status(QStringLiteral("已登出"));
}

bool QuickClient::joinGroup(const QString& groupId) {
  const bool ok = core_.JoinGroup(groupId.toStdString());
  emit status(ok ? QStringLiteral("加入群成功") : QStringLiteral("加入群失败"));
  return ok;
}

bool QuickClient::sendGroupMessage(const QString& groupId, quint32 threshold) {
  const bool ok = core_.SendGroupMessage(groupId.toStdString(), threshold);
  emit status(ok ? QStringLiteral("消息已发送") : QStringLiteral("发送失败"));
  return ok;
}

QStringList QuickClient::groupList() const {
  // 当前后端未返回群列表，这里仅返回占位
  return {};
}

QString QuickClient::serverInfo() const {
  return QStringLiteral("config: %1").arg(config_path_);
}

QString QuickClient::version() const {
  return QStringLiteral("UI demo 1.0");
}

bool QuickClient::dummySendFile(const QString& groupId, const QString& path) {
  if (groupId.isEmpty()) {
    emit status(QStringLiteral("请选择群组后再上传"));
    return false;
  }
  std::vector<std::uint8_t> data;
  if (!path.isEmpty()) {
    std::ifstream ifs(path.toStdString(), std::ios::binary);
    if (!ifs) {
      emit status(QStringLiteral("读取文件失败"));
      return false;
    }
    data.assign(std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>());
  } else {
    data = {'F', 'I', 'L', 'E'};
  }
  const bool ok = core_.SendOffline(groupId.toStdString(), data);
  emit status(ok ? QStringLiteral("已投递文件到: ") + groupId
                 : QStringLiteral("文件上传失败"));
  return ok;
}

QStringList QuickClient::pullOfflineDummy(const QString& groupId) {
  auto msgs = core_.PullOffline();
  QStringList out;
  for (const auto& m : msgs) {
    out << QString::fromLatin1(reinterpret_cast<const char*>(m.data()),
                               static_cast<int>(m.size()));
  }
  emit status(QStringLiteral("离线消息条数: %1").arg(out.size()));
  return out;
}

void QuickClient::clearMessages() {
  emit status(QStringLiteral("消息列表已清空（本地）"));
}

QString QuickClient::token() const {
  return token_;
}

bool QuickClient::loggedIn() const {
  return !token_.isEmpty();
}

}  // namespace mi::client::ui
