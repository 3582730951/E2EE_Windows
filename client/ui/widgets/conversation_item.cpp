#include "conversation_item.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ConversationItem::ConversationItem(const QString& title, const QString& summary,
                                   const QString& time, int unread,
                                   const UiPalette& palette, QWidget* parent)
    : QWidget(parent), palette_(palette) {
    setObjectName(QStringLiteral("Panel"));
    setMinimumHeight(72);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    auto* avatar = new QLabel(this);
    avatar->setPixmap(BuildAvatar(title, palette.accent, 42));
    avatar->setFixedSize(42, 42);
    avatar->setScaledContents(true);
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout();
    textColumn->setSpacing(6);
    textColumn->setContentsMargins(0, 0, 0, 0);

    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setStyleSheet(QStringLiteral("font-size:14px; font-weight:600; color:%1;")
                                   .arg(palette_.textPrimary.name()));
    headerRow->addWidget(titleLabel_, 1);

    timeLabel_ = new QLabel(time, this);
    timeLabel_->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                  .arg(palette_.textSecondary.name()));
    headerRow->addWidget(timeLabel_, 0, Qt::AlignRight);
    textColumn->addLayout(headerRow);

    auto* summaryRow = new QHBoxLayout();
    summaryRow->setContentsMargins(0, 0, 0, 0);
    summaryRow->setSpacing(6);

    summaryLabel_ = new QLabel(summary, this);
    summaryLabel_->setStyleSheet(QStringLiteral("color:%1;").arg(palette_.textSecondary.name()));
    summaryLabel_->setWordWrap(true);
    summaryRow->addWidget(summaryLabel_, 1);

    unreadLabel_ = new QLabel(this);
    unreadLabel_->setFixedHeight(20);
    unreadLabel_->setAlignment(Qt::AlignCenter);
    unreadLabel_->setStyleSheet(QStringLiteral(
        "background:%1; color:white; min-width:22px; border-radius:10px; padding:0 6px;")
                                    .arg(palette_.danger.name()));
    summaryRow->addWidget(unreadLabel_, 0, Qt::AlignRight);

    textColumn->addLayout(summaryRow);
    layout->addLayout(textColumn, 1);

    setUnreadCount(unread);
}

QString ConversationItem::title() const {
    return titleLabel_ ? titleLabel_->text() : QString();
}

void ConversationItem::setUnreadCount(int unread) {
    if (!unreadLabel_) {
        return;
    }
    if (unread <= 0) {
        unreadLabel_->hide();
    } else {
        unreadLabel_->setText(QString::number(unread));
        unreadLabel_->show();
    }
}

}  // namespace mi::client::ui::widgets
