#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>

#include "../common/Theme.h"
#include "../common/SecureClipboard.h"
#include "../common/UiRuntimePaths.h"
#include "../common/UiSettings.h"
#include "GroupChatWindow.h"

#include "endpoint_hardening.h"

int main(int argc, char *argv[]) {
    mi::client::security::StartEndpointHardening();
    UiRuntimePaths::Prepare(argv[0]);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QCoreApplication::setOrganizationName(QStringLiteral("mi_e2ee"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mi.e2ee"));
    QCoreApplication::setApplicationName(QStringLiteral("mi_e2ee_ui"));
    QApplication app(argc, argv);
    SecureClipboard::Install(app);

    const auto settings = UiSettings::Load();
    UiSettings::ApplyToApp(app);

    QMessageBox::critical(
        nullptr,
        QStringLiteral("Legacy QWidget entrypoint disabled"),
        UiSettings::Tr(QStringLiteral("旧 QWidget 群聊入口已停用，请使用 Qt Quick 客户端。"),
                       QStringLiteral("Legacy QWidget group chat entrypoint is disabled. Use the Qt Quick client.")));
    return 2;
}
