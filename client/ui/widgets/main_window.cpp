#include "main_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

MainWindow::MainWindow(const UiPalette& palette, QWidget* parent)
    : QMainWindow(parent), palette_(palette) {
    setWindowTitle(tr("MI E2EE Client"));
    resize(1180, 760);

    central_ = new QWidget(this);
    central_->setAutoFillBackground(true);
    QPalette pal = central_->palette();
    pal.setColor(QPalette::Window, palette_.background);
    central_->setPalette(pal);

    auto* rootLayout = new QHBoxLayout(central_);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    buildChatOnly(rootLayout);

    setCentralWidget(central_);
}

void MainWindow::buildChatOnly(QHBoxLayout* rootLayout) {
    auto* panel = new QFrame(central_);
    panel->setObjectName(QStringLiteral("Panel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    chatWindow_ = new ChatWindow(palette_, panel, false);
    layout->addWidget(chatWindow_);

    rootLayout->addWidget(panel, 1);
}

void MainWindow::setCurrentUser(const QString& user) {
    setWindowTitle(tr("MI E2EE Client - %1").arg(user));
}

void MainWindow::openConversation(const QString& title) {
    if (!title.isEmpty()) {
        setWindowTitle(tr("MI E2EE Client - %1").arg(title));
    }
    if (chatWindow_) {
        chatWindow_->setGroupName(title);
    }
    show();
    raise();
    activateWindow();
}

}  // namespace mi::client::ui::widgets
