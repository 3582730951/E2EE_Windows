#ifndef MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
#define MI_E2EE_CLIENT_UI_QUICK_CLIENT_H

#include <QObject>
#include <QString>

#include "client_core.h"

namespace mi::client::ui {

// 轻量桥接：Qt Quick 与 client_core 的同步调用
class QuickClient : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
  Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY tokenChanged)

 public:
  explicit QuickClient(QObject* parent = nullptr);
  ~QuickClient() override;

  Q_INVOKABLE bool init(const QString& configPath);
  Q_INVOKABLE bool login(const QString& user, const QString& pass);
  Q_INVOKABLE void logout();
  Q_INVOKABLE bool joinGroup(const QString& groupId);
  Q_INVOKABLE bool sendGroupMessage(const QString& groupId, quint32 threshold);
  Q_INVOKABLE QStringList groupList() const;
  Q_INVOKABLE QString serverInfo() const;
  Q_INVOKABLE QString version() const;
  Q_INVOKABLE bool dummySendFile(const QString& groupId, const QString& path);
  Q_INVOKABLE QStringList pullOfflineDummy(const QString& groupId);
  Q_INVOKABLE void clearMessages();

  QString token() const;
  bool loggedIn() const;

 signals:
  void tokenChanged();
  void status(const QString& message);

 private:
  QString config_path_{"client_config.ini"};
  mi::client::ClientCore core_;
  QString token_;
};

}  // namespace mi::client::ui

#endif  // MI_E2EE_CLIENT_UI_QUICK_CLIENT_H
