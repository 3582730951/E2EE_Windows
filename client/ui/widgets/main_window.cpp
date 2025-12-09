#include "main_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

MainWindow::MainWindow(const UiPalette& palette, QWidget* parent)
    : QMainWindow(parent), palette_(palette) {
    setWindowTitle(tr("MI E2EE Client"));
    resize(1280, 780);

    central_ = new QWidget(this);
    central_->setAutoFillBackground(true);
    QPalette pal = central_->palette();
    pal.setColor(QPalette::Window, palette_.background);
    central_->setPalette(pal);

    auto* rootLayout = new QHBoxLayout(central_);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    buildLeftPanel(rootLayout);
    buildMiddlePanel(rootLayout);
    buildRightPanel(rootLayout);

    setCentralWidget(central_);
    populateGroups();
}

void MainWindow::buildLeftPanel(QHBoxLayout* rootLayout) {
    auto* panel = new QFrame(central_);
    panel->setObjectName(QStringLiteral("Panel"));
    panel->setFixedWidth(300);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto addTitle = [&](const QString& text) {
        auto* lbl = new QLabel(text, panel);
        lbl->setStyleSheet(QStringLiteral("font-weight:700; color:%1; font-size:14px;")
                               .arg(palette_.textPrimary.name()));
        layout->addWidget(lbl);
    };

    auto createDivider = [&]() {
        auto* line = new QFrame(panel);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(QStringLiteral("color:#22222c; background:#22222c; height:1px;"));
        return line;
    };

    addTitle(tr("群组"));
    groupEdit_ = new QLineEdit(panel);
    groupEdit_->setPlaceholderText(tr("群组 ID"));
    groupEdit_->setMinimumHeight(36);
    layout->addWidget(groupEdit_);

    auto* joinBtn = new QPushButton(tr("加入群"), panel);
    joinBtn->setMinimumHeight(36);
    joinBtn->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:6px;")
                               .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
    layout->addWidget(joinBtn);

    layout->addWidget(createDivider());

    addTitle(tr("离线/文件"));
    auto* uploadBtn = new QPushButton(tr("文件上传（占位）"), panel);
    uploadBtn->setMinimumHeight(36);
    uploadBtn->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:6px;")
                                 .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
    layout->addWidget(uploadBtn);

    auto* pullOfflineBtn = new QPushButton(tr("拉取离线（占位）"), panel);
    pullOfflineBtn->setMinimumHeight(36);
    pullOfflineBtn->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:6px;")
                                      .arg(palette_.accent.name(), palette_.textPrimary.name()));
    layout->addWidget(pullOfflineBtn);

    layout->addStretch(1);
    rootLayout->addWidget(panel, 0);
}

void MainWindow::buildMiddlePanel(QHBoxLayout* rootLayout) {
    auto* panel = new QFrame(central_);
    panel->setObjectName(QStringLiteral("Panel"));
    panel->setFixedWidth(250);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    conversationList_ = new QListWidget(panel);
    conversationList_->setFrameShape(QFrame::NoFrame);
    conversationList_->setSpacing(8);
    conversationList_->setSelectionMode(QAbstractItemView::SingleSelection);
    conversationList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(conversationList_, 1);

    rootLayout->addWidget(panel, 0);
}

void MainWindow::buildRightPanel(QHBoxLayout* rootLayout) {
    auto* panel = new QFrame(central_);
    panel->setObjectName(QStringLiteral("Panel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    chatWindow_ = new ChatWindow(palette_, panel);
    layout->addWidget(chatWindow_);

    rootLayout->addWidget(panel, 1);
}

void MainWindow::populateGroups() {
    if (!conversationList_) {
        return;
    }
    struct ItemData {
        QString title;
        bool online;
        bool highlight;
    };
    const QList<ItemData> items = {
        {tr("全局公告"), true, false},
        {tr("安全群"), true, true},
        {tr("工作群"), false, false},
    };
    for (const auto& it : items) {
        auto* item = new QListWidgetItem(it.title, conversationList_);
        item->setSizeHint(QSize(220, 48));
        conversationList_->addItem(item);

        QWidget* wrapper = new QWidget(conversationList_);
        auto* row = new QHBoxLayout(wrapper);
        row->setContentsMargins(10, 8, 10, 8);
        row->setSpacing(10);

        auto* indicator = new QLabel(wrapper);
        indicator->setFixedSize(8, 8);
        indicator->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;")
                                     .arg(it.online ? QStringLiteral("#4caf50")
                                                    : QStringLiteral("#666870")));
        row->addWidget(indicator, 0, Qt::AlignVCenter);

        auto* name = new QLabel(it.title, wrapper);
        name->setStyleSheet(QStringLiteral("color:%1; font-weight:%2;")
                                .arg(palette_.textPrimary.name(), it.highlight ? "700" : "500"));
        row->addWidget(name, 1);

        wrapper->setLayout(row);
        if (it.highlight) {
            wrapper->setStyleSheet(QStringLiteral("background:#1a1a2e; border-radius:6px;"));
        }
        conversationList_->setItemWidget(item, wrapper);
    }
    conversationList_->setCurrentRow(1);
}

void MainWindow::setCurrentUser(const QString& user) {
    setWindowTitle(tr("MI E2EE Client - %1").arg(user));
}

void MainWindow::openConversation(const QString& title) {
    if (!title.isEmpty()) {
        setWindowTitle(tr("MI E2EE Client - %1").arg(title));
    }
    show();
    raise();
    activateWindow();
}

}  // namespace mi::client::ui::widgets
