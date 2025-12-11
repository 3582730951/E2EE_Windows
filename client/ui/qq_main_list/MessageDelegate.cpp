#include "MessageDelegate.h"

#include <QApplication>
#include <QDateTime>
#include <QPainter>
#include <QTextLayout>

#include "MessageModel.h"

namespace {
struct BubbleTokens {
    static QColor bgOutgoing() { return QColor("#3A3D40"); }
    static QColor bgIncoming() { return QColor("#2F3235"); }
    static QColor text() { return QColor("#E6E6E6"); }
    static QColor timeText() { return QColor("#6E737A"); }
    static QColor systemText() { return QColor("#9A9FA6"); }
    static QColor systemBg() { return QColor(0, 0, 0, 0); }
    static int radius() { return 10; }
    static int paddingH() { return 14; }
    static int paddingV() { return 10; }
    static int avatarSize() { return 38; }
    static int margin() { return 12; }
    static int lineSpacing() { return 8; }
};

QSize layoutText(const QString &text, const QFont &font, int maxWidth) {
    QTextLayout layout(text, font);
    layout.beginLayout();
    int height = 0;
    int width = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(maxWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        width = qMax(width, static_cast<int>(line.naturalTextWidth()));
    }
    layout.endLayout();
    return QSize(width, height);
}
}  // namespace

MessageDelegate::MessageDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize MessageDelegate::bubbleSize(const QString &text, const QFont &font, int maxWidth) const {
    QSize t = layoutText(text, font, maxWidth);
    return QSize(t.width() + BubbleTokens::paddingH() * 2,
                 t.height() + BubbleTokens::paddingV() * 2);
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    const int viewWidth = option.rect.width();
    auto type = static_cast<MessageItem::Type>(index.data(MessageModel::TypeRole).toInt());
    if (type == MessageItem::Type::TimeDivider) {
        return QSize(viewWidth, 34);
    }
    if (type == MessageItem::Type::System) {
        QFont f = QApplication::font();
        f.setPointSize(12);
        QSize textSize = layoutText(index.data(MessageModel::SystemTextRole).toString(), f,
                                    static_cast<int>(viewWidth * 0.7));
        return QSize(viewWidth, textSize.height() + 16);
    }
    // Text message
    QFont f = QApplication::font();
    f.setPointSize(13);
    int maxBubbleWidth = static_cast<int>(viewWidth * 0.6);
    QSize bsize = bubbleSize(index.data(MessageModel::TextRole).toString(), f, maxBubbleWidth);
    int height = qMax(BubbleTokens::avatarSize(), bsize.height()) + BubbleTokens::margin();
    return QSize(viewWidth, height);
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRect r = option.rect.adjusted(8, 4, -8, -4);
    const int viewWidth = r.width();
    auto type = static_cast<MessageItem::Type>(index.data(MessageModel::TypeRole).toInt());

    if (type == MessageItem::Type::TimeDivider) {
        QFont f = QApplication::font();
        f.setPointSize(11);
        painter->setFont(f);
        painter->setPen(BubbleTokens::timeText());
        painter->drawText(r, Qt::AlignCenter, index.data(MessageModel::TextRole).toString());
        painter->restore();
        return;
    }
    if (type == MessageItem::Type::System) {
        QFont f = QApplication::font();
        f.setPointSize(12);
        painter->setFont(f);
        painter->setPen(BubbleTokens::systemText());
        painter->drawText(r, Qt::AlignCenter, index.data(MessageModel::SystemTextRole).toString());
        painter->restore();
        return;
    }

    // Text bubble
    const bool outgoing = index.data(MessageModel::OutgoingRole).toBool();
    const QString text = index.data(MessageModel::TextRole).toString();
    QColor avatarColor = index.data(MessageModel::AvatarRole).value<QColor>();

    QFont textFont = QApplication::font();
    textFont.setPointSize(13);
    painter->setFont(textFont);
    int maxBubbleWidth = static_cast<int>(viewWidth * 0.6);
    QSize bsize = bubbleSize(text, textFont, maxBubbleWidth);

    const int avatarSize = BubbleTokens::avatarSize();
    const int margin = BubbleTokens::margin();

    QRect avatarRect;
    QRect bubbleRect;
    if (outgoing) {
        avatarRect = QRect(r.right() - avatarSize, r.top() + margin / 2, avatarSize, avatarSize);
        bubbleRect =
            QRect(avatarRect.left() - margin - bsize.width(), avatarRect.top(), bsize.width(),
                  bsize.height());
    } else {
        avatarRect = QRect(r.left(), r.top() + margin / 2, avatarSize, avatarSize);
        bubbleRect =
            QRect(avatarRect.right() + margin, avatarRect.top(), bsize.width(), bsize.height());
    }

    // Bubble background
    painter->setBrush(outgoing ? BubbleTokens::bgOutgoing() : BubbleTokens::bgIncoming());
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(bubbleRect, BubbleTokens::radius(), BubbleTokens::radius());

    // Text
    painter->setPen(BubbleTokens::text());
    QRect textRect = bubbleRect.adjusted(BubbleTokens::paddingH(), BubbleTokens::paddingV(),
                                         -BubbleTokens::paddingH(), -BubbleTokens::paddingV());
    painter->drawText(textRect, Qt::TextWordWrap, text);

    // Avatar
    painter->setBrush(avatarColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(avatarRect);

    painter->restore();
}
