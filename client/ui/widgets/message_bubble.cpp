#include "message_bubble.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

MessageBubble::MessageBubble(const ChatMessage& message, const UiPalette& palette,
                             QWidget* parent)
    : QWidget(parent), palette_(palette) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto* avatar = new QLabel(this);
    const QString avatarText = message.sender.isEmpty() ? QStringLiteral("S") : message.sender;
    avatar->setPixmap(BuildAvatar(avatarText, palette_.accent, 32));
    avatar->setFixedSize(32, 32);
    avatar->setScaledContents(true);
    root->addWidget(avatar, 0, Qt::AlignTop);

    auto* column = new QVBoxLayout();
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    auto* textLabel =
        new QLabel(message.text.isEmpty() ? tr("示例消息") : message.text, this);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:13px;").arg(palette_.textPrimary.name()));
    column->addWidget(textLabel);

    auto* timeLabel = new QLabel(message.time, this);
    timeLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;").arg(palette_.textSecondary.name()));
    column->addWidget(timeLabel, 0, Qt::AlignLeft);

    root->addLayout(column, 1);
}

}  // namespace mi::client::ui::widgets
