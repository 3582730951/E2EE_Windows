#include <QApplication>

#include "../common/Theme.h"
#include "../common/UiSettings.h"
#include "ChatEmptyWindow.h"

#include "endpoint_hardening.h"

int main(int argc, char *argv[]) {
    mi::client::security::StartEndpointHardening();

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    UiSettings::Load();
    UiSettings::ApplyToApp(app);

    ChatEmptyWindow window;
    window.show();
    return app.exec();
}
