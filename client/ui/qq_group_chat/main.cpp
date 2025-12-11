#include <QApplication>

#include "../common/Theme.h"
#include "GroupChatWindow.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QApplication::setFont(Theme::defaultFont(10));

    GroupChatWindow window;
    window.show();
    return app.exec();
}
