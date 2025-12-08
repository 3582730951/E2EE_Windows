#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "quick_client.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);

  mi::client::ui::QuickClient client;
  client.init(QStringLiteral("client_config.ini"));

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("clientBridge", &client);
  // QML 已编译进资源；优先从 qrc 加载，若后续需要本地覆盖可改为检查文件存在。
  const QUrl url(QStringLiteral("qrc:/qt/qml/mi/e2ee/ui/Main.qml"));
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                   [url](QObject* obj, const QUrl& objUrl) {
                     if (!obj && url == objUrl) {
                       QCoreApplication::exit(-1);
                     }
                   },
                   Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
