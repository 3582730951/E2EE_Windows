#include "NotificationCenterDialog.h"

#include <algorithm>

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "../common/Theme.h"
#include "../common/UiSettings.h"

namespace {

void ClearLayout(QLayout *layout) {
    if (!layout) {
        return;
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            delete w;
        }
        if (QLayout *childLayout = item->layout()) {
            ClearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %5; }"
            "QPushButton:disabled { color: %6; background: %7; }")
            .arg(Theme::uiTextMain().name(),
                 Theme::uiPanelBg().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name(),
                 Theme::uiSelectedBg().name(),
                 Theme::uiTextMuted().name(),
                 Theme::uiPanelBg().darker(105).name()));
    return btn;
}

QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    const QColor base = Theme::uiAccentBlue();
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: white; background: %1; border: none; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %2; }"
            "QPushButton:pressed { background: %3; }"
            "QPushButton:disabled { background: %4; color: rgba(255,255,255,180); }")
            .arg(base.name(),
                 base.lighter(112).name(),
                 base.darker(110).name(),
                 base.darker(135).name()));
    return btn;
}

QPushButton *dangerOutlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    const QColor danger = Theme::uiDangerRed();
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %1; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %3; }"
            "QPushButton:pressed { background: %4; }"
            "QPushButton:disabled { color: %5; border-color: %5; background: %2; }")
            .arg(danger.name(),
                 Theme::uiPanelBg().name(),
                 danger.lighter(160).name(),
                 danger.lighter(140).name(),
                 Theme::uiTextMuted().name()));
    return btn;
}

QFrame *cardFrame(QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 12px; }")
                            .arg(Theme::uiPanelBg().name(),
                                 Theme::uiBorder().name()));
    return card;
}

QString FormatTime(qint64 ms) {
    if (ms <= 0) {
        return QString();
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("MM-dd HH:mm"));
}

}  // namespace

NotificationCenterDialog::NotificationCenterDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(UiSettings::Tr(QStringLiteral("通知中心"), QStringLiteral("Notifications")));
    setModal(true);
    resize(580, 560);
    setStyleSheet(QStringLiteral("QDialog { background: %1; }").arg(Theme::uiWindowBg().name()));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    auto *title = new QLabel(UiSettings::Tr(QStringLiteral("通知中心"), QStringLiteral("Notifications")), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: 650;")
                             .arg(Theme::uiTextMain().name()));
    header->addWidget(title);
    header->addStretch();

    auto *refreshBtn = outlineButton(UiSettings::Tr(QStringLiteral("刷新"), QStringLiteral("Refresh")), this);
    refreshBtn->setFixedHeight(30);
    connect(refreshBtn, &QAbstractButton::clicked, this, &NotificationCenterDialog::refreshRequested);
    header->addWidget(refreshBtn);
    root->addLayout(header);

    auto *seg = new QFrame(this);
    seg->setFrameShape(QFrame::NoFrame);
    seg->setObjectName(QStringLiteral("seg"));
    seg->setStyleSheet(QStringLiteral(
        "QFrame#seg { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QToolButton { border: none; background: transparent; padding: 6px 14px; color: %3; font-size: 12px; }"
        "QToolButton:checked { background: %4; color: %5; border-radius: 10px; }")
                           .arg(Theme::uiSearchBg().name(),
                                Theme::uiBorder().name(),
                                Theme::uiTextSub().name(),
                                Theme::uiSelectedBg().name(),
                                Theme::uiTextMain().name()));

    auto *segLayout = new QHBoxLayout(seg);
    segLayout->setContentsMargins(6, 6, 6, 6);
    segLayout->setSpacing(6);

    requestsBtn_ = new QToolButton(seg);
    requestsBtn_->setText(UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Requests")));
    requestsBtn_->setCheckable(true);
    invitesBtn_ = new QToolButton(seg);
    invitesBtn_->setText(UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Invites")));
    invitesBtn_->setCheckable(true);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(requestsBtn_, 0);
    group->addButton(invitesBtn_, 1);
    requestsBtn_->setChecked(true);

    segLayout->addWidget(requestsBtn_, 0, Qt::AlignLeft);
    segLayout->addWidget(invitesBtn_, 0, Qt::AlignLeft);
    segLayout->addStretch();
    root->addWidget(seg);

    stack_ = new QStackedWidget(this);
    stack_->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
    root->addWidget(stack_, 1);

    auto makeScroll = [&](QScrollArea *&outScroll, QWidget *&outBody, QVBoxLayout *&outLayout) {
        outScroll = new QScrollArea(stack_);
        outScroll->setFrameShape(QFrame::NoFrame);
        outScroll->setWidgetResizable(true);
        outScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        outScroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; }"
            "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }")
                                     .arg(Theme::uiScrollBarHandle().name(),
                                          Theme::uiScrollBarHandleHover().name()));
        outBody = new QWidget(outScroll);
        outLayout = new QVBoxLayout(outBody);
        outLayout->setContentsMargins(0, 0, 0, 0);
        outLayout->setSpacing(10);
        outScroll->setWidget(outBody);
        stack_->addWidget(outScroll);
    };

    makeScroll(requestsScroll_, requestsBody_, requestsLayout_);
    makeScroll(invitesScroll_, invitesBody_, invitesLayout_);

    connect(group, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        if (stack_) {
            stack_->setCurrentIndex(id);
        }
    });

    rebuildFriendRequests();
    rebuildGroupInvites();
    updateSegmentTitles();
}

void NotificationCenterDialog::setFriendRequests(const QVector<FriendRequest> &requests) {
    friendRequests_ = requests;
    rebuildFriendRequests();
    updateSegmentTitles();
}

void NotificationCenterDialog::setGroupInvites(const QVector<GroupInvite> &invites) {
    groupInvites_ = invites;
    rebuildGroupInvites();
    updateSegmentTitles();
}

void NotificationCenterDialog::removeFriendRequest(const QString &requester) {
    const QString key = requester.trimmed();
    if (key.isEmpty()) {
        return;
    }
    for (int i = friendRequests_.size() - 1; i >= 0; --i) {
        if (friendRequests_[i].requester == key) {
            friendRequests_.removeAt(i);
        }
    }
    rebuildFriendRequests();
    updateSegmentTitles();
}

void NotificationCenterDialog::removeGroupInvite(const QString &groupId, const QString &messageId) {
    const QString gid = groupId.trimmed();
    const QString mid = messageId.trimmed();
    if (gid.isEmpty()) {
        return;
    }
    for (int i = groupInvites_.size() - 1; i >= 0; --i) {
        const bool matchId = (groupInvites_[i].groupId == gid);
        const bool matchMsg = mid.isEmpty() || groupInvites_[i].messageId == mid;
        if (matchId && matchMsg) {
            groupInvites_.removeAt(i);
        }
    }
    rebuildGroupInvites();
    updateSegmentTitles();
}

void NotificationCenterDialog::rebuildFriendRequests() {
    if (!requestsLayout_) {
        return;
    }
    ClearLayout(requestsLayout_);

    if (friendRequests_.isEmpty()) {
        auto *empty = new QLabel(
            UiSettings::Tr(QStringLiteral("暂无好友申请"), QStringLiteral("No friend requests")), requestsBody_);
        empty->setAlignment(Qt::AlignHCenter);
        empty->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                 .arg(Theme::uiTextMuted().name()));
        requestsLayout_->addStretch();
        requestsLayout_->addWidget(empty);
        requestsLayout_->addStretch();
        return;
    }

    auto sorted = friendRequests_;
    std::sort(sorted.begin(), sorted.end(), [](const FriendRequest &a, const FriendRequest &b) {
        return a.receivedMs > b.receivedMs;
    });

    for (const auto &req : sorted) {
        auto *card = cardFrame(requestsBody_);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(8);

        auto *top = new QHBoxLayout();
        top->setSpacing(8);
        auto *name = new QLabel(req.requester, card);
        name->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 650;")
                                .arg(Theme::uiTextMain().name()));
        top->addWidget(name);
        top->addStretch();
        const QString ts = FormatTime(req.receivedMs);
        if (!ts.isEmpty()) {
            auto *time = new QLabel(ts, card);
            time->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                    .arg(Theme::uiTextMuted().name()));
            top->addWidget(time);
        }
        cardLayout->addLayout(top);

        if (!req.remark.trimmed().isEmpty()) {
            auto *remark = new QLabel(
                UiSettings::Tr(QStringLiteral("备注：%1").arg(req.remark.trimmed()),
                               QStringLiteral("Remark: %1").arg(req.remark.trimmed())),
                card);
            remark->setWordWrap(true);
            remark->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                      .arg(Theme::uiTextSub().name()));
            cardLayout->addWidget(remark);
        }

        auto *row = new QHBoxLayout();
        row->setSpacing(10);
        row->addStretch();
        auto *rejectBtn = outlineButton(UiSettings::Tr(QStringLiteral("拒绝"), QStringLiteral("Reject")), card);
        auto *blockBtn = dangerOutlineButton(UiSettings::Tr(QStringLiteral("拉黑"), QStringLiteral("Block")), card);
        auto *acceptBtn = primaryButton(UiSettings::Tr(QStringLiteral("同意"), QStringLiteral("Accept")), card);

        connect(acceptBtn, &QAbstractButton::clicked, this, [this, who = req.requester]() {
            emit friendRequestActionRequested(who, FriendRequestAction::Accept);
        });
        connect(rejectBtn, &QAbstractButton::clicked, this, [this, who = req.requester]() {
            emit friendRequestActionRequested(who, FriendRequestAction::Reject);
        });
        connect(blockBtn, &QAbstractButton::clicked, this, [this, who = req.requester]() {
            emit friendRequestActionRequested(who, FriendRequestAction::Block);
        });

        row->addWidget(rejectBtn);
        row->addWidget(blockBtn);
        row->addWidget(acceptBtn);
        cardLayout->addLayout(row);

        requestsLayout_->addWidget(card);
    }
    requestsLayout_->addStretch();
}

void NotificationCenterDialog::rebuildGroupInvites() {
    if (!invitesLayout_) {
        return;
    }
    ClearLayout(invitesLayout_);

    if (groupInvites_.isEmpty()) {
        auto *empty = new QLabel(
            UiSettings::Tr(QStringLiteral("暂无群邀请"), QStringLiteral("No group invites")), invitesBody_);
        empty->setAlignment(Qt::AlignHCenter);
        empty->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                 .arg(Theme::uiTextMuted().name()));
        invitesLayout_->addStretch();
        invitesLayout_->addWidget(empty);
        invitesLayout_->addStretch();
        return;
    }

    auto sorted = groupInvites_;
    std::sort(sorted.begin(), sorted.end(), [](const GroupInvite &a, const GroupInvite &b) {
        return a.receivedMs > b.receivedMs;
    });

    for (const auto &inv : sorted) {
        auto *card = cardFrame(invitesBody_);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(8);

        auto *top = new QHBoxLayout();
        top->setSpacing(8);
        auto *title = new QLabel(
            UiSettings::Tr(QStringLiteral("群聊 %1").arg(inv.groupId),
                           QStringLiteral("Group %1").arg(inv.groupId)),
            card);
        title->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 650;")
                                 .arg(Theme::uiTextMain().name()));
        top->addWidget(title);
        top->addStretch();
        const QString ts = FormatTime(inv.receivedMs);
        if (!ts.isEmpty()) {
            auto *time = new QLabel(ts, card);
            time->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                    .arg(Theme::uiTextMuted().name()));
            top->addWidget(time);
        }
        cardLayout->addLayout(top);

        auto *from = new QLabel(
            UiSettings::Tr(QStringLiteral("来自：%1").arg(inv.fromUser),
                           QStringLiteral("From: %1").arg(inv.fromUser)),
            card);
        from->setWordWrap(true);
        from->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                .arg(Theme::uiTextSub().name()));
        cardLayout->addWidget(from);

        auto *row = new QHBoxLayout();
        row->setSpacing(10);
        row->addStretch();

        auto *ignoreBtn = dangerOutlineButton(UiSettings::Tr(QStringLiteral("忽略"), QStringLiteral("Ignore")), card);
        auto *copyBtn = outlineButton(UiSettings::Tr(QStringLiteral("复制群 ID"), QStringLiteral("Copy ID")), card);
        auto *joinBtn = primaryButton(UiSettings::Tr(QStringLiteral("加入"), QStringLiteral("Join")), card);

        connect(joinBtn, &QAbstractButton::clicked, this, [this, inv]() {
            emit groupInviteActionRequested(inv.groupId, inv.fromUser, inv.messageId, GroupInviteAction::Join);
        });
        connect(copyBtn, &QAbstractButton::clicked, this, [this, inv]() {
            emit groupInviteActionRequested(inv.groupId, inv.fromUser, inv.messageId, GroupInviteAction::CopyId);
        });
        connect(ignoreBtn, &QAbstractButton::clicked, this, [this, inv]() {
            emit groupInviteActionRequested(inv.groupId, inv.fromUser, inv.messageId, GroupInviteAction::Ignore);
        });

        row->addWidget(ignoreBtn);
        row->addWidget(copyBtn);
        row->addWidget(joinBtn);
        cardLayout->addLayout(row);

        invitesLayout_->addWidget(card);
    }
    invitesLayout_->addStretch();
}

void NotificationCenterDialog::updateSegmentTitles() {
    if (!requestsBtn_ || !invitesBtn_) {
        return;
    }
    const int req = friendRequests_.size();
    const int inv = groupInvites_.size();

    requestsBtn_->setText(
        UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Requests")) +
        (req > 0 ? QStringLiteral(" (%1)").arg(req) : QString()));
    invitesBtn_->setText(
        UiSettings::Tr(QStringLiteral("群邀请"), QStringLiteral("Invites")) +
        (inv > 0 ? QStringLiteral(" (%1)").arg(inv) : QString()));
}
