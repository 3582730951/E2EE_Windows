#include <QApplication>

#include "widgets/list_window.h"
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

    mi::client::ui::widgets::MainWindow mainWindow(palette);
    mainWindow.setCurrentUser(login.username());
    mainWindow.show();

    const QVector<mi::client::ui::widgets::ListEntry> friends = {
        {QObject::tr("Alice"), QObject::tr("在线"), QColor(QStringLiteral("#4caf50"))},
        {QObject::tr("Bob"), QObject::tr("离线"), QColor(QStringLiteral("#666870"))},
        {QObject::tr("Charlie"), QObject::tr("忙碌"), QColor(QStringLiteral("#ff9800"))},
    };
    const QVector<mi::client::ui::widgets::ListEntry> groups = {
        {QObject::tr("全局公告"), QObject::tr("公共频道"), QColor(QStringLiteral("#4caf50"))},
        {QObject::tr("安全群"), QObject::tr("端到端加密"), QColor(QStringLiteral("#1f6bff"))},
        {QObject::tr("工作群"), QObject::tr("未读 3"), QColor(QStringLiteral("#ff9800"))},
    };

    mi::client::ui::widgets::ListWindow friendWindow(QObject::tr("好友列表"), friends, palette);
    mi::client::ui::widgets::ListWindow groupWindow(QObject::tr("群聊列表"), groups, palette);
    friendWindow.show();
    groupWindow.show();

    return app.exec();
}
