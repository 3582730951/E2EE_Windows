#include <QApplication>

#include "../common/Theme.h"
#include "LoginWindow.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QApplication::setFont(Theme::defaultFont(10));

    LoginWindow window;
    window.show();
    return app.exec();
}
