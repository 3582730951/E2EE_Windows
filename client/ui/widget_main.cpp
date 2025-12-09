#include <QApplication>

#include "widgets/login_dialog.h"
#include "widgets/main_window.h"
#include "widgets/theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    auto palette = mi::client::ui::widgets::DefaultPalette();
    app.setStyleSheet(mi::client::ui::widgets::BuildGlobalStyleSheet(palette));

    mi::client::ui::widgets::LoginDialog login(palette);
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    mi::client::ui::widgets::MainWindow window(palette);
    window.setCurrentUser(login.username());
    window.show();
    return app.exec();
}
