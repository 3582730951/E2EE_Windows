#include "main_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidgetItem>
#include <QPalette>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

MainWindow::MainWindow(const UiPalette& palette, QWidget* parent)
    : QMainWindow(parent), palette_(palette) {
    setWindowTitle(tr("MI E2EE Client"));
    resize(1280, 780);

    central_ = new QWidget(this);
    central_->setAutoFillBackground(true);
    QPalette pal = central_->palette();
    QLinearGradient grad(0, 0, 0, 720);
    grad.setColorAt(0.0, palette_.background);
    grad.setColorAt(1.0, palette_.panelMuted.darker(110));
    pal.setBrush(QPalette::Window, grad);
    central_->setPalette(pal);

    auto* rootLayout = new QHBoxLayout(central_);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    buildNavigation(rootLayout);
    buildConversations(rootLayout);
    buildChatArea(rootLayout);

    setCentralWidget(central_);
    populateConversations();
}

void MainWindow::buildNavigation(QHBoxLayout* rootLayout) {
    auto* navContainer = new QFrame(central_);
    navContainer->setObjectName(QStringLiteral("Panel"));
    navContainer->setFixedWidth(100);
    auto* navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(12, 12, 12, 12);
    navLayout->setSpacing(12);
    navLayout->setAlignment(Qt::AlignTop);

    contactsBtn_ = new NavigationButton(tr("联系人"),
                                        style()->standardIcon(QStyle::SP_FileDialogListView),
                                        navContainer);
    contactsBtn_->setChecked(true);
    contactsBtn_->setUnreadCount(2);
    navLayout->addWidget(contactsBtn_);

    groupsBtn_ =
        new NavigationButton(tr("群组"), style()->standardIcon(QStyle::SP_DirIcon), navContainer);
    groupsBtn_->setUnreadCount(5);
    navLayout->addWidget(groupsBtn_);

    filesBtn_ = new NavigationButton(tr("文件"), style()->standardIcon(QStyle::SP_DriveHDIcon),
                                     navContainer);
    filesBtn_->setUnreadCount(0);
    navLayout->addWidget(filesBtn_);

    navLayout->addStretch(1);

    rootLayout->addWidget(navContainer, 0);
}

void MainWindow::buildConversations(QHBoxLayout* rootLayout) {
    auto* column = new QFrame(central_);
    column->setObjectName(QStringLiteral("Panel"));
    column->setMinimumWidth(320);
    column->setMaximumWidth(380);
    auto* v = new QVBoxLayout(column);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(10);

    auto* search = new QLineEdit(column);
    search->setPlaceholderText(tr("搜索联系人 / 群组"));
    search->setClearButtonEnabled(true);
    v->addWidget(search);

    conversationList_ = new QListWidget(column);
    conversationList_->setFrameShape(QFrame::NoFrame);
    conversationList_->setSpacing(8);
    conversationList_->setSelectionMode(QAbstractItemView::SingleSelection);
    conversationList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->addWidget(conversationList_, 1);

    connect(conversationList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || !conversationList_) {
            return;
        }
        auto* item = conversationList_->item(row);
        if (!item) {
            return;
        }
        auto* widget = qobject_cast<ConversationItem*>(conversationList_->itemWidget(item));
        if (widget && chatWindow_) {
            chatWindow_->setGroupName(widget->title());
        }
    });

    rootLayout->addWidget(column, 0);
}

void MainWindow::buildChatArea(QHBoxLayout* rootLayout) {
    auto* area = new QFrame(central_);
    area->setObjectName(QStringLiteral("Panel"));
    auto* areaLayout = new QHBoxLayout(area);
    areaLayout->setContentsMargins(12, 12, 12, 12);
    areaLayout->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, area);
    splitter->setHandleWidth(2);

    chatWindow_ = new ChatWindow(palette_, splitter);
    memberPanel_ = new MemberPanel(palette_, splitter);
    memberPanel_->setMinimumWidth(240);
    memberPanel_->setMaximumWidth(320);

    splitter->addWidget(chatWindow_);
    splitter->addWidget(memberPanel_);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    areaLayout->addWidget(splitter);
    rootLayout->addWidget(area, 1);
}

void MainWindow::populateConversations() {
    if (!conversationList_) {
        return;
    }
    struct ItemData {
        QString title;
        QString summary;
        QString time;
        int unread;
    };
    const QList<ItemData> items = {
        {tr("安全群"), tr("@全体成员 今晚 9 点准时轮换密钥"), QStringLiteral("09:36"), 3},
        {tr("产品讨论"), tr("上线 Checklist 已同步，等待确认"), QStringLiteral("08:20"), 1},
        {tr("文件分发"), tr("离线文件准备完成，待推送"), QStringLiteral("昨天"), 0},
        {tr("演示群"), tr("自动示例：消息收发展示"), QStringLiteral("昨天"), 12},
    };
    for (const auto& it : items) {
        auto* widget = new ConversationItem(it.title, it.summary, it.time, it.unread, palette_,
                                            conversationList_);
        auto* item = new QListWidgetItem(conversationList_);
        item->setSizeHint(widget->sizeHint());
        conversationList_->addItem(item);
        conversationList_->setItemWidget(item, widget);
    }
    if (conversationList_->count() > 0) {
        conversationList_->setCurrentRow(0);
    }
}

void MainWindow::setCurrentUser(const QString& user) {
    setWindowTitle(tr("MI E2EE Client - %1").arg(user));
}

}  // namespace mi::client::ui::widgets
