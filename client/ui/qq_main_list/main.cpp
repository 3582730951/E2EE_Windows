#include <QApplication>

#include "../common/Theme.h"
#include "../common/UiSettings.h"
#include "BackendAdapter.h"
#include "LoginDialog.h"
#include "MainListWindow.h"

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

    BackendAdapter backend;
    backend.init();  // 尝试按默认 config.ini 初始化，失败时仍可继续尝试登录

    LoginDialog login(&backend);
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    MainListWindow window(&backend);
    window.show();
    return app.exec();
}
