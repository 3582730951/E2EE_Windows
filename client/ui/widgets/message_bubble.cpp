#include "message_bubble.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

MessageBubble::MessageBubble(const ChatMessage& message, const UiPalette& palette,
                             QWidget* parent)
    : QWidget(parent), palette_(palette) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(8);
    root->setDirection(message.fromSelf ? QBoxLayout::RightToLeft : QBoxLayout::LeftToRight);
    root->setAlignment(message.fromSelf ? Qt::AlignRight : Qt::AlignLeft);

    auto* avatar = new QLabel(this);
    avatar->setPixmap(BuildAvatar(message.sender.isEmpty() ? QStringLiteral("Me") : message.sender,
                                  message.fromSelf ? palette_.accent : palette_.panelMuted, 36));
    avatar->setFixedSize(36, 36);
    avatar->setScaledContents(true);
    root->addWidget(avatar, 0, Qt::AlignTop);

    auto* bubble = new QFrame(this);
    bubble->setObjectName(QStringLiteral("Bubble"));
    bubble->setStyleSheet(QStringLiteral(
        "QFrame#Bubble { background:%1; border-radius:12px; border:none; }")
                              .arg(message.fromSelf ? palette_.bubbleSelf.name()
                                                    : palette_.bubblePeer.name()));

    auto* bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(6);

    auto* textLabel = new QLabel(renderRichText(message.text, palette_), bubble);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet(QStringLiteral("color:%1; font-size:13px;")
                                 .arg(palette_.textPrimary.name()));
    bubbleLayout->addWidget(textLabel);

    auto* placeholder = new QLabel(tr("图片 / 表情 预留占位"), bubble);
    placeholder->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                   .arg(palette_.textSecondary.name()));
    bubbleLayout->addWidget(placeholder);

    auto* timeLabel = new QLabel(message.time, bubble);
    timeLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                 .arg(palette_.textSecondary.name()));
    bubbleLayout->addWidget(timeLabel, 0, Qt::AlignRight);

    root->addWidget(bubble, 0, message.fromSelf ? Qt::AlignRight : Qt::AlignLeft);
}

QString MessageBubble::renderRichText(const QString& raw, const UiPalette& palette) const {
    QString html = raw.toHtmlEscaped();
    html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    QRegularExpression mention(QStringLiteral(R"(@[^\s]+)"));
    QString highlighted;
    int last = 0;
    auto it = mention.globalMatch(html);
    while (it.hasNext()) {
        auto m = it.next();
        highlighted.append(html.mid(last, m.capturedStart() - last));
        highlighted.append(QStringLiteral("<span style='color:%1;font-weight:600'>%2</span>")
                               .arg(palette.accent.name(), m.captured()));
        last = m.capturedEnd();
    }
    highlighted.append(html.mid(last));
    return highlighted;
}

}  // namespace mi::client::ui::widgets
