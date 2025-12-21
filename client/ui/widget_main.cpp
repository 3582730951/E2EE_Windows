#include <QApplication>

#include "common/SecureClipboard.h"
#include "common/UiSettings.h"
#include "widgets/list_window.h"
#include "widgets/login_dialog.h"
#include "widgets/main_window.h"
#include "widgets/theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    SecureClipboard::Install(app);
    const auto settings = UiSettings::Load();
    if (auto *clip = SecureClipboard::instance()) {
        clip->setSystemClipboardWriteEnabled(!settings.secureClipboard);
    }
    auto palette = mi::client::ui::widgets::DefaultPalette();
    app.setStyleSheet(mi::client::ui::widgets::BuildGlobalStyleSheet(palette));

    mi::client::ui::widgets::LoginDialog login(palette);
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    mi::client::ui::widgets::MainWindow mainWindow(palette);
    mainWindow.setCurrentUser(login.username());

    QVector<mi::client::ui::widgets::ListEntry> combined = {
        {QStringLiteral("alice"), QObject::tr("Alice"), QObject::tr("在线"),
         QColor(QStringLiteral("#4caf50")),
         QDateTime::currentDateTime().addSecs(-60)},
        {QStringLiteral("security"), QObject::tr("安全群"), QObject::tr("端到端加密"),
         QColor(QStringLiteral("#1f6bff")),
         QDateTime::currentDateTime().addSecs(-10)},
        {QStringLiteral("work"), QObject::tr("工作群"), QObject::tr("未读 3"),
         QColor(QStringLiteral("#ff9800")),
         QDateTime::currentDateTime().addSecs(-30)},
        {QStringLiteral("bob"), QObject::tr("Bob"), QObject::tr("离线"),
         QColor(QStringLiteral("#666870")),
         QDateTime::currentDateTime().addSecs(-300)},
    };

    mi::client::ui::widgets::ListWindow listWindow(QObject::tr("好友/群聊"), combined, palette);
    QObject::connect(&listWindow, &mi::client::ui::widgets::ListWindow::entrySelected,
                     &mainWindow, &mi::client::ui::widgets::MainWindow::openConversation);
    listWindow.show();
    return app.exec();
}
