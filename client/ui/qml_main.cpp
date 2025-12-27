#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "quick_client.h"
#include "common/UiRuntimePaths.h"

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    UiRuntimePaths::Prepare(argv[0]);
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MI"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mi-e2ee.local"));
    QCoreApplication::setApplicationName(QStringLiteral("MI E2EE Client"));

    QQmlApplicationEngine engine;
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);

    const QUrl url(QStringLiteral("qrc:/mi/e2ee/ui/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl) {
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
}
