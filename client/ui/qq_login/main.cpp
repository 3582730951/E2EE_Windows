#include <QApplication>

#include "../common/Theme.h"
#include "../common/UiSettings.h"
#include "LoginWindow.h"

#include "endpoint_hardening.h"

namespace {
struct EarlyEndpointHardening {
    EarlyEndpointHardening() noexcept { mi::client::security::StartEndpointHardening(); }
};

EarlyEndpointHardening gEarlyEndpointHardening;
}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    UiSettings::Load();
    UiSettings::ApplyToApp(app);

    LoginWindow window;
    window.show();
    return app.exec();
}
