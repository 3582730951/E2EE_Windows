#include "MainListWindow.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTime>
#include <QMenu>

#include "../common/IconButton.h"
#include "../common/Theme.h"
#include "ChatWindow.h"
#include "BackendAdapter.h"

namespace {

enum Roles {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    PreviewRole,
    TimeRole,
    UnreadRole,
    GreyBadgeRole,
    HasTagRole
};

struct Tokens {
    static QColor windowBg() { return QColor("#14161A"); }
    static QColor panelBg() { return QColor("#191C20"); }
    static QColor sidebarBg() { return QColor("#1D2025"); }
    static QColor hoverBg() { return QColor("#20242A"); }
    static QColor selectedBg() { return QColor("#262B32"); }
    static QColor searchBg() { return QColor("#1F2227"); }
    static QColor textMain() { return QColor("#F0F2F5"); }
    static QColor textSub() { return QColor("#A9ADB3"); }
    static QColor textMuted() { return QColor("#7C8087"); }
    static QColor tagColor() { return QColor("#E36A5C"); }
    static QColor badgeRed() { return QColor("#D74D4D"); }
    static QColor badgeGrey() { return QColor("#464A50"); }
    static QColor accentBlue() { return QColor("#5D8CFF"); }
    static int sidebarWidth() { return 78; }
    static int rowHeight() { return 74; }
    static int radius() { return 10; }
};

QColor avatarColorFor(const QString &seed) {
    const uint hash = qHash(seed);
    int r = 80 + (hash & 0x7F);
    int g = 90 + ((hash >> 8) & 0x7F);
    int b = 110 + ((hash >> 16) & 0x7F);
    return QColor(r, g, b);
}

class ConversationDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return QSize(0, Tokens::rowHeight());
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        painter->save();
        QRect r = option.rect.adjusted(8, 4, -8, -4);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        QColor bg = Tokens::windowBg();
        if (selected) {
            bg = Tokens::selectedBg();
        } else if (hovered) {
            bg = Tokens::hoverBg();
        }
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(r, 10, 10);

        const QString title = index.data(TitleRole).toString();
        const QString preview = index.data(PreviewRole).toString();
        const QString time = index.data(TimeRole).toString();
        const int unread = index.data(UnreadRole).toInt();
        const bool greyBadge = index.data(GreyBadgeRole).toBool();
        const bool hasTag = index.data(HasTagRole).toBool();

        // Avatar
        QRect avatarRect = QRect(r.left() + 12, r.top() + 10, 46, 46);
        painter->setBrush(avatarColorFor(title));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(avatarRect);

        // Text area
        int textLeft = avatarRect.right() + 12;
        QRect titleRect(textLeft, r.top() + 10, r.width() - textLeft - 80, 22);
        QRect previewRect(textLeft, titleRect.bottom() + 6, r.width() - textLeft - 90, 20);

        QFont titleFont = Theme::defaultFont(14, QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(Tokens::textMain());
        painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

        // Preview with optional tag highlight
        QFont previewFont = Theme::defaultFont(12, QFont::Normal);
        painter->setFont(previewFont);
        int x = previewRect.left();
        if (hasTag) {
            const QString tag = preview.startsWith('[') ? preview.section(']', 0, 0) + "]" : QStringLiteral("[有新文件]");
            const QString rest = preview.mid(tag.length()).trimmed();
            painter->setPen(Tokens::tagColor());
            QString tagDraw = painter->fontMetrics().elidedText(tag, Qt::ElideRight,
                                                                previewRect.width());
            painter->drawText(previewRect.translated(x - previewRect.left(), 0),
                              Qt::AlignVCenter | Qt::AlignLeft, tagDraw);
            x += painter->fontMetrics().horizontalAdvance(tagDraw) + 6;
            painter->setPen(Tokens::textSub());
            painter->drawText(QRect(x, previewRect.top(), previewRect.right() - x, previewRect.height()),
                              Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(rest, Qt::ElideRight,
                                                                previewRect.right() - x));
        } else {
            painter->setPen(Tokens::textSub());
            painter->drawText(previewRect, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(preview, Qt::ElideRight,
                                                                previewRect.width()));
        }

        // Time
        QFont timeFont = Theme::defaultFont(11, QFont::Normal);
        painter->setFont(timeFont);
        painter->setPen(Tokens::textMuted());
        QRect timeRect(r.right() - 64, r.top() + 12, 60, 16);
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, time);

        // Badge
        if (unread > 0) {
            QString badgeText = unread > 99 ? QStringLiteral("99+") : QString::number(unread);
            QFont badgeFont = Theme::defaultFont(11, QFont::DemiBold);
            painter->setFont(badgeFont);
            QRect badgeRect = painter->fontMetrics().boundingRect(badgeText);
            badgeRect.adjust(0, 0, 10, 6);
            badgeRect.moveTo(r.right() - badgeRect.width() - 14, previewRect.top() + 2);
            painter->setBrush(greyBadge ? Tokens::badgeGrey() : Tokens::badgeRed());
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(badgeRect, badgeRect.height() / 2.0, badgeRect.height() / 2.0);
            painter->setPen(Qt::white);
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
        }

        painter->restore();
    }
};

IconButton *titleButton(const QString &glyph, QWidget *parent, const QColor &fg) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(26, 26);
    btn->setColors(fg, QColor("#FFFFFF"), QColor("#E0E0E0"), QColor(0, 0, 0, 0),
                   QColor(255, 255, 255, 18), QColor(255, 255, 255, 32));
    return btn;
}

IconButton *navButton(const QString &glyph, QWidget *parent, bool selected = false) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(44, 44);
    QColor baseBg = selected ? Tokens::hoverBg() : QColor(0, 0, 0, 0);
    btn->setColors(QColor("#D6D9DF"), QColor("#FFFFFF"), QColor("#D0D0D0"), baseBg,
                   Tokens::hoverBg(), Tokens::selectedBg());
    btn->setRound(true);
    return btn;
}

void addBadgeDot(QWidget *anchor, const QString &text = QString()) {
    if (!anchor) {
        return;
    }
    auto *badge = new QLabel(anchor);
    if (text.isEmpty()) {
        badge->setFixedSize(8, 8);
        badge->setStyleSheet("background: #D74D4D; border-radius: 4px;");
        badge->move(anchor->width() - 12, 6);
    } else {
        badge->setStyleSheet(
            "color: white; background: #D74D4D; border-radius: 10px; padding: 1px 6px; font-size: 10px;");
        badge->adjustSize();
        badge->move(anchor->width() - badge->width() + 2, 4);
    }
    badge->raise();
    badge->show();
}

}  // namespace

MainListWindow::MainListWindow(BackendAdapter *backend, QWidget *parent)
    : FramelessWindowBase(parent), backend_(backend) {
    resize(473, 827);
    setMinimumSize(473, 827);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Title bar with custom buttons.
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(44);
    titleBar->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::windowBg().name()));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 8, 10, 8);

    auto *titleLabel = new QLabel(QStringLiteral("QQ"), titleBar);
    titleLabel->setStyleSheet("color: #D6D9DF; font-size: 12px; letter-spacing: 1px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    auto *funcBtn = titleButton(QStringLiteral("\u25A1"), titleBar, QColor("#B7BBC2"));
    auto *minBtn = titleButton(QStringLiteral("\u2212"), titleBar, QColor("#B7BBC2"));
    auto *closeBtn = titleButton(QStringLiteral("\u2715"), titleBar, QColor("#B7BBC2"));
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(funcBtn);
    titleLayout->addSpacing(6);
    titleLayout->addWidget(minBtn);
    titleLayout->addWidget(closeBtn);
    rootLayout->addWidget(titleBar);
    setTitleBar(titleBar);

    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left sidebar
    auto *sidebar = new QWidget(body);
    sidebar->setFixedWidth(Tokens::sidebarWidth());
    sidebar->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::sidebarBg().name()));
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 12, 10, 12);
    sideLayout->setSpacing(14);

    auto *qqMark = new QLabel(QStringLiteral("QQ"), sidebar);
    qqMark->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    qqMark->setStyleSheet("color: #D6D9DF; font-size: 12px;");
    sideLayout->addWidget(qqMark, 0, Qt::AlignLeft);

    auto *bell = navButton(QStringLiteral("!"), sidebar, false);
    bell->setFixedSize(32, 32);
    sideLayout->addWidget(bell, 0, Qt::AlignLeft);

    auto *avatar = new QLabel(sidebar);
    avatar->setFixedSize(46, 46);
    avatar->setStyleSheet("background: #5D8CFF; border-radius: 23px;");
    sideLayout->addWidget(avatar, 0, Qt::AlignLeft);

    auto *sessionBtn = navButton(QStringLiteral("\u2630"), sidebar, true);
    addBadgeDot(sessionBtn, QStringLiteral("99+"));
    sideLayout->addWidget(sessionBtn, 0, Qt::AlignLeft);

    auto *starBtn = navButton(QStringLiteral("\u2605"), sidebar, false);
    addBadgeDot(starBtn, QString());
    sideLayout->addWidget(starBtn, 0, Qt::AlignLeft);

    sideLayout->addWidget(navButton(QStringLiteral("\u2709"), sidebar), 0, Qt::AlignLeft);
    sideLayout->addWidget(navButton(QStringLiteral("F"), sidebar), 0, Qt::AlignLeft);
    sideLayout->addWidget(navButton(QStringLiteral("\u2699"), sidebar), 0, Qt::AlignLeft);
    sideLayout->addStretch();

    auto *menuBtn = navButton(QStringLiteral("\u2630"), sidebar);
    sideLayout->addWidget(menuBtn, 0, Qt::AlignLeft | Qt::AlignBottom);

    // Right main area
    auto *mainArea = new QWidget(body);
    mainArea->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::windowBg().name()));
    auto *mainLayout2 = new QVBoxLayout(mainArea);
    mainLayout2->setContentsMargins(12, 12, 12, 12);
    mainLayout2->setSpacing(10);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);

    auto *searchBox = new QFrame(mainArea);
    searchBox->setFixedHeight(36);
    searchBox->setStyleSheet(
        QStringLiteral("QFrame { background: %1; border-radius: 18px; border: 1px solid #1F2227; }"
                       "QLineEdit { background: transparent; border: none; color: #E6E6E6; font-size: 13px; }"
                       "QLabel { color: #8C9096; font-size: 13px; }")
            .arg(Tokens::searchBg().name()));
    auto *sLayout = new QHBoxLayout(searchBox);
    sLayout->setContentsMargins(12, 6, 12, 6);
    sLayout->setSpacing(8);
    auto *searchIcon = new QLabel(QStringLiteral("Q"), searchBox);
    searchIcon->setStyleSheet("color: #8C9096; font-size: 12px; font-weight: 600;");
    searchEdit_ = new QLineEdit(searchBox);
    searchEdit_->setPlaceholderText(QStringLiteral("搜索"));
    connect(searchEdit_, &QLineEdit::textChanged, this, &MainListWindow::handleSearchTextChanged);
    sLayout->addWidget(searchIcon);
    sLayout->addWidget(searchEdit_, 1);

    auto *plusBtn = new IconButton(QStringLiteral("+"), mainArea);
    plusBtn->setFixedSize(36, 36);
    plusBtn->setColors(QColor("#D6D9DF"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                       Tokens::searchBg(), Tokens::hoverBg(), Tokens::selectedBg());
    connect(plusBtn, &QPushButton::clicked, this, &MainListWindow::handleAddFriend);

    searchRow->addWidget(searchBox, 1);
    searchRow->addWidget(plusBtn);
    mainLayout2->addLayout(searchRow);

    // Conversation list
    listView_ = new QListView(mainArea);
    listView_->setFrameShape(QFrame::NoFrame);
    listView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView_->setSpacing(0);
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setStyleSheet(
        "QListView { background: transparent; outline: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #2A2D33; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }");

    model_ = new QStandardItemModel(listView_);
    auto addRow = [&](const QString &id, const QString &title, const QString &preview, const QString &time, int unread,
                      bool greyBadge, bool hasTag) {
        auto *item = new QStandardItem();
        item->setData(id, IdRole);
        item->setData(title, TitleRole);
        item->setData(preview, PreviewRole);
        item->setData(time, TimeRole);
        item->setData(unread, UnreadRole);
        item->setData(greyBadge, GreyBadgeRole);
        item->setData(hasTag, HasTagRole);
        model_->appendRow(item);
    };

    if (backend_) {
        QString err;
        const auto friends = backend_->listFriends(err);
        if (!friends.isEmpty()) {
            for (const auto &f : friends) {
                addRow(f.username, f.displayName(), QStringLiteral("点击开始聊天"), QString(), 0, true, false);
            }
        } else {
            addRow(QStringLiteral("__placeholder__"),
                   QStringLiteral("暂无好友"),
                   err.isEmpty() ? QStringLiteral("点击右上角 + 添加好友") : err,
                   QString(), 0, true, false);
        }
    } else {
        addRow(QStringLiteral("__placeholder__"),
               QStringLiteral("暂无好友"),
               QStringLiteral("未连接后端，点击右上角 + 添加好友"),
               QString(), 0, true, false);
    }

    listView_->setModel(model_);
    listView_->setItemDelegate(new ConversationDelegate(listView_));
    if (model_->rowCount() > 0) {
        listView_->setCurrentIndex(model_->index(0, 0));
    }
    connect(listView_, &QListView::clicked, this, &MainListWindow::openChatForIndex);
    connect(listView_, &QListView::doubleClicked, this, &MainListWindow::openChatForIndex);
    connect(listView_, &QListView::activated, this, &MainListWindow::openChatForIndex);

    listView_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listView_, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!backend_ || !model_) {
            return;
        }
        const QModelIndex idx = listView_->indexAt(pos);
        if (!idx.isValid()) {
            return;
        }
        const QString id = idx.data(IdRole).toString();
        if (id.startsWith(QStringLiteral("__"))) {
            return;
        }
        QMenu menu(this);
        QAction *edit = menu.addAction(QStringLiteral("修改备注"));
        QAction *picked = menu.exec(listView_->viewport()->mapToGlobal(pos));
        if (picked != edit) {
            return;
        }
        bool ok = false;
        const QString current = idx.data(TitleRole).toString();
        const QString newRemark =
            QInputDialog::getText(this, QStringLiteral("修改备注"),
                                  QStringLiteral("输入备注（可留空）"),
                                  QLineEdit::Normal, current, &ok);
        if (!ok) {
            return;
        }
        QString err;
        if (!backend_->setFriendRemark(id, newRemark.trimmed(), err)) {
            QMessageBox::warning(this, QStringLiteral("修改备注"),
                                 err.isEmpty() ? QStringLiteral("修改失败") : err);
            return;
        }
        const QString display = newRemark.trimmed().isEmpty() ? id : newRemark.trimmed();
        if (auto *item = model_->itemFromIndex(idx)) {
            item->setData(display, TitleRole);
            item->setData(QStringLiteral("备注已更新"), PreviewRole);
            item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        }
    });

    mainLayout2->addWidget(listView_);

    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(mainArea, 1);

    rootLayout->addWidget(body);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_main_list.png"));

    if (backend_) {
        connect(backend_, &BackendAdapter::offlineMessage, this, &MainListWindow::handleOfflineMessage);
    }
}

void MainListWindow::openChatForIndex(const QModelIndex &index) {
    if (!index.isValid()) {
        return;
    }
    const QString id = index.data(IdRole).toString();
    if (id.startsWith(QStringLiteral("__"))) {
        return;
    }
    const QString title = index.data(TitleRole).toString();

    if (chatWindows_.contains(id) && chatWindows_[id]) {
        chatWindows_[id]->setConversation(id, title);
        chatWindows_[id]->show();
        chatWindows_[id]->raise();
        chatWindows_[id]->activateWindow();
        return;
    }

    ChatWindow *win = new ChatWindow(backend_);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setConversation(id, title);
    chatWindows_[id] = win;
    connect(win, &QObject::destroyed, this, [this, id]() { chatWindows_.remove(id); });
    win->show();
    win->raise();
    win->activateWindow();
}

void MainListWindow::handleAddFriend() {
    bool ok = false;
    const QString account =
        QInputDialog::getText(this, QStringLiteral("添加好友"), QStringLiteral("输入账号"), QLineEdit::Normal,
                              QString(), &ok);
    if (!ok || account.trimmed().isEmpty()) {
        return;
    }
    if (backend_) {
        const QString defaultRemark = account.trimmed();
        const QString remark =
            QInputDialog::getText(this, QStringLiteral("添加好友"),
                                  QStringLiteral("输入备注（可留空）"),
                                  QLineEdit::Normal, defaultRemark, &ok);
        if (!ok) {
            return;
        }
        QString err;
        if (backend_->addFriend(account.trimmed(), remark.trimmed(), err)) {
            for (int i = model_->rowCount() - 1; i >= 0; --i) {
                if (model_->item(i)->data(IdRole).toString().startsWith(QStringLiteral("__"))) {
                    model_->removeRow(i);
                }
            }
            // insert to top if not exists
            bool exists = false;
            for (int i = 0; i < model_->rowCount(); ++i) {
                if (model_->item(i)->data(IdRole).toString() == account.trimmed()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                const QString display = remark.trimmed().isEmpty() ? account.trimmed() : remark.trimmed();
                auto *item = new QStandardItem();
                item->setData(account.trimmed(), IdRole);
                item->setData(display, TitleRole);
                item->setData(QStringLiteral("已添加好友"), PreviewRole);
                item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
                item->setData(0, UnreadRole);
                item->setData(true, GreyBadgeRole);
                item->setData(false, HasTagRole);
                model_->insertRow(0, item);
                listView_->setCurrentIndex(model_->index(0, 0));
            }
            QMessageBox::information(this, QStringLiteral("添加好友"),
                                     QStringLiteral("已添加好友：%1").arg(account.trimmed()));
        } else {
            QMessageBox::warning(this, QStringLiteral("添加好友"),
                                 QStringLiteral("发送失败：%1").arg(err));
        }
    } else {
        QMessageBox::warning(this, QStringLiteral("添加好友"),
                             QStringLiteral("未连接后端"));
    }
}

void MainListWindow::handleSearchTextChanged(const QString &text) {
    // Placeholder: future backend filtering; currently no-op.
    Q_UNUSED(text);
}

void MainListWindow::handleOfflineMessage(const QString &convId, const QString &text, bool isFile) {
    Q_UNUSED(isFile);
    int rowIndex = -1;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (model_->item(i)->data(IdRole).toString() == convId) {
            rowIndex = i;
            break;
        }
    }
    if (rowIndex == -1) {
        auto *item = new QStandardItem();
        item->setData(convId, IdRole);
        item->setData(convId, TitleRole);
        item->setData(text, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(1, UnreadRole);
        item->setData(false, GreyBadgeRole);
        item->setData(false, HasTagRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(text, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendIncomingMessage(text, now);
        // 已打开窗口则视为已读
        if (rowIndex >= 0) {
            model_->item(rowIndex)->setData(0, UnreadRole);
        }
    }
}
