#include <QApplication>
#include <QDebug>

#include "common/UiRuntimePaths.h"

int main(int argc, char* argv[]) {
    UiRuntimePaths::Prepare(argv[0]);
    QCoreApplication::setOrganizationName(QStringLiteral("mi_e2ee"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mi.e2ee"));
    QCoreApplication::setApplicationName(QStringLiteral("mi_e2ee_legacy_widget_disabled"));
    QApplication app(argc, argv);
    qCritical() << "Legacy QWidget entrypoint is disabled. Use mi_e2ee_client_ui_app.";
    return 2;
}
