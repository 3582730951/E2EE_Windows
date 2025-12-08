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
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString localMain = appDir + "/qml/Main.qml";
  if (QFile::exists(localMain)) {
    engine.addImportPath(appDir + "/qml");
  }
  const QUrl url = QFile::exists(localMain)
                       ? QUrl::fromLocalFile(localMain)
                       : QUrl(QStringLiteral("qrc:/qt/qml/mi/e2ee/ui/Main.qml"));
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
