#include "MainListWindow.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTime>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QGuiApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <memory>

#include "../common/IconButton.h"
#include "../common/SettingsDialog.h"
#include "../common/Theme.h"
#include "../common/UiIcons.h"
#include "../common/UiSettings.h"
#include "../common/UiStyle.h"
#include "../common/Toast.h"
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
    HasTagRole,
    IsGroupRole
};

struct Tokens {
    static QColor windowBg() { return Theme::uiWindowBg(); }
    static QColor panelBg() { return Theme::uiPanelBg(); }
    static QColor sidebarBg() { return Theme::uiSidebarBg(); }
    static QColor hoverBg() { return Theme::uiHoverBg(); }
    static QColor selectedBg() { return Theme::uiSelectedBg(); }
    static QColor searchBg() { return Theme::uiSearchBg(); }
    static QColor textMain() { return Theme::uiTextMain(); }
    static QColor textSub() { return Theme::uiTextSub(); }
    static QColor textMuted() { return Theme::uiTextMuted(); }
    static QColor tagColor() { return Theme::uiTagColor(); }
    static QColor badgeRed() { return Theme::uiBadgeRed(); }
    static QColor badgeGrey() { return Theme::uiBadgeGrey(); }
    static QColor accentBlue() { return Theme::uiAccentBlue(); }
    static int sidebarWidth() { return 78; }
    static int rowHeight() { return 74; }
    static int radius() { return 10; }
};

bool LooksLikeImageFile(const QString &nameOrPath) {
    const QString lower = nameOrPath.trimmed().toLower();
    return lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".jpg")) ||
           lower.endsWith(QStringLiteral(".jpeg")) || lower.endsWith(QStringLiteral(".bmp")) ||
           lower.endsWith(QStringLiteral(".gif")) || lower.endsWith(QStringLiteral(".webp"));
}

bool LooksLikeAudioFile(const QString &nameOrPath) {
    const QString lower = nameOrPath.trimmed().toLower();
    return lower.endsWith(QStringLiteral(".wav")) || lower.endsWith(QStringLiteral(".mp3")) ||
           lower.endsWith(QStringLiteral(".m4a")) || lower.endsWith(QStringLiteral(".aac")) ||
           lower.endsWith(QStringLiteral(".ogg")) || lower.endsWith(QStringLiteral(".opus")) ||
           lower.endsWith(QStringLiteral(".flac"));
}

bool LooksLikeVideoFile(const QString &nameOrPath) {
    const QString lower = nameOrPath.trimmed().toLower();
    return lower.endsWith(QStringLiteral(".mp4")) || lower.endsWith(QStringLiteral(".mkv")) ||
           lower.endsWith(QStringLiteral(".mov")) || lower.endsWith(QStringLiteral(".webm")) ||
           lower.endsWith(QStringLiteral(".avi")) || lower.endsWith(QStringLiteral(".flv")) ||
           lower.endsWith(QStringLiteral(".m4v"));
}

QString FilePreviewTag(const QString &nameOrPath) {
    if (LooksLikeImageFile(nameOrPath)) {
        return UiSettings::Tr(QStringLiteral("[图片]"), QStringLiteral("[Image]"));
    }
    if (LooksLikeAudioFile(nameOrPath)) {
        return UiSettings::Tr(QStringLiteral("[语音]"), QStringLiteral("[Voice]"));
    }
    if (LooksLikeVideoFile(nameOrPath)) {
        return UiSettings::Tr(QStringLiteral("[视频]"), QStringLiteral("[Video]"));
    }
    return UiSettings::Tr(QStringLiteral("[文件]"), QStringLiteral("[File]"));
}

QColor avatarColorFor(const QString &seed) {
    const uint hash = qHash(seed);
    int r = 80 + (hash & 0x7F);
    int g = 90 + ((hash >> 8) & 0x7F);
    int b = 110 + ((hash >> 16) & 0x7F);
    return QColor(r, g, b);
}

#ifdef Q_OS_WIN
QString AutoStartValueName() {
    return QStringLiteral("MI_E2EE_Client_UI");
}

QString AutoStartRunKey() {
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

QString AutoStartCommandForCurrentExe() {
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\"").arg(exe);
}

bool IsAutoStartEnabled() {
    QSettings settings(AutoStartRunKey(), QSettings::NativeFormat);
    const QString value = settings.value(AutoStartValueName()).toString().trimmed();
    if (value.isEmpty()) {
        return false;
    }
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return value.contains(exe, Qt::CaseInsensitive);
}

bool SetAutoStartEnabled(bool enabled) {
    QSettings settings(AutoStartRunKey(), QSettings::NativeFormat);
    if (enabled) {
        settings.setValue(AutoStartValueName(), AutoStartCommandForCurrentExe());
    } else {
        settings.remove(AutoStartValueName());
    }
    settings.sync();
    return settings.status() == QSettings::NoError;
}
#endif

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
            const QString tag = preview.startsWith('[')
                                    ? preview.section(']', 0, 0) + "]"
                                    : UiSettings::Tr(QStringLiteral("[有新文件]"),
                                                     QStringLiteral("[New file]"));
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
    btn->setColors(fg, Tokens::textMain(), Tokens::textMain(), QColor(0, 0, 0, 0),
                   Tokens::hoverBg(), Tokens::selectedBg());
    return btn;
}

IconButton *titleButtonSvg(const QString &svgPath, QWidget *parent, const QColor &fg) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(26, 26);
    btn->setSvgIcon(svgPath, 16);
    btn->setColors(fg, Tokens::textMain(), Tokens::textMain(), QColor(0, 0, 0, 0),
                   Tokens::hoverBg(), Tokens::selectedBg());
    return btn;
}

IconButton *navButton(const QString &glyph, QWidget *parent, bool selected = false) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(44, 44);
    QColor baseBg = selected ? Tokens::hoverBg() : QColor(0, 0, 0, 0);
    btn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(), baseBg,
                   Tokens::hoverBg(), Tokens::selectedBg());
    btn->setRound(true);
    return btn;
}

IconButton *navButtonSvg(const QString &svgPath, QWidget *parent, bool selected = false) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(44, 44);
    btn->setSvgIcon(svgPath, 20);
    QColor baseBg = selected ? Tokens::hoverBg() : QColor(0, 0, 0, 0);
    btn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(), baseBg,
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
        badge->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 4px;")
                .arg(Theme::uiBadgeRed().name()));
        badge->move(anchor->width() - 12, 6);
    } else {
        badge->setFont(Theme::defaultFont(10, QFont::DemiBold));
        badge->setStyleSheet(QStringLiteral(
            "color: white; background: %1; border-radius: 10px; padding: 1px 6px;")
                                 .arg(Theme::uiBadgeRed().name()));
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
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; letter-spacing: 1px;")
                                  .arg(Tokens::textMain().name()));
    titleLayout->addWidget(titleLabel);
    connLabel_ = new QLabel(QStringLiteral(""), titleBar);
    connLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                  .arg(Tokens::textMuted().name()));
    titleLayout->addSpacing(10);
    titleLayout->addWidget(connLabel_);
    titleLayout->addStretch();
    auto *funcBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/maximize.svg"), titleBar, Tokens::textSub());
    auto *minBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/minimize.svg"), titleBar, Tokens::textSub());
    auto *closeBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), titleBar, Tokens::textSub());
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(funcBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
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
    qqMark->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                              .arg(Tokens::textMain().name()));
    sideLayout->addWidget(qqMark, 0, Qt::AlignLeft);

    auto *bell = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/bell.svg"), sidebar, false);
    bell->setFixedSize(32, 32);
    sideLayout->addWidget(bell, 0, Qt::AlignLeft);

    auto *avatar = new QLabel(sidebar);
    avatar->setFixedSize(46, 46);
    avatar->setStyleSheet(QStringLiteral("background: %1; border-radius: 23px;")
                              .arg(Tokens::accentBlue().name()));
    sideLayout->addWidget(avatar, 0, Qt::AlignLeft);

    auto *sessionBtn = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/chat.svg"), sidebar, true);
    addBadgeDot(sessionBtn, QStringLiteral("99+"));
    sideLayout->addWidget(sessionBtn, 0, Qt::AlignLeft);

    auto *starBtn = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/star.svg"), sidebar, false);
    addBadgeDot(starBtn, QString());
    sideLayout->addWidget(starBtn, 0, Qt::AlignLeft);

    sideLayout->addWidget(navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/group.svg"), sidebar), 0, Qt::AlignLeft);
    sideLayout->addWidget(navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/file-upload.svg"), sidebar), 0, Qt::AlignLeft);
    auto *settingsBtn = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/settings.svg"), sidebar);
    sideLayout->addWidget(settingsBtn, 0, Qt::AlignLeft);
    connect(settingsBtn, &QPushButton::clicked, this, &MainListWindow::handleDeviceManager);
    sideLayout->addStretch();

    auto *menuBtn = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/more.svg"), sidebar);
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
        QStringLiteral(
            "QFrame { background: %1; border-radius: 18px; border: 1px solid %2; }"
            "QLineEdit { background: transparent; border: none; color: %3; font-size: 13px; }"
            "QLabel { color: %4; font-size: 13px; }")
            .arg(Tokens::searchBg().name(),
                 Theme::uiBorder().name(),
                 Tokens::textMain().name(),
                 Tokens::textMuted().name()));
    auto *sLayout = new QHBoxLayout(searchBox);
    sLayout->setContentsMargins(12, 6, 12, 6);
    sLayout->setSpacing(8);
    auto *searchIcon = new QLabel(searchBox);
    searchIcon->setFixedSize(16, 16);
    searchIcon->setPixmap(UiIcons::TintedSvg(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"),
                                             16, Tokens::textMuted()));
    searchIcon->setAlignment(Qt::AlignCenter);
    searchEdit_ = new QLineEdit(searchBox);
    searchEdit_->setPlaceholderText(UiSettings::Tr(QStringLiteral("搜索"), QStringLiteral("Search")));
    connect(searchEdit_, &QLineEdit::textChanged, this, &MainListWindow::handleSearchTextChanged);
    sLayout->addWidget(searchIcon);
    sLayout->addWidget(searchEdit_, 1);

    auto *plusBtn = new IconButton(QString(), mainArea);
    plusBtn->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/plus.svg"), 18);
    plusBtn->setFocusPolicy(Qt::NoFocus);
    plusBtn->setFixedSize(36, 36);
    plusBtn->setColors(Tokens::textMain(), Tokens::textMain(), Tokens::textMain(),
                       Tokens::searchBg(), Tokens::hoverBg(), Tokens::selectedBg());
    connect(plusBtn, &QPushButton::clicked, this, [this, plusBtn]() {
        QMenu menu(this);
        UiStyle::ApplyMenuStyle(menu);
        QAction *addFriend = menu.addAction(
            UiSettings::Tr(QStringLiteral("添加好友"), QStringLiteral("Add friend")));
        QAction *createGroup = menu.addAction(
            UiSettings::Tr(QStringLiteral("创建群聊"), QStringLiteral("Create group")));
        QAction *joinGroup = menu.addAction(
            UiSettings::Tr(QStringLiteral("加入群聊"), QStringLiteral("Join group")));
        QAction *picked = menu.exec(plusBtn->mapToGlobal(QPoint(0, plusBtn->height())));
        if (!picked) {
            return;
        }
        if (picked == addFriend) {
            handleAddFriend();
            return;
        }
        if (picked == createGroup) {
            handleCreateGroup();
            return;
        }
        if (picked == joinGroup) {
            handleJoinGroup();
            return;
        }
    });

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
        QStringLiteral(
            "QListView { background: transparent; outline: none; border: 1px solid transparent; border-radius: 8px; }"
            "QListView:focus { border: 1px solid %3; }"
            "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }")
            .arg(Theme::uiScrollBarHandle().name(),
                 Theme::uiScrollBarHandleHover().name(),
                 Theme::uiAccentBlue().name()));
    setTabOrder(searchEdit_, listView_);

    model_ = new QStandardItemModel(listView_);
    auto addRow = [this](const QString &id, const QString &title, const QString &preview, const QString &time, int unread,
                         bool greyBadge, bool hasTag, bool isGroup) {
        if (!model_) {
            return;
        }
        auto *item = new QStandardItem();
        item->setData(id, IdRole);
        item->setData(title, TitleRole);
        item->setData(preview, PreviewRole);
        item->setData(time, TimeRole);
        item->setData(unread, UnreadRole);
        item->setData(greyBadge, GreyBadgeRole);
        item->setData(hasTag, HasTagRole);
        item->setData(isGroup, IsGroupRole);
        model_->appendRow(item);
    };

    if (backend_) {
        connect(backend_, &BackendAdapter::friendListLoaded, this,
                [this, addRow](const QVector<BackendAdapter::FriendEntry> &friends, const QString &loadErr) mutable {
                    if (!model_) {
                        return;
                    }

                    for (int i = model_->rowCount() - 1; i >= 0; --i) {
                        const QString id = model_->item(i)->data(IdRole).toString();
                        if (id.startsWith(QStringLiteral("__"))) {
                            model_->removeRow(i);
                        }
                    }

                    if (!friends.isEmpty()) {
                        for (const auto &f : friends) {
                            addRow(f.username, f.displayName(),
                                   UiSettings::Tr(QStringLiteral("点击开始聊天"),
                                                 QStringLiteral("Click to chat")),
                                   QString(), 0, true, false, false);
                        }
                    } else {
                        const QString tip = loadErr.trimmed().isEmpty()
                                                ? UiSettings::Tr(QStringLiteral("点击右上角 + 添加好友"),
                                                                QStringLiteral("Use + to add friends"))
                                                : loadErr.trimmed();
                        addRow(QStringLiteral("__placeholder__"),
                               UiSettings::Tr(QStringLiteral("暂无好友"), QStringLiteral("No friends yet")),
                               tip,
                               QString(), 0, true, false, false);
                    }

                    if (model_->rowCount() > 0 && !listView_->currentIndex().isValid()) {
                        listView_->setCurrentIndex(model_->index(0, 0));
                    }
                });
        addRow(QStringLiteral("__loading__"),
               UiSettings::Tr(QStringLiteral("加载中"), QStringLiteral("Loading")),
               UiSettings::Tr(QStringLiteral("正在获取好友列表…"), QStringLiteral("Fetching friend list…")),
               QString(), 0, true, false, false);
        backend_->requestFriendList();
    } else {
        addRow(QStringLiteral("__placeholder__"),
               UiSettings::Tr(QStringLiteral("暂无好友"), QStringLiteral("No friends yet")),
               UiSettings::Tr(QStringLiteral("未连接后端，点击右上角 + 添加好友"),
                             QStringLiteral("Backend offline. Use + to add friends")),
               QString(), 0, true, false, false);
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
        const bool isGroup = idx.data(IsGroupRole).toBool();
        if (isGroup) {
            QMenu menu(this);
            UiStyle::ApplyMenuStyle(menu);
            QAction *copyId = menu.addAction(QStringLiteral("复制群 ID"));
            QAction *invite = menu.addAction(QStringLiteral("邀请成员..."));
            QAction *members = menu.addAction(QStringLiteral("查看成员"));
            menu.addSeparator();
            QAction *leave = menu.addAction(QStringLiteral("退出群聊"));
            QAction *picked = menu.exec(listView_->viewport()->mapToGlobal(pos));
            if (!picked) {
                return;
            }
            if (picked == copyId) {
                if (auto *cb = QGuiApplication::clipboard()) {
                    cb->setText(id);
                }
                QMessageBox::information(this, QStringLiteral("群聊"), QStringLiteral("群 ID 已复制"));
                return;
            }
            if (picked == invite) {
                bool ok = false;
                const QString who =
                    QInputDialog::getText(this, QStringLiteral("邀请成员"),
                                          QStringLiteral("输入对方账号"),
                                          QLineEdit::Normal, QString(), &ok);
                if (!ok || who.trimmed().isEmpty()) {
                    return;
                }
                QString messageId;
                QString err;
                if (!backend_->sendGroupInvite(id, who.trimmed(), messageId, err)) {
                    QMessageBox::warning(this, QStringLiteral("邀请成员"),
                                         err.isEmpty() ? QStringLiteral("邀请失败") : err);
                    return;
                }
                if (!err.isEmpty()) {
                    QMessageBox::information(this, QStringLiteral("邀请成员"),
                                             QStringLiteral("已发送（提示：%1）").arg(err));
                } else {
                    QMessageBox::information(this, QStringLiteral("邀请成员"),
                                             QStringLiteral("已邀请：%1").arg(who.trimmed()));
                }
                return;
            }
            if (picked == members) {
                QString err;
                const auto list = backend_->listGroupMembers(id, err);
                if (list.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("群成员"),
                                         err.isEmpty() ? QStringLiteral("获取失败") : err);
                    return;
                }
                QString text = QStringLiteral("成员（%1）：\n").arg(list.size());
                for (const auto &m : list) {
                    text += QStringLiteral("- %1\n").arg(m);
                }
                QMessageBox::information(this, QStringLiteral("群成员"), text.trimmed());
                return;
            }
            if (picked == leave) {
                if (QMessageBox::question(this, QStringLiteral("退出群聊"),
                                          QStringLiteral("确认退出该群聊？")) != QMessageBox::Yes) {
                    return;
                }
                QString err;
                if (!backend_->leaveGroup(id, err)) {
                    QMessageBox::warning(this, QStringLiteral("退出群聊"),
                                         err.isEmpty() ? QStringLiteral("退出失败") : err);
                    return;
                }
                if (chatWindows_.contains(id) && chatWindows_[id]) {
                    chatWindows_[id]->close();
                }
                model_->removeRow(idx.row());
                return;
            }
            return;
        }
        QMenu menu(this);
        UiStyle::ApplyMenuStyle(menu);
        QAction *edit = menu.addAction(QStringLiteral("修改备注"));
        QAction *del = menu.addAction(QStringLiteral("删除好友"));
        menu.addSeparator();
        QAction *block = menu.addAction(QStringLiteral("拉黑"));
        QAction *unblock = menu.addAction(QStringLiteral("取消拉黑"));
        QAction *picked = menu.exec(listView_->viewport()->mapToGlobal(pos));
        if (!picked) {
            return;
        }

        if (picked == edit) {
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
            return;
        }

        if (picked == del) {
            if (QMessageBox::question(this, QStringLiteral("删除好友"),
                                      QStringLiteral("确认删除好友：%1？").arg(id)) != QMessageBox::Yes) {
                return;
            }
            QString err;
            if (!backend_->deleteFriend(id, err)) {
                QMessageBox::warning(this, QStringLiteral("删除好友"),
                                     err.isEmpty() ? QStringLiteral("删除失败") : err);
                return;
            }
            model_->removeRow(idx.row());
            return;
        }

        if (picked == block || picked == unblock) {
            const bool doBlock = (picked == block);
            if (doBlock) {
                if (QMessageBox::question(this, QStringLiteral("拉黑"),
                                          QStringLiteral("确认拉黑：%1？").arg(id)) != QMessageBox::Yes) {
                    return;
                }
            }
            QString err;
            if (!backend_->setUserBlocked(id, doBlock, err)) {
                QMessageBox::warning(this, doBlock ? QStringLiteral("拉黑") : QStringLiteral("取消拉黑"),
                                     err.isEmpty() ? QStringLiteral("操作失败") : err);
                return;
            }
            if (doBlock) {
                model_->removeRow(idx.row());
            }
            return;
        }
    });

    mainLayout2->addWidget(listView_);

    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(mainArea, 1);

    rootLayout->addWidget(body);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_main_list.png"));

    initTray();

    if (backend_) {
        connect(backend_, &BackendAdapter::incomingMessage, this, &MainListWindow::handleIncomingMessage);
        connect(backend_, &BackendAdapter::incomingSticker, this, &MainListWindow::handleIncomingSticker);
        connect(backend_, &BackendAdapter::syncedOutgoingMessage, this, &MainListWindow::handleSyncedOutgoingMessage);
        connect(backend_, &BackendAdapter::syncedOutgoingSticker, this, &MainListWindow::handleSyncedOutgoingSticker);
        connect(backend_, &BackendAdapter::delivered, this, &MainListWindow::handleDelivered);
        connect(backend_, &BackendAdapter::read, this, &MainListWindow::handleRead);
        connect(backend_, &BackendAdapter::typingChanged, this, &MainListWindow::handleTypingChanged);
        connect(backend_, &BackendAdapter::presenceChanged, this, &MainListWindow::handlePresenceChanged);
        connect(backend_, &BackendAdapter::messageResent, this, &MainListWindow::handleMessageResent);
        connect(backend_, &BackendAdapter::fileSendFinished, this, &MainListWindow::handleFileSendFinished);
        connect(backend_, &BackendAdapter::fileSaveFinished, this, &MainListWindow::handleFileSaveFinished);
        connect(backend_, &BackendAdapter::peerTrustRequired, this, &MainListWindow::handlePeerTrustRequired);
        connect(backend_, &BackendAdapter::serverTrustRequired, this, &MainListWindow::handleServerTrustRequired);
        connect(backend_, &BackendAdapter::friendRequestReceived, this, &MainListWindow::handleFriendRequestReceived);
        connect(backend_, &BackendAdapter::groupInviteReceived, this, &MainListWindow::handleGroupInviteReceived);
        connect(backend_, &BackendAdapter::groupNoticeReceived, this, &MainListWindow::handleGroupNoticeReceived);
        connect(backend_, &BackendAdapter::connectionStateChanged, this, &MainListWindow::handleConnectionStateChanged);
        handleConnectionStateChanged(backend_->isOnline(), backend_->isOnline() ? QStringLiteral("在线") : QStringLiteral("离线"));
    }
}

void MainListWindow::initTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    if (tray_) {
        return;
    }

    tray_ = new QSystemTrayIcon(this);
    QIcon icon = windowIcon();
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("chat"));
    }
    tray_->setIcon(icon);
    tray_->setToolTip(QStringLiteral("MI E2EE"));

    trayMenu_ = new QMenu(this);
    UiStyle::ApplyMenuStyle(*trayMenu_);
    traySettingsAction_ =
        trayMenu_->addAction(UiSettings::Tr(QStringLiteral("设置..."),
                                            QStringLiteral("Settings...")));
    connect(traySettingsAction_, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (backend_) {
            dlg.setClientConfigPath(backend_->configPath());
        }
        dlg.exec();
        const UiSettings::Settings s = UiSettings::current();
        if (trayNotifyAction_) {
            trayNotifyAction_->blockSignals(true);
            trayNotifyAction_->setChecked(s.trayNotifications);
            trayNotifyAction_->blockSignals(false);
        }
        if (trayPreviewAction_) {
            trayPreviewAction_->blockSignals(true);
            trayPreviewAction_->setChecked(s.trayPreview);
            trayPreviewAction_->setEnabled(s.trayNotifications);
            trayPreviewAction_->blockSignals(false);
        }
    });

    trayMenu_->addSeparator();
    trayShowAction_ = trayMenu_->addAction(UiSettings::Tr(QStringLiteral("显示/隐藏"),
                                                          QStringLiteral("Show/Hide")));
    trayMenu_->addSeparator();
    trayNotifyAction_ =
        trayMenu_->addAction(UiSettings::Tr(QStringLiteral("启用通知"),
                                            QStringLiteral("Enable notifications")));
    trayNotifyAction_->setCheckable(true);
    trayNotifyAction_->setChecked(UiSettings::current().trayNotifications);
    connect(trayNotifyAction_, &QAction::toggled, this, [this](bool on) {
        UiSettings::Settings s = UiSettings::current();
        s.trayNotifications = on;
        if (!on) {
            s.trayPreview = false;
        }
        UiSettings::setCurrent(s);
        UiSettings::Save(s);
        if (trayPreviewAction_) {
            trayPreviewAction_->blockSignals(true);
            trayPreviewAction_->setEnabled(on);
            trayPreviewAction_->setChecked(s.trayPreview);
            trayPreviewAction_->blockSignals(false);
        }
    });

    trayPreviewAction_ =
        trayMenu_->addAction(UiSettings::Tr(QStringLiteral("通知显示消息内容（默认关闭）"),
                                            QStringLiteral("Show message previews (default off)")));
    trayPreviewAction_->setCheckable(true);
    trayPreviewAction_->setChecked(UiSettings::current().trayPreview);
    trayPreviewAction_->setEnabled(UiSettings::current().trayNotifications);
    connect(trayPreviewAction_, &QAction::toggled, this, [this](bool on) {
        UiSettings::Settings s = UiSettings::current();
        s.trayPreview = on;
        UiSettings::setCurrent(s);
        UiSettings::Save(s);
    });
#ifdef Q_OS_WIN
    trayAutostartAction_ =
        trayMenu_->addAction(UiSettings::Tr(QStringLiteral("开机自启（默认关闭）"),
                                            QStringLiteral("Start with Windows (default off)")));
    trayAutostartAction_->setCheckable(true);
    trayAutostartAction_->setChecked(IsAutoStartEnabled());
    connect(trayAutostartAction_, &QAction::toggled, this, [this](bool on) {
        if (!SetAutoStartEnabled(on)) {
            if (trayAutostartAction_) {
                trayAutostartAction_->blockSignals(true);
                trayAutostartAction_->setChecked(!on);
                trayAutostartAction_->blockSignals(false);
            }
            QMessageBox::warning(this,
                                 UiSettings::Tr(QStringLiteral("开机自启"),
                                               QStringLiteral("Start with Windows")),
                                 UiSettings::Tr(QStringLiteral("设置失败（可能无权限）"),
                                               QStringLiteral("Failed to update setting.")));
        }
    });
#else
    trayAutostartAction_ =
        trayMenu_->addAction(UiSettings::Tr(QStringLiteral("开机自启（仅 Windows）"),
                                            QStringLiteral("Start with Windows (Windows only)")));
    trayAutostartAction_->setEnabled(false);
#endif
    trayMenu_->addSeparator();
    trayExitAction_ = trayMenu_->addAction(UiSettings::Tr(QStringLiteral("退出"),
                                                          QStringLiteral("Exit")));

    connect(trayShowAction_, &QAction::triggered, this, [this]() {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    });
    connect(trayExitAction_, &QAction::triggered, this, [this]() {
        closing_ = true;
        if (tray_) {
            tray_->hide();
        }
        close();
    });

    tray_->setContextMenu(trayMenu_);
    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason != QSystemTrayIcon::Trigger &&
                    reason != QSystemTrayIcon::DoubleClick) {
                    return;
                }
                if (isVisible()) {
                    hide();
                } else {
                    show();
                    raise();
                    activateWindow();
                }
            });

    tray_->show();
}

void MainListWindow::showTrayMessage(const QString &title, const QString &message) {
    if (!tray_ || !tray_->isVisible()) {
        return;
    }
    if (trayNotifyAction_ && !trayNotifyAction_->isChecked()) {
        return;
    }
    tray_->showMessage(title, message, QSystemTrayIcon::Information, 6000);
}

void MainListWindow::closeEvent(QCloseEvent *event) {
    if (closing_ || !tray_) {
        FramelessWindowBase::closeEvent(event);
        return;
    }

    hide();
    event->ignore();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = lastNotifyMs_.value(QStringLiteral("__tray_hint__"), 0);
    if (now - last > 30000) {
        lastNotifyMs_.insert(QStringLiteral("__tray_hint__"), now);
        showTrayMessage(UiSettings::Tr(QStringLiteral("已最小化到托盘"),
                                       QStringLiteral("Minimized to tray")),
                        UiSettings::Tr(QStringLiteral("右键托盘图标可退出"),
                                       QStringLiteral("Right-click tray icon to exit")));
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
    const bool isGroup = index.data(IsGroupRole).toBool();

    if (chatWindows_.contains(id) && chatWindows_[id]) {
        chatWindows_[id]->setConversation(id, title, isGroup);
        chatWindows_[id]->show();
        chatWindows_[id]->raise();
        chatWindows_[id]->activateWindow();
        return;
    }

    ChatWindow *win = new ChatWindow(backend_);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setConversation(id, title, isGroup);
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
        if (backend_->sendFriendRequest(account.trimmed(), remark.trimmed(), err)) {
            QMessageBox::information(this, QStringLiteral("添加好友"),
                                     QStringLiteral("已发送好友申请：%1").arg(account.trimmed()));
        } else {
            QMessageBox::warning(this, QStringLiteral("添加好友"),
                                 QStringLiteral("发送失败：%1").arg(err));
        }
    } else {
        QMessageBox::warning(this, QStringLiteral("添加好友"),
                             QStringLiteral("未连接后端"));
    }
}

void MainListWindow::handleCreateGroup() {
    if (!backend_ || !model_) {
        QMessageBox::warning(this, QStringLiteral("创建群聊"), QStringLiteral("未连接后端"));
        return;
    }
    QString groupId;
    QString err;
    if (!backend_->createGroup(groupId, err)) {
        QMessageBox::warning(this, QStringLiteral("创建群聊"),
                             err.isEmpty() ? QStringLiteral("创建失败") : err);
        return;
    }

    if (auto *cb = QGuiApplication::clipboard()) {
        cb->setText(groupId);
    }

    int rowIndex = -1;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (model_->item(i)->data(IdRole).toString() == groupId) {
            rowIndex = i;
            break;
        }
    }
    if (rowIndex == -1) {
        auto *item = new QStandardItem();
        item->setData(groupId, IdRole);
        item->setData(UiSettings::Tr(QStringLiteral("群聊 %1").arg(groupId),
                                     QStringLiteral("Group %1").arg(groupId)),
                      TitleRole);
        item->setData(UiSettings::Tr(QStringLiteral("点击开始聊天"),
                                     QStringLiteral("Click to chat")),
                      PreviewRole);
        item->setData(QString(), TimeRole);
        item->setData(0, UnreadRole);
        item->setData(true, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(true, IsGroupRole);
        model_->insertRow(0, item);
        rowIndex = 0;
    }

    listView_->setCurrentIndex(model_->index(rowIndex, 0));
    openChatForIndex(model_->index(rowIndex, 0));
    QMessageBox::information(this, QStringLiteral("创建群聊"),
                             QStringLiteral("群聊已创建，群 ID 已复制到剪贴板。\n\n%1").arg(groupId));
}

void MainListWindow::handleJoinGroup() {
    if (!backend_ || !model_) {
        QMessageBox::warning(this, QStringLiteral("加入群聊"), QStringLiteral("未连接后端"));
        return;
    }

    bool ok = false;
    const QString groupId =
        QInputDialog::getText(this, QStringLiteral("加入群聊"),
                              QStringLiteral("输入群 ID"),
                              QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || groupId.isEmpty()) {
        return;
    }

    QString err;
    if (!backend_->joinGroup(groupId, err)) {
        QMessageBox::warning(this, QStringLiteral("加入群聊"),
                             err.isEmpty() ? QStringLiteral("加入失败") : err);
        return;
    }

    int rowIndex = -1;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (model_->item(i)->data(IdRole).toString() == groupId) {
            rowIndex = i;
            break;
        }
    }
    if (rowIndex == -1) {
        auto *item = new QStandardItem();
        item->setData(groupId, IdRole);
        item->setData(QStringLiteral("群聊 %1").arg(groupId), TitleRole);
        item->setData(QStringLiteral("点击开始聊天"), PreviewRole);
        item->setData(QString(), TimeRole);
        item->setData(0, UnreadRole);
        item->setData(true, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(true, IsGroupRole);
        model_->insertRow(0, item);
        rowIndex = 0;
    } else {
        if (auto *item = model_->item(rowIndex)) {
            item->setData(true, IsGroupRole);
        }
    }

    listView_->setCurrentIndex(model_->index(rowIndex, 0));
    openChatForIndex(model_->index(rowIndex, 0));
    QMessageBox::information(this, QStringLiteral("加入群聊"),
                             QStringLiteral("已加入群聊：%1").arg(groupId));
}

void MainListWindow::handleDeviceManager() {
    if (!backend_) {
        QMessageBox::warning(this, QStringLiteral("设备管理"), QStringLiteral("未连接后端"));
        return;
    }

    const QString selfId = backend_->currentDeviceId().trimmed();
    QString err;
    const auto initial = backend_->listDevices(err);
    if (initial.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("设备管理"),
                             err.isEmpty() ? QStringLiteral("获取设备列表失败") : err);
        return;
    }

    struct DialogState {
        QVector<BackendAdapter::DeviceEntry> devices;
    };
    auto state = std::make_shared<DialogState>();
    state->devices = initial;

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle(QStringLiteral("设备管理"));
    dlg->resize(560, 420);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *hint = new QLabel(dlg);
    hint->setTextFormat(Qt::PlainText);
    hint->setWordWrap(true);
    hint->setText(selfId.isEmpty()
                      ? QStringLiteral("当前设备 ID：未知")
                      : QStringLiteral("当前设备 ID：%1").arg(selfId));
    root->addWidget(hint);

    auto *table = new QTableWidget(dlg);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({QStringLiteral("设备 ID"), QStringLiteral("最近活动")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    root->addWidget(table, 1);

    auto formatAge = [](quint32 sec) -> QString {
        if (sec == 0) {
            return QStringLiteral("刚刚");
        }
        if (sec < 60) {
            return QStringLiteral("%1 秒前").arg(sec);
        }
        const quint32 min = sec / 60;
        if (min < 60) {
            return QStringLiteral("%1 分钟前").arg(min);
        }
        const quint32 hr = min / 60;
        return QStringLiteral("%1 小时前").arg(hr);
    };

    auto populate = [table, formatAge](const QVector<BackendAdapter::DeviceEntry> &list) {
        table->clearContents();
        table->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            const auto &d = list[i];
            auto *idItem = new QTableWidgetItem(d.deviceId);
            auto *ageItem = new QTableWidgetItem(formatAge(d.lastSeenSec));
            table->setItem(i, 0, idItem);
            table->setItem(i, 1, ageItem);
        }
        table->resizeColumnsToContents();
    };

    populate(state->devices);

    auto *pairFrame = new QFrame(dlg);
    pairFrame->setFrameShape(QFrame::StyledPanel);
    pairFrame->setStyleSheet(
        QStringLiteral("QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
            .arg(Theme::uiPanelBg().name(), Theme::uiBorder().name()));
    auto *pairRoot = new QVBoxLayout(pairFrame);
    pairRoot->setContentsMargins(12, 10, 12, 10);
    pairRoot->setSpacing(8);

    auto *pairTitle = new QLabel(pairFrame);
    pairTitle->setTextFormat(Qt::PlainText);
    pairTitle->setText(QStringLiteral("设备配对（多端同步）"));
    pairTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    pairRoot->addWidget(pairTitle);

    auto *pairHint = new QLabel(pairFrame);
    pairHint->setTextFormat(Qt::PlainText);
    pairHint->setWordWrap(true);
    pairHint->setFont(Theme::defaultFont(11));
    pairHint->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextSub().name()));
    pairRoot->addWidget(pairHint);

    const bool syncEnabled = backend_->deviceSyncEnabled();
    const bool isPrimary = backend_->deviceSyncIsPrimary();
    if (!syncEnabled) {
        pairHint->setText(QStringLiteral("未启用多端同步：请在 client_config.ini 的 [device_sync] 打开 enabled=1，并设置 role=primary/linked。"));
    } else {
        pairHint->setText(isPrimary ? QStringLiteral("当前为主设备：生成配对码后，在新设备输入配对码并等待确认。")
                                    : QStringLiteral("当前为从设备：输入主设备生成的配对码，等待主设备确认。"));
    }

    auto *pairTimer = new QTimer(dlg);
    pairTimer->setInterval(2000);

    if (syncEnabled && isPrimary) {
        auto *codeRow = new QHBoxLayout();
        codeRow->setSpacing(8);

        auto *codeLabel = new QLabel(pairFrame);
        codeLabel->setText(QStringLiteral("配对码："));
        codeRow->addWidget(codeLabel);

        auto *codeEdit = new QLineEdit(pairFrame);
        codeEdit->setReadOnly(true);
        codeEdit->setPlaceholderText(QStringLiteral("未生成"));
        codeRow->addWidget(codeEdit, 1);

        auto *genBtn = new QPushButton(QStringLiteral("生成配对码"), pairFrame);
        codeRow->addWidget(genBtn);

        auto *cancelBtn = new QPushButton(QStringLiteral("取消"), pairFrame);
        cancelBtn->setEnabled(false);
        codeRow->addWidget(cancelBtn);

        pairRoot->addLayout(codeRow);

        auto *reqTable = new QTableWidget(pairFrame);
        reqTable->setColumnCount(1);
        reqTable->setHorizontalHeaderLabels({QStringLiteral("待确认的设备请求")});
        reqTable->horizontalHeader()->setStretchLastSection(true);
        reqTable->verticalHeader()->setVisible(false);
        reqTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        reqTable->setSelectionMode(QAbstractItemView::SingleSelection);
        reqTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        reqTable->setShowGrid(false);
        reqTable->setAlternatingRowColors(true);
        pairRoot->addWidget(reqTable);

        auto *reqButtons = new QHBoxLayout();
        reqButtons->setSpacing(8);
        reqButtons->addStretch(1);
        auto *refreshReqBtn = new QPushButton(QStringLiteral("刷新请求"), pairFrame);
        auto *approveBtn = new QPushButton(QStringLiteral("允许配对"), pairFrame);
        approveBtn->setEnabled(false);
        reqButtons->addWidget(refreshReqBtn);
        reqButtons->addWidget(approveBtn);
        pairRoot->addLayout(reqButtons);

        auto currentReq = [reqTable]() -> BackendAdapter::DevicePairingRequestEntry {
            BackendAdapter::DevicePairingRequestEntry out;
            const QModelIndexList rows = reqTable->selectionModel()
                                             ? reqTable->selectionModel()->selectedRows()
                                             : QModelIndexList{};
            if (rows.isEmpty()) {
                return out;
            }
            const int row = rows.first().row();
            if (auto *item = reqTable->item(row, 0)) {
                out.deviceId = item->text();
                out.requestIdHex = item->data(Qt::UserRole + 1).toString();
            }
            return out;
        };

        auto populateReq = [reqTable](const QVector<BackendAdapter::DevicePairingRequestEntry> &list) {
            reqTable->clearContents();
            reqTable->setRowCount(list.size());
            for (int i = 0; i < list.size(); ++i) {
                const auto &r = list[i];
                auto *item = new QTableWidgetItem(r.deviceId);
                item->setData(Qt::UserRole + 1, r.requestIdHex);
                reqTable->setItem(i, 0, item);
            }
            reqTable->resizeColumnsToContents();
        };

        auto refreshReq = [=]() {
            QVector<BackendAdapter::DevicePairingRequestEntry> reqs;
            QString err;
            if (!backend_->pollDevicePairingRequests(reqs, err)) {
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("设备配对"), err);
                }
                return;
            }
            populateReq(reqs);
            approveBtn->setEnabled(!currentReq().deviceId.trimmed().isEmpty());
        };

        QObject::connect(reqTable, &QTableWidget::itemSelectionChanged, dlg, [=]() {
            approveBtn->setEnabled(!currentReq().deviceId.trimmed().isEmpty());
        });

        QObject::connect(refreshReqBtn, &QPushButton::clicked, dlg, refreshReq);

        QObject::connect(genBtn, &QPushButton::clicked, dlg, [=]() mutable {
            QString err;
            QString code;
            if (!backend_->beginDevicePairingPrimary(code, err)) {
                QMessageBox::warning(this, QStringLiteral("设备配对"),
                                     err.isEmpty() ? QStringLiteral("生成配对码失败") : err);
                return;
            }
            codeEdit->setText(code);
            cancelBtn->setEnabled(true);
            pairTimer->start();
            refreshReq();
        });

        QObject::connect(cancelBtn, &QPushButton::clicked, dlg, [=]() mutable {
            backend_->cancelDevicePairing();
            codeEdit->clear();
            codeEdit->setPlaceholderText(QStringLiteral("未生成"));
            cancelBtn->setEnabled(false);
            approveBtn->setEnabled(false);
            pairTimer->stop();
            populateReq({});
        });

        QObject::connect(approveBtn, &QPushButton::clicked, dlg, [=]() mutable {
            const auto req = currentReq();
            if (req.deviceId.trimmed().isEmpty() || req.requestIdHex.trimmed().isEmpty()) {
                return;
            }
            if (QMessageBox::question(this, QStringLiteral("设备配对"),
                                      QStringLiteral("确认允许该设备配对？\n\n%1").arg(req.deviceId)) != QMessageBox::Yes) {
                return;
            }
            QString err;
            if (!backend_->approveDevicePairingRequest(req, err)) {
                QMessageBox::warning(this, QStringLiteral("设备配对"),
                                     err.isEmpty() ? QStringLiteral("确认配对失败") : err);
                return;
            }
            codeEdit->clear();
            codeEdit->setPlaceholderText(QStringLiteral("未生成"));
            cancelBtn->setEnabled(false);
            approveBtn->setEnabled(false);
            pairTimer->stop();
            populateReq({});
            QMessageBox::information(this, QStringLiteral("设备配对"), QStringLiteral("已完成配对"));
        });

        QObject::connect(pairTimer, &QTimer::timeout, dlg, refreshReq);
    } else if (syncEnabled && !isPrimary) {
        auto *codeRow = new QHBoxLayout();
        codeRow->setSpacing(8);

        auto *codeLabel = new QLabel(pairFrame);
        codeLabel->setText(QStringLiteral("配对码："));
        codeRow->addWidget(codeLabel);

        auto *codeEdit = new QLineEdit(pairFrame);
        codeEdit->setPlaceholderText(QStringLiteral("输入主设备配对码"));
        codeRow->addWidget(codeEdit, 1);

        auto *startBtn = new QPushButton(QStringLiteral("开始配对"), pairFrame);
        codeRow->addWidget(startBtn);

        auto *cancelBtn = new QPushButton(QStringLiteral("取消"), pairFrame);
        cancelBtn->setEnabled(false);
        codeRow->addWidget(cancelBtn);

        pairRoot->addLayout(codeRow);

        auto *status = new QLabel(pairFrame);
        status->setTextFormat(Qt::PlainText);
        status->setWordWrap(true);
        status->setFont(Theme::defaultFont(11));
        status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextSub().name()));
        status->setText(QStringLiteral("未开始"));
        pairRoot->addWidget(status);

        auto pollOnce = [=]() mutable {
            bool done = false;
            QString err;
            if (!backend_->pollDevicePairingLinked(done, err)) {
                if (!err.isEmpty()) {
                    status->setText(QStringLiteral("配对失败：%1").arg(err));
                }
                pairTimer->stop();
                cancelBtn->setEnabled(false);
                return;
            }
            if (done) {
                pairTimer->stop();
                cancelBtn->setEnabled(false);
                status->setText(QStringLiteral("配对完成：已写入 device_sync_key"));
                QMessageBox::information(this, QStringLiteral("设备配对"), QStringLiteral("配对完成"));
                return;
            }
            status->setText(QStringLiteral("等待主设备确认…"));
        };

        QObject::connect(startBtn, &QPushButton::clicked, dlg, [=]() mutable {
            const QString code = codeEdit->text().trimmed();
            QString err;
            if (!backend_->beginDevicePairingLinked(code, err)) {
                QMessageBox::warning(this, QStringLiteral("设备配对"),
                                     err.isEmpty() ? QStringLiteral("开始配对失败") : err);
                return;
            }
            cancelBtn->setEnabled(true);
            status->setText(QStringLiteral("等待主设备确认…"));
            pairTimer->start();
            pollOnce();
        });

        QObject::connect(cancelBtn, &QPushButton::clicked, dlg, [=]() mutable {
            backend_->cancelDevicePairing();
            pairTimer->stop();
            cancelBtn->setEnabled(false);
            status->setText(QStringLiteral("已取消"));
        });

        QObject::connect(pairTimer, &QTimer::timeout, dlg, pollOnce);
    }

    root->addWidget(pairFrame);

    auto currentSelected = [table]() -> QString {
        const QModelIndexList rows = table->selectionModel()
                                         ? table->selectionModel()->selectedRows()
                                         : QModelIndexList{};
        if (rows.isEmpty()) {
            return {};
        }
        const QModelIndex idx = rows.first();
        return table->item(idx.row(), 0) ? table->item(idx.row(), 0)->text() : QString();
    };

    auto *buttons = new QDialogButtonBox(dlg);
    auto *refreshBtn = buttons->addButton(QStringLiteral("刷新"), QDialogButtonBox::ActionRole);
    auto *copyBtn = buttons->addButton(QStringLiteral("复制设备 ID"), QDialogButtonBox::ActionRole);
    auto *kickBtn = buttons->addButton(QStringLiteral("踢下线"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    root->addWidget(buttons);

    auto updateButtons = [=]() {
        const QString selected = currentSelected().trimmed();
        const bool hasSel = !selected.isEmpty();
        copyBtn->setEnabled(hasSel);
        kickBtn->setEnabled(hasSel && !selfId.isEmpty() && selected != selfId);
    };

    auto refresh = [=]() {
        QString err;
        const auto list = backend_->listDevices(err);
        if (list.isEmpty()) {
            if (!err.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("设备管理"), err);
            }
            return;
        }
        state->devices = list;
        populate(state->devices);
        updateButtons();
    };

    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    QObject::connect(refreshBtn, &QPushButton::clicked, dlg, refresh);
    QObject::connect(table, &QTableWidget::itemSelectionChanged, dlg, updateButtons);

    QObject::connect(copyBtn, &QPushButton::clicked, dlg, [=]() {
        const QString selected = currentSelected().trimmed();
        if (selected.isEmpty()) {
            return;
        }
        if (auto *cb = QGuiApplication::clipboard()) {
            cb->setText(selected);
        }
        QMessageBox::information(this, QStringLiteral("设备管理"), QStringLiteral("已复制"));
    });

    QObject::connect(kickBtn, &QPushButton::clicked, dlg, [=]() {
        const QString selected = currentSelected().trimmed();
        if (selected.isEmpty()) {
            return;
        }
        if (!selfId.isEmpty() && selected == selfId) {
            QMessageBox::information(this, QStringLiteral("设备管理"), QStringLiteral("不能踢下线当前设备"));
            return;
        }
        if (QMessageBox::question(this, QStringLiteral("踢下线"),
                                  QStringLiteral("确认踢下线该设备？\n\n%1").arg(selected)) != QMessageBox::Yes) {
            return;
        }
        QString err;
        if (!backend_->kickDevice(selected, err)) {
            QMessageBox::warning(this, QStringLiteral("踢下线"),
                                 err.isEmpty() ? QStringLiteral("踢下线失败") : err);
            return;
        }
        refresh();
        QMessageBox::information(this, QStringLiteral("踢下线"), QStringLiteral("已踢下线"));
    });

    updateButtons();
    dlg->show();
}

void MainListWindow::handleSearchTextChanged(const QString &text) {
    // Placeholder: future backend filtering; currently no-op.
    Q_UNUSED(text);
}

void MainListWindow::handleIncomingMessage(const QString &convId, bool isGroup, const QString &sender,
                                           const QString &messageId, const QString &text, bool isFile, qint64 fileSize) {
    Q_UNUSED(messageId);
    Q_UNUSED(fileSize);
    QString preview;
    if (isFile) {
        const QString tag = FilePreviewTag(text);
        preview = isGroup && !sender.trimmed().isEmpty()
                      ? QStringLiteral("%1 %2: %3").arg(tag, sender, text)
                      : QStringLiteral("%1 %2").arg(tag, text);
    } else {
        preview = isGroup && !sender.trimmed().isEmpty()
                      ? QStringLiteral("%1: %2").arg(sender, text)
                      : text;
    }
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
        item->setData(isGroup ? QStringLiteral("群聊 %1").arg(convId) : convId, TitleRole);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(1, UnreadRole);
        item->setData(false, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(isGroup, IsGroupRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(isGroup, IsGroupRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendIncomingMessage(sender, messageId, text, isFile, fileSize, now);
        // 已打开窗口则视为已读
        if (rowIndex >= 0) {
            model_->item(rowIndex)->setData(0, UnreadRole);
        }
        return;
    }

    if (!tray_) {
        return;
    }

    const bool mainActive = isVisible() && !isMinimized() && isActiveWindow();
    if (mainActive) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString key = QStringLiteral("msg:") + convId;
    const qint64 last = lastNotifyMs_.value(key, 0);
    if (nowMs - last < 2000) {
        return;
    }
    lastNotifyMs_.insert(key, nowMs);

    const QString title =
        isGroup
            ? UiSettings::Tr(QStringLiteral("群聊 %1").arg(convId),
                             QStringLiteral("Group %1").arg(convId))
            : convId;
    const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
    const QString notifyTitle =
        allowPreview ? title
                     : UiSettings::Tr(QStringLiteral("新消息"),
                                      QStringLiteral("New message"));
    const QString notifyMsg =
        allowPreview ? preview
                     : UiSettings::Tr(QStringLiteral("收到新消息"),
                                      QStringLiteral("New message received"));
    showTrayMessage(notifyTitle, notifyMsg);
}

void MainListWindow::handleIncomingSticker(const QString &convId, const QString &sender,
                                           const QString &messageId, const QString &stickerId) {
    const QString preview = UiSettings::Tr(QStringLiteral("[贴纸]"),
                                           QStringLiteral("[Sticker]"));
    const bool isGroup = false;

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
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(1, UnreadRole);
        item->setData(false, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(isGroup, IsGroupRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(isGroup, IsGroupRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendIncomingSticker(sender, messageId, stickerId, now);
        if (rowIndex >= 0) {
            model_->item(rowIndex)->setData(0, UnreadRole);
        }
        return;
    }

    if (!tray_) {
        return;
    }
    const bool mainActive = isVisible() && !isMinimized() && isActiveWindow();
    if (mainActive) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString key = QStringLiteral("msg:") + convId;
    const qint64 last = lastNotifyMs_.value(key, 0);
    if (nowMs - last < 2000) {
        return;
    }
    lastNotifyMs_.insert(key, nowMs);

    const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
    const QString notifyTitle =
        allowPreview ? convId
                     : UiSettings::Tr(QStringLiteral("新消息"),
                                      QStringLiteral("New message"));
    const QString notifyMsg =
        allowPreview ? preview
                     : UiSettings::Tr(QStringLiteral("收到新消息"),
                                      QStringLiteral("New message received"));
    showTrayMessage(notifyTitle, notifyMsg);
}

void MainListWindow::handleSyncedOutgoingMessage(const QString &convId, bool isGroup, const QString &sender,
                                                 const QString &messageId, const QString &text, bool isFile, qint64 fileSize) {
    Q_UNUSED(sender);
    QString preview;
    if (isFile) {
        const QString tag = FilePreviewTag(text);
        preview = UiSettings::Tr(QStringLiteral("我 %1 %2").arg(tag, text),
                                 QStringLiteral("Me %1 %2").arg(tag, text));
    } else {
        preview = UiSettings::Tr(QStringLiteral("我: %1").arg(text),
                                 QStringLiteral("Me: %1").arg(text));
    }

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
        item->setData(
            isGroup
                ? UiSettings::Tr(QStringLiteral("群聊 %1").arg(convId),
                                 QStringLiteral("Group %1").arg(convId))
                : convId,
            TitleRole);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(0, UnreadRole);
        item->setData(true, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(isGroup, IsGroupRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(isGroup, IsGroupRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendSyncedOutgoingMessage(messageId, text, isFile, fileSize, now);
    }
}

void MainListWindow::handleSyncedOutgoingSticker(const QString &convId, const QString &messageId, const QString &stickerId) {
    const QString preview = UiSettings::Tr(QStringLiteral("我: [贴纸]"),
                                           QStringLiteral("Me: [Sticker]"));
    const bool isGroup = false;

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
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(0, UnreadRole);
        item->setData(true, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(isGroup, IsGroupRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(isGroup, IsGroupRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendSyncedOutgoingSticker(messageId, stickerId, now);
    }
}

void MainListWindow::handleDelivered(const QString &convId, const QString &messageId) {
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markDelivered(messageId);
    }
}

void MainListWindow::handleRead(const QString &convId, const QString &messageId) {
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markRead(messageId);
    }
}

void MainListWindow::handleTypingChanged(const QString &convId, bool typing) {
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setTypingIndicator(typing);
    }
}

void MainListWindow::handlePresenceChanged(const QString &convId, bool online) {
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setPresenceIndicator(online);
    }
}

void MainListWindow::handleMessageResent(const QString &convId, const QString &messageId) {
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markSent(messageId);
    }
}

void MainListWindow::handleFileSendFinished(const QString &convId, const QString &messageId,
                                            bool success, const QString &error) {
    if (!chatWindows_.contains(convId) || !chatWindows_[convId]) {
        return;
    }
    chatWindows_[convId]->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
    if (success) {
        chatWindows_[convId]->markSent(messageId);
        const QString msg = error.trimmed().isEmpty()
                                ? UiSettings::Tr(QStringLiteral("文件已发送"),
                                                 QStringLiteral("File sent"))
                                : UiSettings::Tr(QStringLiteral("提示：%1").arg(error),
                                                 QStringLiteral("Info: %1").arg(error));
        Toast::Show(this, msg, Toast::Level::Info);
        return;
    }
    chatWindows_[convId]->markFailed(messageId);
    const QString msg = error.trimmed().isEmpty()
                            ? UiSettings::Tr(QStringLiteral("发送失败"),
                                             QStringLiteral("Send failed"))
                            : UiSettings::Tr(QStringLiteral("发送失败：%1").arg(error),
                                             QStringLiteral("Send failed: %1").arg(error));
    Toast::Show(this, msg, Toast::Level::Error, 3200);
}

void MainListWindow::handleFileSaveFinished(const QString &convId, const QString &messageId,
                                            bool success, const QString &error, const QString &outPath) {
    if (!chatWindows_.contains(convId) || !chatWindows_[convId]) {
        return;
    }
    chatWindows_[convId]->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
    if (success) {
        chatWindows_[convId]->setFileLocalPath(messageId, outPath);
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("文件已保存：%1").arg(outPath),
                                   QStringLiteral("File saved: %1").arg(outPath)),
                    Toast::Level::Success,
                    3000);
        return;
    }
    const QString msg = error.trimmed().isEmpty()
                            ? UiSettings::Tr(QStringLiteral("保存失败"),
                                             QStringLiteral("Save failed"))
                            : UiSettings::Tr(QStringLiteral("保存失败：%1").arg(error),
                                             QStringLiteral("Save failed: %1").arg(error));
    Toast::Show(this, msg, Toast::Level::Error, 3200);
}

void MainListWindow::handlePeerTrustRequired(const QString &peer,
                                             const QString &fingerprintHex,
                                             const QString &pin) {
    if (!backend_) {
        return;
    }

    const QString detail = QStringLiteral(
        "检测到需要验证对端身份（首次通信或对端密钥指纹变更）。\n\n"
        "对端：%1\n"
        "指纹：%2\n"
        "安全码（SAS）：%3\n\n"
        "请通过线下可信渠道核对安全码/指纹后再继续。")
                               .arg(peer, fingerprintHex, pin);

    QMessageBox box(QMessageBox::Warning, QStringLiteral("验证对端身份"), detail,
                    QMessageBox::NoButton, this);
    auto *trustBtn = box.addButton(QStringLiteral("我已核对，信任"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("稍后"), QMessageBox::RejectRole);
    box.setDefaultButton(trustBtn);
    box.exec();

    if (box.clickedButton() != trustBtn) {
        return;
    }

    bool ok = false;
    const QString input = QInputDialog::getText(
        this, QStringLiteral("输入安全码"),
        QStringLiteral("请输入上面显示的安全码（可包含 '-'，忽略大小写）："),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    QString err;
    if (!backend_->trustPendingPeer(input, err)) {
        QMessageBox::warning(this, QStringLiteral("信任失败"),
                             err.isEmpty() ? QStringLiteral("信任失败") : err);
        return;
    }

    QMessageBox::information(
        this,
        UiSettings::Tr(QStringLiteral("已信任"), QStringLiteral("Trusted")),
        UiSettings::Tr(QStringLiteral("已信任：%1").arg(peer),
                       QStringLiteral("Trusted: %1").arg(peer)));
}

void MainListWindow::handleServerTrustRequired(const QString &fingerprintHex, const QString &pin) {
    if (!backend_) {
        return;
    }

    const QString detail = UiSettings::Tr(
        QStringLiteral(
            "检测到需要验证服务器身份（首次连接或证书指纹变更）。\n\n"
            "指纹：%1\n"
            "安全码（SAS）：%2\n\n"
            "请通过线下可信渠道核对安全码/指纹后再继续。")
            .arg(fingerprintHex, pin),
        QStringLiteral(
            "Server identity verification required (first connection or certificate pin changed).\n\n"
            "Fingerprint: %1\n"
            "SAS: %2\n\n"
            "Verify via an out-of-band channel before trusting.")
            .arg(fingerprintHex, pin));

    QMessageBox box(QMessageBox::Warning,
                    UiSettings::Tr(QStringLiteral("验证服务器身份"),
                                   QStringLiteral("Verify server identity")),
                    detail,
                    QMessageBox::NoButton, this);
    auto *trustBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("我已核对，信任"),
                                     QStringLiteral("I verified it, trust")),
                      QMessageBox::AcceptRole);
    box.addButton(UiSettings::Tr(QStringLiteral("稍后"), QStringLiteral("Later")),
                  QMessageBox::RejectRole);
    box.setDefaultButton(trustBtn);
    box.exec();

    if (box.clickedButton() != trustBtn) {
        return;
    }

    bool ok = false;
    const QString input = QInputDialog::getText(
        this,
        UiSettings::Tr(QStringLiteral("输入安全码"),
                       QStringLiteral("Enter SAS")),
        UiSettings::Tr(QStringLiteral("请输入上面显示的安全码（可包含 '-'，忽略大小写）："),
                       QStringLiteral("Enter the SAS shown above (ignore '-' and case):")),
        QLineEdit::Normal, pin, &ok);
    if (!ok) {
        return;
    }

    QString err;
    if (!backend_->trustPendingServer(input, err)) {
        QMessageBox::warning(
            this,
            UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed")),
            err.isEmpty()
                ? UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed"))
                : err);
        return;
    }

    QMessageBox::information(
        this,
        UiSettings::Tr(QStringLiteral("已信任"), QStringLiteral("Trusted")),
        UiSettings::Tr(QStringLiteral("已信任服务器"),
                       QStringLiteral("Server trusted")));
}

void MainListWindow::handleFriendRequestReceived(const QString &requester, const QString &remark) {
    if (!backend_ || !model_) {
        return;
    }
    if (tray_) {
        const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
        const QString msg = allowPreview
                                ? (remark.trimmed().isEmpty()
                                       ? UiSettings::Tr(QStringLiteral("收到好友申请：%1").arg(requester),
                                                       QStringLiteral("Friend request: %1").arg(requester))
                                       : UiSettings::Tr(QStringLiteral("收到好友申请：%1（%2）").arg(requester, remark.trimmed()),
                                                       QStringLiteral("Friend request: %1 (%2)").arg(requester, remark.trimmed())))
                                : UiSettings::Tr(QStringLiteral("你收到新的好友申请"),
                                                 QStringLiteral("You received a new friend request"));
        showTrayMessage(UiSettings::Tr(QStringLiteral("好友申请"),
                                       QStringLiteral("Friend request")),
                        msg);
    }

    QString detail = UiSettings::Tr(QStringLiteral("收到好友申请：%1").arg(requester),
                                    QStringLiteral("Friend request from: %1").arg(requester));
    if (!remark.trimmed().isEmpty()) {
        detail += UiSettings::Tr(QStringLiteral("\n备注：%1").arg(remark.trimmed()),
                                 QStringLiteral("\nRemark: %1").arg(remark.trimmed()));
    }

    QMessageBox box(QMessageBox::Question,
                    UiSettings::Tr(QStringLiteral("新的好友申请"),
                                   QStringLiteral("New friend request")),
                    detail,
                    QMessageBox::NoButton, this);
    auto *acceptBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("同意"), QStringLiteral("Accept")),
                      QMessageBox::AcceptRole);
    auto *rejectBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("拒绝"), QStringLiteral("Reject")),
                      QMessageBox::RejectRole);
    auto *blockBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("拉黑"), QStringLiteral("Block")),
                      QMessageBox::DestructiveRole);
    box.setDefaultButton(acceptBtn);
    box.exec();

    QString err;
    if (box.clickedButton() == acceptBtn) {
        if (!backend_->respondFriendRequest(requester, true, err)) {
            QMessageBox::warning(
                this,
                UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Friend request")),
                err.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("同意失败"), QStringLiteral("Accept failed"))
                    : err);
            return;
        }

        for (int i = model_->rowCount() - 1; i >= 0; --i) {
            if (model_->item(i)->data(IdRole).toString().startsWith(QStringLiteral("__"))) {
                model_->removeRow(i);
            }
        }
        bool exists = false;
        for (int i = 0; i < model_->rowCount(); ++i) {
            if (model_->item(i)->data(IdRole).toString() == requester) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            auto *item = new QStandardItem();
            item->setData(requester, IdRole);
            item->setData(requester, TitleRole);
            item->setData(UiSettings::Tr(QStringLiteral("点击开始聊天"),
                                         QStringLiteral("Click to chat")),
                          PreviewRole);
            item->setData(QString(), TimeRole);
            item->setData(0, UnreadRole);
            item->setData(true, GreyBadgeRole);
            item->setData(false, HasTagRole);
            item->setData(false, IsGroupRole);
            model_->insertRow(0, item);
            listView_->setCurrentIndex(model_->index(0, 0));
        }

        QMessageBox::information(
            this,
            UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Friend request")),
            UiSettings::Tr(QStringLiteral("已添加好友：%1").arg(requester),
                           QStringLiteral("Friend added: %1").arg(requester)));
        return;
    }

    if (box.clickedButton() == rejectBtn) {
        if (!backend_->respondFriendRequest(requester, false, err)) {
            QMessageBox::warning(
                this,
                UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Friend request")),
                err.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("拒绝失败"), QStringLiteral("Reject failed"))
                    : err);
            return;
        }
        QMessageBox::information(
            this,
            UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Friend request")),
            UiSettings::Tr(QStringLiteral("已拒绝：%1").arg(requester),
                           QStringLiteral("Rejected: %1").arg(requester)));
        return;
    }

    if (box.clickedButton() == blockBtn) {
        if (!backend_->setUserBlocked(requester, true, err)) {
            QMessageBox::warning(
                this,
                UiSettings::Tr(QStringLiteral("拉黑"), QStringLiteral("Block")),
                err.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("拉黑失败"), QStringLiteral("Block failed"))
                    : err);
            return;
        }
        for (int i = model_->rowCount() - 1; i >= 0; --i) {
            if (model_->item(i)->data(IdRole).toString() == requester) {
                model_->removeRow(i);
            }
        }
        QMessageBox::information(
            this,
            UiSettings::Tr(QStringLiteral("拉黑"), QStringLiteral("Block")),
            UiSettings::Tr(QStringLiteral("已拉黑：%1").arg(requester),
                           QStringLiteral("Blocked: %1").arg(requester)));
        return;
    }
}

void MainListWindow::handleGroupInviteReceived(const QString &groupId, const QString &fromUser, const QString &messageId) {
    Q_UNUSED(messageId);
    if (!backend_ || !model_) {
        return;
    }
    if (tray_) {
        const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
        const QString msg =
            allowPreview
                ? UiSettings::Tr(QStringLiteral("来自：%1\n群 ID：%2").arg(fromUser, groupId),
                                 QStringLiteral("From: %1\nGroup ID: %2").arg(fromUser, groupId))
                : UiSettings::Tr(QStringLiteral("你收到新的群邀请"),
                                 QStringLiteral("You received a new group invite"));
        showTrayMessage(UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Group invite")),
                        msg);
    }

    const QString detail = UiSettings::Tr(
        QStringLiteral("收到群邀请\n\n来自：%1\n群 ID：%2").arg(fromUser, groupId),
        QStringLiteral("Group invite\n\nFrom: %1\nGroup ID: %2").arg(fromUser, groupId));

    QMessageBox box(QMessageBox::Question,
                    UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Group invite")),
                    detail,
                    QMessageBox::NoButton, this);
    auto *joinBtn =
        box.addButton(UiSettings::Tr(QStringLiteral("加入"), QStringLiteral("Join")),
                      QMessageBox::AcceptRole);
    auto *copyBtn = box.addButton(
        UiSettings::Tr(QStringLiteral("复制群 ID"), QStringLiteral("Copy group ID")),
        QMessageBox::ActionRole);
    box.addButton(UiSettings::Tr(QStringLiteral("忽略"), QStringLiteral("Ignore")),
                  QMessageBox::RejectRole);
    box.setDefaultButton(joinBtn);
    box.exec();

    if (box.clickedButton() == copyBtn) {
        if (auto *cb = QGuiApplication::clipboard()) {
            cb->setText(groupId);
        }
        QMessageBox::information(
            this,
            UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Group invite")),
            UiSettings::Tr(QStringLiteral("群 ID 已复制"),
                           QStringLiteral("Group ID copied")));
        return;
    }

    if (box.clickedButton() != joinBtn) {
        return;
    }

    QString err;
    if (!backend_->joinGroup(groupId, err)) {
        QMessageBox::warning(
            this,
            UiSettings::Tr(QStringLiteral("加入群聊"), QStringLiteral("Join group")),
            err.isEmpty()
                ? UiSettings::Tr(QStringLiteral("加入失败"), QStringLiteral("Join failed"))
                : err);
        return;
    }

    int rowIndex = -1;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (model_->item(i)->data(IdRole).toString() == groupId) {
            rowIndex = i;
            break;
        }
    }
    if (rowIndex == -1) {
        auto *item = new QStandardItem();
        item->setData(groupId, IdRole);
        item->setData(QStringLiteral("群聊 %1").arg(groupId), TitleRole);
        item->setData(QStringLiteral("点击开始聊天"), PreviewRole);
        item->setData(QString(), TimeRole);
        item->setData(0, UnreadRole);
        item->setData(true, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(true, IsGroupRole);
        model_->insertRow(0, item);
        rowIndex = 0;
    } else {
        if (auto *item = model_->item(rowIndex)) {
            item->setData(true, IsGroupRole);
        }
    }

    listView_->setCurrentIndex(model_->index(rowIndex, 0));
    openChatForIndex(model_->index(rowIndex, 0));
    QMessageBox::information(this, QStringLiteral("群邀请"),
                             QStringLiteral("已加入群聊：%1").arg(groupId));
}

void MainListWindow::handleGroupNoticeReceived(const QString &groupId, const QString &text) {
    if (!model_) {
        return;
    }
    const QString preview =
        UiSettings::Tr(QStringLiteral("[系统] %1").arg(text),
                       QStringLiteral("[System] %1").arg(text));
    int rowIndex = -1;
    for (int i = 0; i < model_->rowCount(); ++i) {
        if (model_->item(i)->data(IdRole).toString() == groupId) {
            rowIndex = i;
            break;
        }
    }
    if (rowIndex == -1) {
        auto *item = new QStandardItem();
        item->setData(groupId, IdRole);
        item->setData(UiSettings::Tr(QStringLiteral("群聊 %1").arg(groupId),
                                     QStringLiteral("Group %1").arg(groupId)),
                      TitleRole);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(1, UnreadRole);
        item->setData(false, GreyBadgeRole);
        item->setData(false, HasTagRole);
        item->setData(true, IsGroupRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(true, IsGroupRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (chatWindows_.contains(groupId) && chatWindows_[groupId]) {
        chatWindows_[groupId]->appendSystemMessage(preview, now);
        if (rowIndex >= 0) {
            model_->item(rowIndex)->setData(0, UnreadRole);
        }
        return;
    }

    if (!tray_) {
        return;
    }
    const bool mainActive = isVisible() && !isMinimized() && isActiveWindow();
    if (mainActive) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString key = QStringLiteral("notice:") + groupId;
    const qint64 last = lastNotifyMs_.value(key, 0);
    if (nowMs - last < 2000) {
        return;
    }
    lastNotifyMs_.insert(key, nowMs);

    const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
    const QString notifyTitle =
        allowPreview ? UiSettings::Tr(QStringLiteral("群聊 %1").arg(groupId),
                                      QStringLiteral("Group %1").arg(groupId))
                     : UiSettings::Tr(QStringLiteral("群通知"),
                                      QStringLiteral("Group notice"));
    const QString notifyMsg =
        allowPreview ? preview : UiSettings::Tr(QStringLiteral("群成员变更"),
                                                QStringLiteral("Group membership changed"));
    showTrayMessage(notifyTitle, notifyMsg);
}

void MainListWindow::handleConnectionStateChanged(bool online, const QString &detail) {
    if (!connLabel_) {
        return;
    }
    connLabel_->setText(detail);
    connLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                  .arg(online ? Theme::accentGreen().name()
                                              : Theme::uiDangerRed().name()));
}
