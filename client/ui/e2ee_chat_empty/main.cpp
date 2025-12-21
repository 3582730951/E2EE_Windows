#include <QApplication>
#include <QGuiApplication>

#include "../common/Theme.h"
#include "../common/SecureClipboard.h"
#include "../common/UiSettings.h"
#include "ChatEmptyWindow.h"

#include "endpoint_hardening.h"

int main(int argc, char *argv[]) {
    mi::client::security::StartEndpointHardening();

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    SecureClipboard::Install(app);

    UiSettings::Load();
    UiSettings::ApplyToApp(app);

    ChatEmptyWindow window;
    window.show();
    return app.exec();
}
