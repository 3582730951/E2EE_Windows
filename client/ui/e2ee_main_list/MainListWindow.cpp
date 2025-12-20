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
#include <QSet>
#include <QGuiApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>

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
#include "ConversationDetailsDialog.h"
#include "NotificationCenterDialog.h"
#include "TrustPromptDialog.h"

namespace {

QString PinnedSettingsKey() { return QStringLiteral("ui/pinned_conversations"); }
QString ModePlaceholderId() { return QStringLiteral("__mode_placeholder__"); }

enum Roles {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    PreviewRole,
    TimeRole,
    UnreadRole,
    GreyBadgeRole,
    HasTagRole,
    IsGroupRole,
    LastActiveRole,
    PinnedRole
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
    static int sidebarWidth() { return 72; }
    static int rowHeight() { return 78; }
    static int radius() { return 14; }
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

QString SurfaceGradient(const QColor &base) {
    const bool light = (Theme::scheme() == Theme::Scheme::Light);
    const QColor top = base.lighter(light ? 103 : 108);
    const QColor bottom = base.darker(light ? 103 : 92);
    return QStringLiteral(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);")
        .arg(top.name(), bottom.name());
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

class ConversationProxyModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    enum class Mode {
        All = 0,
        PinnedOnly = 1,
        GroupsOnly = 2,
    };

    void setMode(Mode mode) {
        if (mode_ == mode) {
            return;
        }
        mode_ = mode;
        invalidateFilter();
        invalidate();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!idx.isValid()) {
            return false;
        }
        const QString id = idx.data(IdRole).toString();
        if (id.startsWith(QStringLiteral("__"))) {
            return true;
        }

        if (mode_ == Mode::PinnedOnly && !idx.data(PinnedRole).toBool()) {
            return false;
        }
        if (mode_ == Mode::GroupsOnly && !idx.data(IsGroupRole).toBool()) {
            return false;
        }

        const QRegularExpression re = filterRegularExpression();
        if (!re.isValid() || re.pattern().trimmed().isEmpty()) {
            return true;
        }

        const QString title = idx.data(TitleRole).toString();
        const QString preview = idx.data(PreviewRole).toString();
        return title.contains(re) || id.contains(re) || preview.contains(re);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        const bool leftPinned = left.data(PinnedRole).toBool();
        const bool rightPinned = right.data(PinnedRole).toBool();
        if (leftPinned != rightPinned) {
            return leftPinned < rightPinned;
        }

        const qint64 leftActive = left.data(LastActiveRole).toLongLong();
        const qint64 rightActive = right.data(LastActiveRole).toLongLong();
        if (leftActive != rightActive) {
            return leftActive < rightActive;
        }

        const QString leftTitle = left.data(TitleRole).toString();
        const QString rightTitle = right.data(TitleRole).toString();
        return QString::localeAwareCompare(leftTitle, rightTitle) < 0;
    }

private:
    Mode mode_{Mode::All};
};

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
        painter->drawRoundedRect(r, Tokens::radius(), Tokens::radius());

        const QString title = index.data(TitleRole).toString();
        const QString preview = index.data(PreviewRole).toString();
        const QString time = index.data(TimeRole).toString();
        const int unread = index.data(UnreadRole).toInt();
        const bool greyBadge = index.data(GreyBadgeRole).toBool();
        const bool hasTag = index.data(HasTagRole).toBool();
        const bool pinned = index.data(PinnedRole).toBool();

        // Avatar
        const int avatarSize = 48;
        const int avatarTop = r.top() + (r.height() - avatarSize) / 2;
        QRect avatarRect = QRect(r.left() + 14, avatarTop, avatarSize, avatarSize);
        painter->setBrush(avatarColorFor(title));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(avatarRect);

        // Text area
        int textLeft = avatarRect.right() + 12;
        QRect titleRect(textLeft, avatarRect.top() + 2, r.width() - textLeft - 84, 22);
        QRect previewRect(textLeft, titleRect.bottom() + 4, r.width() - textLeft - 84, 20);

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
        QRect timeRect(r.right() - 64, avatarRect.top() + 2, 60, 16);
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, time);

        // Pin indicator
        if (pinned) {
            const QColor iconColor = selected ? Tokens::textMain() : Tokens::textMuted();
            const QPixmap star = UiIcons::TintedSvg(QStringLiteral(":/mi/e2ee/ui/icons/star.svg"),
                                                    12, iconColor);
            painter->drawPixmap(QRect(r.right() - 80, timeRect.top() + 1, 12, 12), star);
        }

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
    resize(1180, 820);
    setMinimumSize(980, 640);

    loadPinned();

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Title bar with custom buttons.
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(48);
    titleBar->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::windowBg().name()));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 8, 10, 8);

    auto *titleLabel = new QLabel(QStringLiteral("E2EE"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; letter-spacing: 1px;")
                                  .arg(Tokens::textMain().name()));
    titleLayout->addWidget(titleLabel);
    connLabel_ = new QLabel(QStringLiteral(""), titleBar);
    connLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                  .arg(Tokens::textMuted().name()));
    titleLayout->addSpacing(10);
    titleLayout->addWidget(connLabel_);
    titleLayout->addStretch();
    auto *minBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/minimize.svg"), titleBar, Tokens::textSub());
    auto *funcBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/maximize.svg"), titleBar, Tokens::textSub());
    auto *closeBtn = titleButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), titleBar, Tokens::textSub());
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(funcBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(minBtn);
    titleLayout->addWidget(funcBtn);
    titleLayout->addSpacing(6);
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

    auto *brandMark = new QLabel(QStringLiteral("E2EE"), sidebar);
    brandMark->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    brandMark->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                 .arg(Tokens::textMain().name()));
    sideLayout->addWidget(brandMark, 0, Qt::AlignLeft);

    navBellBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/bell.svg"), sidebar, false);
    navBellBtn_->setFixedSize(32, 32);
    navBellBtn_->setToolTip(UiSettings::Tr(QStringLiteral("通知中心"), QStringLiteral("Notifications")));
    navBellBtn_->setAccessibleName(navBellBtn_->toolTip());
    sideLayout->addWidget(navBellBtn_, 0, Qt::AlignLeft);
    connect(navBellBtn_, &QPushButton::clicked, this, &MainListWindow::handleNotificationCenter);

    auto *avatar = new QLabel(sidebar);
    avatar->setFixedSize(46, 46);
    avatar->setStyleSheet(QStringLiteral("background: %1; border-radius: 23px;")
                              .arg(Tokens::accentBlue().name()));
    sideLayout->addWidget(avatar, 0, Qt::AlignLeft);

    navAllBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/chat.svg"), sidebar, true);
    navAllBtn_->setToolTip(UiSettings::Tr(QStringLiteral("会话"), QStringLiteral("Chats")));
    navAllBtn_->setAccessibleName(navAllBtn_->toolTip());
    sideLayout->addWidget(navAllBtn_, 0, Qt::AlignLeft);
    connect(navAllBtn_, &QPushButton::clicked, this, [this]() {
        setConversationListMode(ConversationListMode::All);
    });

    navPinnedBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/star.svg"), sidebar, false);
    navPinnedBtn_->setToolTip(UiSettings::Tr(QStringLiteral("置顶"), QStringLiteral("Pinned")));
    navPinnedBtn_->setAccessibleName(navPinnedBtn_->toolTip());
    sideLayout->addWidget(navPinnedBtn_, 0, Qt::AlignLeft);
    connect(navPinnedBtn_, &QPushButton::clicked, this, [this]() {
        setConversationListMode(ConversationListMode::PinnedOnly);
    });

    navGroupsBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/group.svg"), sidebar, false);
    navGroupsBtn_->setToolTip(UiSettings::Tr(QStringLiteral("群聊"), QStringLiteral("Groups")));
    navGroupsBtn_->setAccessibleName(navGroupsBtn_->toolTip());
    sideLayout->addWidget(navGroupsBtn_, 0, Qt::AlignLeft);
    connect(navGroupsBtn_, &QPushButton::clicked, this, [this]() {
        setConversationListMode(ConversationListMode::GroupsOnly);
    });

    navFilesBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/file-upload.svg"), sidebar, false);
    navFilesBtn_->setToolTip(UiSettings::Tr(QStringLiteral("共享文件"), QStringLiteral("Shared files")));
    navFilesBtn_->setAccessibleName(navFilesBtn_->toolTip());
    sideLayout->addWidget(navFilesBtn_, 0, Qt::AlignLeft);
    connect(navFilesBtn_, &QPushButton::clicked, this, [this]() {
        if (!backend_ || !model_) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("未连接后端"),
                                       QStringLiteral("Backend is offline")),
                        Toast::Level::Warning);
            return;
        }
        const QString id = embeddedConvId_.trimmed();
        if (id.isEmpty() || id.startsWith(QStringLiteral("__"))) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("请先选择一个会话"),
                                       QStringLiteral("Select a chat first")),
                        Toast::Level::Info);
            return;
        }
        auto *item = findItemById(id);
        if (!item) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("会话不存在"),
                                       QStringLiteral("Chat not found")),
                        Toast::Level::Warning);
            return;
        }
        ConversationDetailsDialog dlg(backend_,
                                      id,
                                      item->data(TitleRole).toString(),
                                      item->data(IsGroupRole).toBool(),
                                      this);
        dlg.setStartPage(ConversationDetailsDialog::StartPage::Files);
        dlg.exec();
    });

    navSettingsBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/settings.svg"), sidebar, false);
    navSettingsBtn_->setToolTip(UiSettings::Tr(QStringLiteral("设置"), QStringLiteral("Settings")));
    navSettingsBtn_->setAccessibleName(navSettingsBtn_->toolTip());
    sideLayout->addWidget(navSettingsBtn_, 0, Qt::AlignLeft);
    connect(navSettingsBtn_, &QPushButton::clicked, this, &MainListWindow::handleSettings);
    sideLayout->addStretch();

    navMenuBtn_ = navButtonSvg(QStringLiteral(":/mi/e2ee/ui/icons/more.svg"), sidebar, false);
    navMenuBtn_->setToolTip(UiSettings::Tr(QStringLiteral("菜单"), QStringLiteral("Menu")));
    navMenuBtn_->setAccessibleName(navMenuBtn_->toolTip());
    sideLayout->addWidget(navMenuBtn_, 0, Qt::AlignLeft | Qt::AlignBottom);
    connect(navMenuBtn_, &QPushButton::clicked, this, [this]() { showAppMenu(); });

    // Right main area
    auto *mainArea = new QWidget(body);
    mainArea->setStyleSheet(SurfaceGradient(Tokens::windowBg()));
    auto *mainLayout2 = new QVBoxLayout(mainArea);
    mainLayout2->setContentsMargins(12, 12, 12, 12);
    mainLayout2->setSpacing(10);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);

    auto *searchBox = new QFrame(mainArea);
    searchBox->setFixedHeight(38);
    searchBox->setStyleSheet(
        QStringLiteral(
            "QFrame { background: %1; border-radius: 19px; border: 1px solid %2; }"
            "QLineEdit { background: transparent; border: none; color: %3; font-size: 13px; }"
            "QLabel { color: %4; font-size: 13px; }")
            .arg(Tokens::searchBg().name(),
                 Theme::uiBorder().name(),
                 Tokens::textMain().name(),
                 Tokens::textMuted().name()));
    auto *sLayout = new QHBoxLayout(searchBox);
    sLayout->setContentsMargins(12, 7, 12, 7);
    sLayout->setSpacing(8);
    auto *searchIcon = new QLabel(searchBox);
    searchIcon->setFixedSize(16, 16);
    searchIcon->setPixmap(UiIcons::TintedSvg(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"),
                                             16, Tokens::textMuted()));
    searchIcon->setAlignment(Qt::AlignCenter);
    searchEdit_ = new QLineEdit(searchBox);
    searchEdit_->setPlaceholderText(UiSettings::Tr(QStringLiteral("搜索"), QStringLiteral("Search")));
    searchEdit_->setClearButtonEnabled(true);
    connect(searchEdit_, &QLineEdit::textChanged, this, &MainListWindow::handleSearchTextChanged);
    connect(searchEdit_, &QLineEdit::returnPressed, this, [this]() {
        if (!listView_ || !listView_->model()) {
            return;
        }
        QModelIndex idx = listView_->currentIndex();
        if (!idx.isValid() && listView_->model()->rowCount() > 0) {
            idx = listView_->model()->index(0, 0);
        }
        if (!idx.isValid()) {
            return;
        }
        listView_->setCurrentIndex(idx);
        previewChatForIndex(idx);
        if (embeddedChat_) {
            embeddedChat_->focusMessageInput();
        }
    });
    sLayout->addWidget(searchIcon);
    sLayout->addWidget(searchEdit_, 1);

    auto *plusBtn = new IconButton(QString(), mainArea);
    plusBtn->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/plus.svg"), 18);
    plusBtn->setFocusPolicy(Qt::NoFocus);
    plusBtn->setFixedSize(38, 38);
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

    // Conversation list
    listView_ = new QListView(mainArea);
    listView_->setFrameShape(QFrame::NoFrame);
    listView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView_->setSpacing(0);
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setStyleSheet(
        QStringLiteral(
            "QListView { background: transparent; outline: none; border: 1px solid transparent; border-radius: 12px; }"
            "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }")
            .arg(Theme::uiScrollBarHandle().name(),
                 Theme::uiScrollBarHandleHover().name()));
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
        item->setData(!id.startsWith(QStringLiteral("__")) && pinnedIds_.contains(id), PinnedRole);
        item->setData(id.startsWith(QStringLiteral("__")) ? static_cast<qint64>(-1) : static_cast<qint64>(0),
                      LastActiveRole);
        model_->appendRow(item);
    };

    proxyModel_ = new ConversationProxyModel(listView_);
    proxyModel_->setSourceModel(model_);
    proxyModel_->setDynamicSortFilter(true);
    proxyModel_->setSortRole(LastActiveRole);
    proxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel_->sort(0, Qt::DescendingOrder);

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

                     updateModePlaceholder();
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

    listView_->setModel(proxyModel_);
    listView_->setItemDelegate(new ConversationDelegate(listView_));
    connect(listView_, &QListView::clicked, this, &MainListWindow::previewChatForIndex);
    connect(listView_, &QListView::activated, this, &MainListWindow::previewChatForIndex);
    if (auto *selection = listView_->selectionModel()) {
        connect(selection, &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex &current, const QModelIndex &) { previewChatForIndex(current); });
    }

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
        const bool pinned = idx.data(PinnedRole).toBool();
        if (isGroup) {
            QMenu menu(this);
            UiStyle::ApplyMenuStyle(menu);
            QAction *openInWindow =
                menu.addAction(UiSettings::Tr(QStringLiteral("在新窗口打开"),
                                              QStringLiteral("Open in new window")));
            QAction *pinAction = menu.addAction(
                pinned ? UiSettings::Tr(QStringLiteral("取消置顶"), QStringLiteral("Unpin"))
                       : UiSettings::Tr(QStringLiteral("置顶"), QStringLiteral("Pin")));
            menu.addSeparator();
            QAction *copyId = menu.addAction(QStringLiteral("复制群 ID"));
            QAction *invite = menu.addAction(QStringLiteral("邀请成员..."));
            QAction *members = menu.addAction(QStringLiteral("查看成员"));
            menu.addSeparator();
            QAction *leave = menu.addAction(QStringLiteral("退出群聊"));
            QAction *picked = menu.exec(listView_->viewport()->mapToGlobal(pos));
            if (!picked) {
                return;
            }
            if (picked == openInWindow) {
                openChatForIndex(idx);
                return;
            }
            if (picked == pinAction) {
                togglePinnedForId(id);
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
                for (int i = model_->rowCount() - 1; i >= 0; --i) {
                    if (model_->item(i)->data(IdRole).toString() == id) {
                        model_->removeRow(i);
                        break;
                    }
                }
                if (embeddedChat_ && embeddedConvId_ == id) {
                    embeddedConvId_.clear();
                    embeddedChat_->setConversation(QString(),
                                                   UiSettings::Tr(QStringLiteral("请选择会话"),
                                                                  QStringLiteral("Select a chat")),
                                                   false);
                }
                return;
            }
            return;
        }
        QMenu menu(this);
        UiStyle::ApplyMenuStyle(menu);
        QAction *openInWindow =
            menu.addAction(UiSettings::Tr(QStringLiteral("在新窗口打开"),
                                          QStringLiteral("Open in new window")));
        QAction *pinAction = menu.addAction(
            pinned ? UiSettings::Tr(QStringLiteral("取消置顶"), QStringLiteral("Unpin"))
                   : UiSettings::Tr(QStringLiteral("置顶"), QStringLiteral("Pin")));
        menu.addSeparator();
        QAction *edit = menu.addAction(QStringLiteral("修改备注"));
        QAction *del = menu.addAction(QStringLiteral("删除好友"));
        menu.addSeparator();
        QAction *block = menu.addAction(QStringLiteral("拉黑"));
        QAction *unblock = menu.addAction(QStringLiteral("取消拉黑"));
        QAction *picked = menu.exec(listView_->viewport()->mapToGlobal(pos));
        if (!picked) {
            return;
        }

        if (picked == openInWindow) {
            openChatForIndex(idx);
            return;
        }
        if (picked == pinAction) {
            togglePinnedForId(id);
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
            if (auto *item = findItemById(id)) {
                item->setData(display, TitleRole);
                item->setData(QStringLiteral("备注已更新"), PreviewRole);
                item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
                item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
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
            for (int i = model_->rowCount() - 1; i >= 0; --i) {
                if (model_->item(i)->data(IdRole).toString() == id) {
                    model_->removeRow(i);
                    break;
                }
            }
            if (embeddedChat_ && embeddedConvId_ == id) {
                embeddedConvId_.clear();
                embeddedChat_->setConversation(QString(),
                                               UiSettings::Tr(QStringLiteral("请选择会话"),
                                                              QStringLiteral("Select a chat")),
                                               false);
            }
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
                for (int i = model_->rowCount() - 1; i >= 0; --i) {
                    if (model_->item(i)->data(IdRole).toString() == id) {
                        model_->removeRow(i);
                        break;
                    }
                }
                if (embeddedChat_ && embeddedConvId_ == id) {
                    embeddedConvId_.clear();
                    embeddedChat_->setConversation(QString(),
                                                   UiSettings::Tr(QStringLiteral("请选择会话"),
                                                                  QStringLiteral("Select a chat")),
                                                   false);
                }
            }
            return;
        }
    });

    auto *splitter = new QSplitter(Qt::Horizontal, mainArea);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(
        QStringLiteral("QSplitter::handle { background: %1; }")
            .arg(Theme::uiBorder().name()));

    auto *listPanel = new QWidget(splitter);
    auto *listPanelLayout = new QVBoxLayout(listPanel);
    listPanelLayout->setContentsMargins(0, 0, 0, 0);
    listPanelLayout->setSpacing(10);
    listPanelLayout->addLayout(searchRow);
    listView_->setMinimumWidth(320);
    listPanelLayout->addWidget(listView_, 1);
    splitter->addWidget(listPanel);

    embeddedChat_ = new ChatWindow(backend_, splitter);
    embeddedChat_->setEmbeddedMode(true);
    embeddedChat_->setConversation(QString(),
                                   UiSettings::Tr(QStringLiteral("请选择会话"),
                                                  QStringLiteral("Select a chat")),
                                   false);
    setTabOrder(listView_, embeddedChat_);
    splitter->addWidget(embeddedChat_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>{360, 800});

    mainLayout2->addWidget(splitter, 1);

    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(mainArea, 1);

    rootLayout->addWidget(body);

    setCentralWidget(central);

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

void MainListWindow::handleSettings() {
    SettingsDialog dlg(this);
    if (backend_) {
        dlg.setClientConfigPath(backend_->configPath());
    }
    dlg.exec();
}

void MainListWindow::handleNotificationCenter() {
    if (!backend_) {
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("未连接后端"),
                                   QStringLiteral("Backend is offline")),
                    Toast::Level::Warning);
        return;
    }

    auto refreshFromBackend = [&](NotificationCenterDialog &dlg) {
        QString err;
        const auto list = backend_->listFriendRequests(err);
        pendingFriendRequests_.clear();
        QVector<NotificationCenterDialog::FriendRequest> reqs;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        for (const auto &r : list) {
            NotificationCenterDialog::FriendRequest fr;
            fr.requester = r.requesterUsername.trimmed();
            fr.remark = r.requesterRemark.trimmed();
            fr.receivedMs = nowMs;
            if (!fr.requester.isEmpty()) {
                pendingFriendRequests_.insert(fr.requester, fr.remark);
                reqs.push_back(fr);
            }
        }
        updateNotificationBadge();
        dlg.setFriendRequests(reqs);

        if (!err.trimmed().isEmpty()) {
            Toast::Show(&dlg, err.trimmed(), Toast::Level::Warning);
        }
    };

    QVector<NotificationCenterDialog::GroupInvite> invites;
    invites.reserve(pendingGroupInvites_.size());
    for (const auto &inv : pendingGroupInvites_) {
        NotificationCenterDialog::GroupInvite v;
        v.groupId = inv.groupId;
        v.fromUser = inv.fromUser;
        v.messageId = inv.messageId;
        v.receivedMs = inv.receivedMs;
        invites.push_back(v);
    }

    NotificationCenterDialog dlg(this);
    dlg.setGroupInvites(invites);
    refreshFromBackend(dlg);

    connect(&dlg, &NotificationCenterDialog::refreshRequested, this, [this, &dlg, refreshFromBackend]() mutable {
        refreshFromBackend(dlg);
    });

    connect(&dlg, &NotificationCenterDialog::friendRequestActionRequested, this,
            [this, &dlg](const QString &requester,
                         NotificationCenterDialog::FriendRequestAction action) {
                const QString who = requester.trimmed();
                if (who.isEmpty() || !backend_) {
                    return;
                }

                QString err;
                const auto fail = [&](const QString &fallback) {
                    Toast::Show(&dlg, err.isEmpty() ? fallback : err, Toast::Level::Error);
                };

                if (action == NotificationCenterDialog::FriendRequestAction::Accept) {
                    if (!backend_->respondFriendRequest(who, true, err)) {
                        fail(UiSettings::Tr(QStringLiteral("同意失败"),
                                            QStringLiteral("Accept failed")));
                        return;
                    }
                    pendingFriendRequests_.remove(who);
                    updateNotificationBadge();
                    dlg.removeFriendRequest(who);
                    backend_->requestFriendList();
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("已添加好友：%1").arg(who),
                                               QStringLiteral("Friend added: %1").arg(who)),
                                Toast::Level::Success);
                    return;
                }

                if (action == NotificationCenterDialog::FriendRequestAction::Reject) {
                    if (!backend_->respondFriendRequest(who, false, err)) {
                        fail(UiSettings::Tr(QStringLiteral("拒绝失败"),
                                            QStringLiteral("Reject failed")));
                        return;
                    }
                    pendingFriendRequests_.remove(who);
                    updateNotificationBadge();
                    dlg.removeFriendRequest(who);
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("已拒绝：%1").arg(who),
                                               QStringLiteral("Rejected: %1").arg(who)),
                                Toast::Level::Info);
                    return;
                }

                if (action == NotificationCenterDialog::FriendRequestAction::Block) {
                    if (!backend_->setUserBlocked(who, true, err)) {
                        fail(UiSettings::Tr(QStringLiteral("拉黑失败"),
                                            QStringLiteral("Block failed")));
                        return;
                    }
                    QString rejectErr;
                    backend_->respondFriendRequest(who, false, rejectErr);  // best-effort cleanup
                    pendingFriendRequests_.remove(who);
                    updateNotificationBadge();
                    dlg.removeFriendRequest(who);
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("已拉黑：%1").arg(who),
                                               QStringLiteral("Blocked: %1").arg(who)),
                                Toast::Level::Success);
                }
            });

    connect(&dlg, &NotificationCenterDialog::groupInviteActionRequested, this,
            [this, &dlg](const QString &groupId,
                         const QString &fromUser,
                         const QString &messageId,
                         NotificationCenterDialog::GroupInviteAction action) {
                const QString gid = groupId.trimmed();
                if (gid.isEmpty()) {
                    return;
                }

                if (action == NotificationCenterDialog::GroupInviteAction::CopyId) {
                    if (auto *cb = QGuiApplication::clipboard()) {
                        cb->setText(gid);
                    }
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("群 ID 已复制"),
                                               QStringLiteral("Group ID copied")),
                                Toast::Level::Info);
                    return;
                }

                auto removeInvite = [&]() {
                    const QString mid = messageId.trimmed();
                    for (int i = pendingGroupInvites_.size() - 1; i >= 0; --i) {
                        const bool matchId = pendingGroupInvites_[i].groupId == gid;
                        const bool matchMsg = mid.isEmpty() || pendingGroupInvites_[i].messageId == mid;
                        if (matchId && matchMsg) {
                            pendingGroupInvites_.removeAt(i);
                        }
                    }
                    updateNotificationBadge();
                    dlg.removeGroupInvite(gid, mid);
                };

                if (action == NotificationCenterDialog::GroupInviteAction::Ignore) {
                    removeInvite();
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("已忽略群邀请"),
                                               QStringLiteral("Invite ignored")),
                                Toast::Level::Info);
                    return;
                }

                if (!backend_) {
                    Toast::Show(&dlg,
                                UiSettings::Tr(QStringLiteral("未连接后端"),
                                               QStringLiteral("Backend is offline")),
                                Toast::Level::Warning);
                    return;
                }

                QString err;
                if (!backend_->joinGroup(gid, err)) {
                    Toast::Show(&dlg,
                                err.isEmpty()
                                    ? UiSettings::Tr(QStringLiteral("加入失败"),
                                                     QStringLiteral("Join failed"))
                                    : err,
                                Toast::Level::Error);
                    return;
                }

                removeInvite();

                int rowIndex = -1;
                for (int i = 0; i < model_->rowCount(); ++i) {
                    if (model_->item(i)->data(IdRole).toString() == gid) {
                        rowIndex = i;
                        break;
                    }
                }
                if (rowIndex == -1) {
                    auto *item = new QStandardItem();
                    item->setData(gid, IdRole);
                    item->setData(UiSettings::Tr(QStringLiteral("群聊 %1").arg(gid),
                                                 QStringLiteral("Group %1").arg(gid)),
                                  TitleRole);
                    item->setData(UiSettings::Tr(QStringLiteral("点击开始聊天"),
                                                 QStringLiteral("Click to chat")),
                                  PreviewRole);
                    item->setData(QString(), TimeRole);
                    item->setData(0, UnreadRole);
                    item->setData(true, GreyBadgeRole);
                    item->setData(false, HasTagRole);
                    item->setData(true, IsGroupRole);
                    item->setData(pinnedIds_.contains(gid), PinnedRole);
                    item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
                    model_->insertRow(0, item);
                } else {
                    if (auto *item = model_->item(rowIndex)) {
                        item->setData(true, IsGroupRole);
                    }
                }

                updateModePlaceholder();
                selectConversation(gid);
                const QModelIndex viewIndex = viewIndexForId(gid);
                if (viewIndex.isValid()) {
                    previewChatForIndex(viewIndex);
                }

                const QString hint = fromUser.trimmed().isEmpty()
                                         ? UiSettings::Tr(QStringLiteral("已加入群聊：%1").arg(gid),
                                                          QStringLiteral("Joined group: %1").arg(gid))
                                         : UiSettings::Tr(QStringLiteral("已加入群聊：%1（来自 %2）").arg(gid, fromUser.trimmed()),
                                                          QStringLiteral("Joined group: %1 (from %2)").arg(gid, fromUser.trimmed()));
                Toast::Show(this, hint, Toast::Level::Success);
                dlg.accept();
            });

    dlg.exec();
}

void MainListWindow::loadPinned() {
    pinnedIds_.clear();
    QSettings s;
    const QStringList list = s.value(PinnedSettingsKey()).toStringList();
    for (const auto &id : list) {
        const QString trimmed = id.trimmed();
        if (!trimmed.isEmpty()) {
            pinnedIds_.insert(trimmed);
        }
    }
}

void MainListWindow::savePinned() const {
    QSettings s;
    QStringList list = pinnedIds_.values();
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    s.setValue(PinnedSettingsKey(), list);
    s.sync();
}

void MainListWindow::togglePinnedForId(const QString &id) {
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("__"))) {
        return;
    }

    const bool pinned = pinnedIds_.contains(trimmed);
    if (pinned) {
        pinnedIds_.remove(trimmed);
    } else {
        pinnedIds_.insert(trimmed);
    }
    savePinned();

    if (auto *item = findItemById(trimmed)) {
        item->setData(!pinned, PinnedRole);
    }

    if (proxyModel_) {
        proxyModel_->invalidate();
        proxyModel_->sort(0, Qt::DescendingOrder);
    }
    updateModePlaceholder();

    Toast::Show(this,
                pinned
                    ? UiSettings::Tr(QStringLiteral("已取消置顶"), QStringLiteral("Unpinned"))
                    : UiSettings::Tr(QStringLiteral("已置顶"), QStringLiteral("Pinned")),
                Toast::Level::Success);
}

void MainListWindow::setConversationListMode(ConversationListMode mode) {
    listMode_ = mode;
    updateNavSelection();
    if (proxyModel_) {
        auto *proxy = static_cast<ConversationProxyModel *>(proxyModel_);
        ConversationProxyModel::Mode proxyMode = ConversationProxyModel::Mode::All;
        switch (mode) {
            case ConversationListMode::PinnedOnly:
                proxyMode = ConversationProxyModel::Mode::PinnedOnly;
                break;
            case ConversationListMode::GroupsOnly:
                proxyMode = ConversationProxyModel::Mode::GroupsOnly;
                break;
            case ConversationListMode::All:
            default:
                proxyMode = ConversationProxyModel::Mode::All;
                break;
        }
        proxy->setMode(proxyMode);
        proxy->sort(0, Qt::DescendingOrder);
    }
    updateModePlaceholder();
}

void MainListWindow::updateModePlaceholder() {
    if (!model_ || !proxyModel_) {
        return;
    }

    const QString pid = ModePlaceholderId();
    auto findRow = [&]() -> int {
        for (int i = 0; i < model_->rowCount(); ++i) {
            if (auto *it = model_->item(i)) {
                if (it->data(IdRole).toString() == pid) {
                    return i;
                }
            }
        }
        return -1;
    };

    const bool isPinnedOnly = (listMode_ == ConversationListMode::PinnedOnly);
    const bool isGroupsOnly = (listMode_ == ConversationListMode::GroupsOnly);

    int realCount = 0;
    if (isPinnedOnly || isGroupsOnly) {
        for (int i = 0; i < model_->rowCount(); ++i) {
            auto *it = model_->item(i);
            if (!it) {
                continue;
            }
            const QString id = it->data(IdRole).toString();
            if (id.startsWith(QStringLiteral("__"))) {
                continue;
            }
            if (isPinnedOnly && it->data(PinnedRole).toBool()) {
                ++realCount;
            } else if (isGroupsOnly && it->data(IsGroupRole).toBool()) {
                ++realCount;
            }
        }
    }

    const bool needPlaceholder = (isPinnedOnly || isGroupsOnly) && (realCount == 0);
    const int existingRow = findRow();

    if (!needPlaceholder) {
        if (existingRow >= 0) {
            model_->removeRow(existingRow);
        }
        return;
    }

    const QString title =
        isPinnedOnly
            ? UiSettings::Tr(QStringLiteral("暂无置顶"), QStringLiteral("No pinned chats"))
            : UiSettings::Tr(QStringLiteral("暂无群聊"), QStringLiteral("No groups"));
    const QString preview =
        isPinnedOnly
            ? UiSettings::Tr(QStringLiteral("右键会话 -> 置顶"), QStringLiteral("Right-click a chat to pin"))
            : UiSettings::Tr(QStringLiteral("使用 + 创建/加入群聊"), QStringLiteral("Use + to create/join a group"));

    QStandardItem *item = nullptr;
    if (existingRow >= 0) {
        item = model_->item(existingRow);
    } else {
        item = new QStandardItem();
        model_->insertRow(0, item);
    }
    if (!item) {
        return;
    }
    item->setData(pid, IdRole);
    item->setData(title, TitleRole);
    item->setData(preview, PreviewRole);
    item->setData(QString(), TimeRole);
    item->setData(0, UnreadRole);
    item->setData(true, GreyBadgeRole);
    item->setData(false, HasTagRole);
    item->setData(false, IsGroupRole);
    item->setData(false, PinnedRole);
    item->setData(static_cast<qint64>(-1), LastActiveRole);

    if (proxyModel_) {
        proxyModel_->invalidate();
        proxyModel_->sort(0, Qt::DescendingOrder);
    }
}

void MainListWindow::updateNavSelection() {
    auto apply = [&](IconButton *btn, bool selected) {
        if (!btn) {
            return;
        }
        QColor baseBg = selected ? Tokens::hoverBg() : QColor(0, 0, 0, 0);
        btn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(), baseBg,
                       Tokens::hoverBg(), Tokens::selectedBg());
    };

    apply(navAllBtn_, listMode_ == ConversationListMode::All);
    apply(navPinnedBtn_, listMode_ == ConversationListMode::PinnedOnly);
    apply(navGroupsBtn_, listMode_ == ConversationListMode::GroupsOnly);
}

void MainListWindow::updateNotificationBadge() {
    if (!navBellBtn_) {
        return;
    }
    const int count = pendingFriendRequests_.size() + pendingGroupInvites_.size();
    if (count <= 0) {
        if (bellBadge_) {
            bellBadge_->hide();
        }
        return;
    }

    if (!bellBadge_) {
        bellBadge_ = new QLabel(navBellBtn_);
        bellBadge_->setFixedSize(8, 8);
        bellBadge_->setAttribute(Qt::WA_TransparentForMouseEvents);
        bellBadge_->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 4px;")
                .arg(Theme::uiBadgeRed().name()));
        bellBadge_->move(navBellBtn_->width() - 12, 6);
    }
    bellBadge_->raise();
    bellBadge_->show();
}

void MainListWindow::showAppMenu() {
    if (!appMenu_) {
        appMenu_ = new QMenu(this);
        UiStyle::ApplyMenuStyle(*appMenu_);

        QAction *notify =
            appMenu_->addAction(UiSettings::Tr(QStringLiteral("通知中心"),
                                              QStringLiteral("Notifications")));
        QAction *settings =
            appMenu_->addAction(UiSettings::Tr(QStringLiteral("设置"),
                                              QStringLiteral("Settings")));
        QAction *deviceMgr = appMenu_->addAction(QStringLiteral("设备管理"));
        appMenu_->addSeparator();
        QAction *about =
            appMenu_->addAction(UiSettings::Tr(QStringLiteral("关于"),
                                              QStringLiteral("About")));
        QAction *exit =
            appMenu_->addAction(UiSettings::Tr(QStringLiteral("退出"),
                                              QStringLiteral("Exit")));

        connect(notify, &QAction::triggered, this, &MainListWindow::handleNotificationCenter);
        connect(settings, &QAction::triggered, this, &MainListWindow::handleSettings);
        connect(deviceMgr, &QAction::triggered, this, &MainListWindow::handleDeviceManager);
        connect(about, &QAction::triggered, this, [this]() {
            QMessageBox::information(this,
                                     UiSettings::Tr(QStringLiteral("关于"),
                                                    QStringLiteral("About")),
                                     UiSettings::Tr(QStringLiteral("MI E2EE 客户端（Qt UI）"),
                                                    QStringLiteral("MI E2EE Client (Qt UI)")));
        });
        connect(exit, &QAction::triggered, this, [this]() { close(); });
    }

    const QPoint anchor = navMenuBtn_ ? navMenuBtn_->mapToGlobal(QPoint(0, navMenuBtn_->height()))
                                      : QCursor::pos();
    appMenu_->exec(anchor);
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

QStandardItem *MainListWindow::findItemById(const QString &id) const {
    if (!model_) {
        return nullptr;
    }
    for (int i = 0; i < model_->rowCount(); ++i) {
        auto *item = model_->item(i);
        if (!item) {
            continue;
        }
        if (item->data(IdRole).toString() == id) {
            return item;
        }
    }
    return nullptr;
}

QModelIndex MainListWindow::viewIndexForId(const QString &id) const {
    if (!listView_ || !listView_->model()) {
        return QModelIndex();
    }
    auto *viewModel = listView_->model();
    for (int i = 0; i < viewModel->rowCount(); ++i) {
        const QModelIndex idx = viewModel->index(i, 0);
        if (idx.data(IdRole).toString() == id) {
            return idx;
        }
    }
    return QModelIndex();
}

void MainListWindow::selectConversation(const QString &id) {
    const QModelIndex idx = viewIndexForId(id);
    if (!idx.isValid()) {
        return;
    }
    listView_->setCurrentIndex(idx);
    listView_->scrollTo(idx);
}

void MainListWindow::previewChatForIndex(const QModelIndex &index) {
    if (!embeddedChat_) {
        return;
    }

    if (!index.isValid()) {
        embeddedConvId_.clear();
        embeddedChat_->setConversation(QString(),
                                       UiSettings::Tr(QStringLiteral("请选择会话"),
                                                      QStringLiteral("Select a chat")),
                                       false);
        return;
    }

    const QString id = index.data(IdRole).toString();
    if (id.startsWith(QStringLiteral("__"))) {
        embeddedConvId_.clear();
        embeddedChat_->setConversation(QString(),
                                       UiSettings::Tr(QStringLiteral("请选择会话"),
                                                      QStringLiteral("Select a chat")),
                                       false);
        return;
    }

    const QString title = index.data(TitleRole).toString();
    const bool isGroup = index.data(IsGroupRole).toBool();

    const bool changing = (embeddedConvId_ != id);
    embeddedConvId_ = id;
    embeddedChat_->setConversation(id, title, isGroup);
    if (auto *item = findItemById(id)) {
        item->setData(0, UnreadRole);
    }

    if (changing) {
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(embeddedChat_->graphicsEffect());
        if (!effect) {
            effect = new QGraphicsOpacityEffect(embeddedChat_);
            embeddedChat_->setGraphicsEffect(effect);
        }
        effect->setOpacity(0.0);
        auto *anim = new QPropertyAnimation(effect, "opacity", embeddedChat_);
        anim->setDuration(160);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
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

    if (auto *item = findItemById(id)) {
        item->setData(0, UnreadRole);
    }

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
        item->setData(pinnedIds_.contains(groupId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->insertRow(0, item);
        rowIndex = 0;
    }

    selectConversation(groupId);
    const QModelIndex viewIndex = viewIndexForId(groupId);
    if (viewIndex.isValid()) {
        previewChatForIndex(viewIndex);
    }
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
        item->setData(pinnedIds_.contains(groupId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->insertRow(0, item);
        rowIndex = 0;
    } else {
        if (auto *item = model_->item(rowIndex)) {
            item->setData(true, IsGroupRole);
        }
    }

    selectConversation(groupId);
    const QModelIndex viewIndex = viewIndexForId(groupId);
    if (viewIndex.isValid()) {
        previewChatForIndex(viewIndex);
    }
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
    if (!proxyModel_) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        proxyModel_->setFilterRegularExpression(QRegularExpression());
    } else {
        proxyModel_->setFilterRegularExpression(QRegularExpression(
            QRegularExpression::escape(trimmed),
            QRegularExpression::CaseInsensitiveOption));
    }
}

void MainListWindow::handleIncomingMessage(const QString &convId, bool isGroup, const QString &sender,
                                           const QString &messageId, const QString &text, bool isFile, qint64 fileSize) {
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
        item->setData(pinnedIds_.contains(convId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(isGroup, IsGroupRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    bool hasActiveView = false;
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->appendIncomingMessage(sender, messageId, text, isFile, fileSize, now);
        hasActiveView = true;
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendIncomingMessage(sender, messageId, text, isFile, fileSize, now);
        hasActiveView = true;
    }
    if (hasActiveView) {
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
        item->setData(pinnedIds_.contains(convId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(isGroup, IsGroupRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    bool hasActiveView = false;
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->appendIncomingSticker(sender, messageId, stickerId, now);
        hasActiveView = true;
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendIncomingSticker(sender, messageId, stickerId, now);
        hasActiveView = true;
    }
    if (hasActiveView) {
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
        item->setData(pinnedIds_.contains(convId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(isGroup, IsGroupRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->appendSyncedOutgoingMessage(messageId, text, isFile, fileSize, now);
    }
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
        item->setData(pinnedIds_.contains(convId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        item->setData(isGroup, IsGroupRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->appendSyncedOutgoingSticker(messageId, stickerId, now);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->appendSyncedOutgoingSticker(messageId, stickerId, now);
    }
}

void MainListWindow::handleDelivered(const QString &convId, const QString &messageId) {
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->markDelivered(messageId);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markDelivered(messageId);
    }
}

void MainListWindow::handleRead(const QString &convId, const QString &messageId) {
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->markRead(messageId);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markRead(messageId);
    }
}

void MainListWindow::handleTypingChanged(const QString &convId, bool typing) {
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->setTypingIndicator(typing);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setTypingIndicator(typing);
    }
}

void MainListWindow::handlePresenceChanged(const QString &convId, bool online) {
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->setPresenceIndicator(online);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setPresenceIndicator(online);
    }
}

void MainListWindow::handleMessageResent(const QString &convId, const QString &messageId) {
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->markSent(messageId);
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->markSent(messageId);
    }
}

void MainListWindow::handleFileSendFinished(const QString &convId, const QString &messageId,
                                            bool success, const QString &error) {
    bool updated = false;
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
        if (success) {
            embeddedChat_->markSent(messageId);
        } else {
            embeddedChat_->markFailed(messageId);
        }
        updated = true;
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
        if (success) {
            chatWindows_[convId]->markSent(messageId);
        } else {
            chatWindows_[convId]->markFailed(messageId);
        }
        updated = true;
    }
    if (!updated) {
        return;
    }
    if (success) {
        const QString msg = error.trimmed().isEmpty()
                                ? UiSettings::Tr(QStringLiteral("文件已发送"),
                                                 QStringLiteral("File sent"))
                                : UiSettings::Tr(QStringLiteral("提示：%1").arg(error),
                                                 QStringLiteral("Info: %1").arg(error));
        Toast::Show(this, msg, Toast::Level::Info);
        return;
    }
    const QString msg = error.trimmed().isEmpty()
                            ? UiSettings::Tr(QStringLiteral("发送失败"),
                                             QStringLiteral("Send failed"))
                            : UiSettings::Tr(QStringLiteral("发送失败：%1").arg(error),
                                             QStringLiteral("Send failed: %1").arg(error));
    Toast::Show(this, msg, Toast::Level::Error, 3200);
}

void MainListWindow::handleFileSaveFinished(const QString &convId, const QString &messageId,
                                            bool success, const QString &error, const QString &outPath) {
    bool updated = false;
    if (embeddedChat_ && embeddedConvId_ == convId) {
        embeddedChat_->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
        if (success) {
            embeddedChat_->setFileLocalPath(messageId, outPath);
        }
        updated = true;
    }
    if (chatWindows_.contains(convId) && chatWindows_[convId]) {
        chatWindows_[convId]->setFileTransferState(messageId, ChatWindow::FileTransferState::None);
        if (success) {
            chatWindows_[convId]->setFileLocalPath(messageId, outPath);
        }
        updated = true;
    }
    if (!updated) {
        return;
    }
    if (success) {
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

    const QString title = UiSettings::Tr(QStringLiteral("验证对端身份"),
                                         QStringLiteral("Verify peer identity"));
    const QString description = UiSettings::Tr(
        QStringLiteral("检测到需要验证对端身份（首次通信或对端密钥指纹变更）。\n"
                       "请通过线下可信渠道核对安全码/指纹后再继续。"),
        QStringLiteral("Peer identity verification required (first contact or peer key changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this, title, description, fingerprintHex, pin, input,
                            UiSettings::Tr(QStringLiteral("对端"), QStringLiteral("Peer")), peer)) {
        return;
    }

    QString err;
    if (!backend_->trustPendingPeer(input, err)) {
        QMessageBox::warning(this,
                             UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed")),
                             err.isEmpty()
                                 ? UiSettings::Tr(QStringLiteral("信任失败"), QStringLiteral("Trust failed"))
                                 : err);
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

    const QString title = UiSettings::Tr(QStringLiteral("验证服务器身份"),
                                         QStringLiteral("Verify server identity"));
    const QString description = UiSettings::Tr(
        QStringLiteral("检测到需要验证服务器身份（首次连接或证书指纹变更）。\n"
                       "请通过线下可信渠道核对安全码/指纹后再继续。"),
        QStringLiteral("Server identity verification required (first connection or certificate pin changed).\n"
                       "Verify via an out-of-band channel before trusting."));

    QString input;
    if (!PromptTrustWithSas(this, title, description, fingerprintHex, pin, input)) {
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
    const QString who = requester.trimmed();
    if (who.isEmpty()) {
        return;
    }

    pendingFriendRequests_.insert(who, remark.trimmed());
    updateNotificationBadge();

    if (tray_) {
        const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
        const QString msg = allowPreview
                                ? (remark.trimmed().isEmpty()
                                       ? UiSettings::Tr(QStringLiteral("收到好友申请：%1").arg(who),
                                                       QStringLiteral("Friend request: %1").arg(who))
                                       : UiSettings::Tr(QStringLiteral("收到好友申请：%1（%2）").arg(who, remark.trimmed()),
                                                       QStringLiteral("Friend request: %1 (%2)").arg(who, remark.trimmed())))
                                : UiSettings::Tr(QStringLiteral("你收到新的好友申请"),
                                                 QStringLiteral("You received a new friend request"));
        showTrayMessage(UiSettings::Tr(QStringLiteral("好友申请"),
                                       QStringLiteral("Friend request")),
                        msg);
    }

    const bool mainActive = isVisible() && !isMinimized() && isActiveWindow();
    if (!mainActive) {
        return;
    }

    Toast::Show(this,
                remark.trimmed().isEmpty()
                    ? UiSettings::Tr(QStringLiteral("收到好友申请：%1").arg(who),
                                     QStringLiteral("Friend request: %1").arg(who))
                    : UiSettings::Tr(QStringLiteral("收到好友申请：%1（%2）").arg(who, remark.trimmed()),
                                     QStringLiteral("Friend request: %1 (%2)").arg(who, remark.trimmed())),
                Toast::Level::Info);
}

void MainListWindow::handleGroupInviteReceived(const QString &groupId, const QString &fromUser, const QString &messageId) {
    const QString gid = groupId.trimmed();
    if (gid.isEmpty()) {
        return;
    }

    const QString from = fromUser.trimmed();
    const QString mid = messageId.trimmed();
    for (const auto &inv : pendingGroupInvites_) {
        if (inv.groupId != gid) {
            continue;
        }
        if (!mid.isEmpty()) {
            if (inv.messageId == mid) {
                updateNotificationBadge();
                return;
            }
            continue;
        }
        if (inv.messageId.trimmed().isEmpty() && inv.fromUser.trimmed() == from) {
            updateNotificationBadge();
            return;
        }
    }

    PendingGroupInvite inv;
    inv.groupId = gid;
    inv.fromUser = from;
    inv.messageId = mid;
    inv.receivedMs = QDateTime::currentMSecsSinceEpoch();
    pendingGroupInvites_.push_back(inv);
    updateNotificationBadge();

    if (tray_) {
        const bool allowPreview = trayPreviewAction_ && trayPreviewAction_->isChecked();
        QString msg;
        if (!allowPreview) {
            msg = UiSettings::Tr(QStringLiteral("你收到新的群邀请"),
                                 QStringLiteral("You received a new group invite"));
        } else if (from.isEmpty()) {
            msg = UiSettings::Tr(QStringLiteral("群 ID：%1").arg(gid),
                                 QStringLiteral("Group ID: %1").arg(gid));
        } else {
            msg = UiSettings::Tr(QStringLiteral("来自：%1\n群 ID：%2").arg(from, gid),
                                 QStringLiteral("From: %1\nGroup ID: %2").arg(from, gid));
        }
        showTrayMessage(UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Group invite")),
                        msg);
    }

    const bool mainActive = isVisible() && !isMinimized() && isActiveWindow();
    if (!mainActive) {
        return;
    }

    Toast::Show(this,
                from.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("收到群邀请：%1").arg(gid),
                                     QStringLiteral("Group invite: %1").arg(gid))
                    : UiSettings::Tr(QStringLiteral("收到群邀请：%1（来自 %2）").arg(gid, from),
                                     QStringLiteral("Group invite: %1 (from %2)").arg(gid, from)),
                Toast::Level::Info);
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
        item->setData(pinnedIds_.contains(groupId), PinnedRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
        model_->appendRow(item);
        rowIndex = model_->rowCount() - 1;
    } else {
        auto *item = model_->item(rowIndex);
        item->setData(preview, PreviewRole);
        item->setData(QTime::currentTime().toString("HH:mm"), TimeRole);
        int unread = item->data(UnreadRole).toInt();
        item->setData(unread + 1, UnreadRole);
        item->setData(true, IsGroupRole);
        item->setData(QDateTime::currentMSecsSinceEpoch(), LastActiveRole);
    }

    const QDateTime now = QDateTime::currentDateTime();
    bool hasActiveView = false;
    if (embeddedChat_ && embeddedConvId_ == groupId) {
        embeddedChat_->appendSystemMessage(preview, now);
        hasActiveView = true;
    }
    if (chatWindows_.contains(groupId) && chatWindows_[groupId]) {
        chatWindows_[groupId]->appendSystemMessage(preview, now);
        hasActiveView = true;
    }
    if (hasActiveView) {
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
