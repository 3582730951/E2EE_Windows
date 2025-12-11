#include <QApplication>
#include <QDebug>

#include "../common/Theme.h"
#include "BackendAdapter.h"
#include "LoginDialog.h"
#include "MainListWindow.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QApplication::setFont(Theme::defaultFont(10));

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
