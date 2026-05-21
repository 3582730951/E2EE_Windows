#include "GroupChatWindow.h"

#include <QLabel>
#include <QVBoxLayout>

#include "../common/Theme.h"
#include "../common/UiSettings.h"

GroupChatWindow::GroupChatWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(720, 480);
    setMinimumSize(560, 360);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);

    auto *title = new QLabel(
        UiSettings::Tr(QStringLiteral("旧 QWidget 群聊入口已停用"),
                       QStringLiteral("Legacy QWidget group chat entrypoint is disabled")),
        central);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;")
                             .arg(Theme::uiTextMain().name()));

    auto *message = new QLabel(
        UiSettings::Tr(QStringLiteral("请使用 Qt Quick 客户端入口 mi_e2ee_client_ui_app。"),
                       QStringLiteral("Use the Qt Quick client entrypoint mi_e2ee_client_ui_app.")),
        central);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    message->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
                               .arg(Theme::uiTextSub().name()));

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(message);
    layout->addStretch();
    setCentralWidget(central);
}
