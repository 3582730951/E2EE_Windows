#include "MessageDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QFileInfo>
#include <QTextLayout>
#include <QLinearGradient>
#include <QVector>

#include <cmath>

#include "../common/Theme.h"
#include "../common/UiIcons.h"
#include "../common/UiSettings.h"
#include "../common/EmojiPackManager.h"
#include "MessageModel.h"

namespace {
struct BubbleTokens {
    static QColor bgOutgoing() { return Theme::uiMessageOutgoingBg(); }
    static QColor bgIncoming() { return Theme::uiMessageIncomingBg(); }
    static QColor textIncoming() { return Theme::uiMessageText(); }
    static QColor textOutgoing() { return QColor(Qt::white); }
    static QColor timeText() { return Theme::uiMessageTimeText(); }
    static QColor timeTextOutgoing() {
        QColor c(Qt::white);
        c.setAlpha(190);
        return c;
    }
    static QColor systemText() { return Theme::uiMessageSystemText(); }
    static QColor systemBg() { return QColor(0, 0, 0, 0); }
    static int radius() { return 12; }
    static int paddingH() { return 12; }
    static int paddingV() { return 8; }
    static int avatarSize() { return 36; }
    static int margin() { return 10; }
    static int lineSpacing() { return 6; }
};

constexpr qreal kBubbleMaxRatio = 0.7;

QString FormatFileSize(qint64 bytes) {
    if (bytes <= 0) {
        return QStringLiteral("0 B");
    }
    static const char *units[] = {"B", "KB", "MB", "GB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= 1024.0 && unit < 3) {
        v /= 1024.0;
        ++unit;
    }
    const int prec = (unit == 0) ? 0 : (v < 10.0 ? 1 : 0);
    return QStringLiteral("%1 %2").arg(v, 0, 'f', prec).arg(QString::fromLatin1(units[unit]));
}

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

enum class FileKind { Generic = 0, Image = 1, Audio = 2, Video = 3 };

bool HasWhitespace(const QString &text);

FileKind DetectFileKind(const QString &nameOrPath) {
    if (LooksLikeImageFile(nameOrPath)) {
        return FileKind::Image;
    }
    if (LooksLikeAudioFile(nameOrPath)) {
        return FileKind::Audio;
    }
    if (LooksLikeVideoFile(nameOrPath)) {
        return FileKind::Video;
    }
    return FileKind::Generic;
}

QString FileKindLabel(FileKind kind) {
    switch (kind) {
        case FileKind::Image:
            return UiSettings::Tr(QStringLiteral("图片"), QStringLiteral("Image"));
        case FileKind::Audio:
            return UiSettings::Tr(QStringLiteral("语音"), QStringLiteral("Audio"));
        case FileKind::Video:
            return UiSettings::Tr(QStringLiteral("视频"), QStringLiteral("Video"));
        case FileKind::Generic:
        default:
            return UiSettings::Tr(QStringLiteral("文件"), QStringLiteral("File"));
    }
}

QString FileKindIconPath(FileKind kind) {
    switch (kind) {
        case FileKind::Image:
            return QStringLiteral(":/mi/e2ee/ui/icons/image.svg");
        case FileKind::Audio:
            return QStringLiteral(":/mi/e2ee/ui/icons/mic.svg");
        case FileKind::Video:
            return QStringLiteral(":/mi/e2ee/ui/icons/video.svg");
        case FileKind::Generic:
        default:
            return QStringLiteral(":/mi/e2ee/ui/icons/file.svg");
    }
}

QColor FileKindColor(FileKind kind) {
    switch (kind) {
        case FileKind::Image:
            return Theme::accentGreen();
        case FileKind::Audio:
            return Theme::accentOrange();
        case FileKind::Video:
            return Theme::uiAccentBlue();
        case FileKind::Generic:
        default:
            return Theme::uiBadgeGrey();
    }
}

QString StatusText(MessageItem::Status status) {
    switch (status) {
        case MessageItem::Status::Read:
            return UiSettings::Tr(QStringLiteral("已读"), QStringLiteral("Read"));
        case MessageItem::Status::Delivered:
            return UiSettings::Tr(QStringLiteral("已送达"), QStringLiteral("Delivered"));
        case MessageItem::Status::Pending:
            return UiSettings::Tr(QStringLiteral("待发送"), QStringLiteral("Queued"));
        case MessageItem::Status::Failed:
            return UiSettings::Tr(QStringLiteral("发送失败"), QStringLiteral("Failed"));
        case MessageItem::Status::Sent:
        default:
            return UiSettings::Tr(QStringLiteral("已发送"), QStringLiteral("Sent"));
    }
}

QString StickerLabel(const QString &stickerId) {
    const auto *item = EmojiPackManager::Instance().Find(stickerId);
    if (item && !item->title.trimmed().isEmpty()) {
        return item->title;
    }
    const QString id = stickerId.trimmed().toLower();
    if (id == QStringLiteral("s1")) {
        return UiSettings::Tr(QStringLiteral("赞"), QStringLiteral("Like"));
    }
    if (id == QStringLiteral("s2")) {
        return UiSettings::Tr(QStringLiteral("耶"), QStringLiteral("Yay"));
    }
    if (id == QStringLiteral("s3")) {
        return UiSettings::Tr(QStringLiteral("哈哈"), QStringLiteral("Haha"));
    }
    if (id == QStringLiteral("s4")) {
        return UiSettings::Tr(QStringLiteral("爱心"), QStringLiteral("Love"));
    }
    if (id == QStringLiteral("s5")) {
        return UiSettings::Tr(QStringLiteral("哭"), QStringLiteral("Cry"));
    }
    if (id == QStringLiteral("s6")) {
        return UiSettings::Tr(QStringLiteral("生气"), QStringLiteral("Angry"));
    }
    if (id == QStringLiteral("s7")) {
        return UiSettings::Tr(QStringLiteral("疑问"), QStringLiteral("?"));
    }
    if (id == QStringLiteral("s8")) {
        return QStringLiteral("OK");
    }
    return stickerId.trimmed().isEmpty()
               ? UiSettings::Tr(QStringLiteral("贴纸"), QStringLiteral("Sticker"))
               : stickerId;
}

QPixmap FallbackStickerPixmap(const QString &stickerId, int size) {
    static QHash<QString, QPixmap> cache;
    const QString key = stickerId + QStringLiteral(":") + QString::number(size);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const uint h = qHash(stickerId.trimmed().toLower());
    const int hue = static_cast<int>(h % 360);
    QColor c1 = QColor::fromHsv(hue, 160, 230);
    QColor c2 = c1.darker(140);
    QLinearGradient g(0, 0, size, size);
    g.setColorAt(0.0, c1);
    g.setColorAt(1.0, c2);

    QRectF bg(0, 0, size, size);
    p.setBrush(g);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg.adjusted(1, 1, -1, -1), 18, 18);

    QPen border(QColor(255, 255, 255, 26));
    border.setWidthF(1.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(border);
    p.drawRoundedRect(bg.adjusted(1, 1, -1, -1), 18, 18);

    QFont f = QApplication::font();
    f.setBold(true);
    f.setPointSize(qMax(10, size / 7));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, StickerLabel(stickerId));

    p.end();
    cache.insert(key, pm);
    return pm;
}

QPixmap StickerPixmap(const QString &stickerId, int size, const QWidget *widget) {
    QWidget *viewport = nullptr;
    if (widget) {
        if (const auto *view = qobject_cast<const QAbstractItemView *>(widget)) {
            viewport = view->viewport();
        } else {
            viewport = const_cast<QWidget *>(widget);
        }
    }
    if (QMovie *movie =
            EmojiPackManager::Instance().StickerMovie(stickerId, size, viewport)) {
        const QPixmap frame = movie->currentPixmap();
        if (!frame.isNull()) {
            return frame;
        }
    }
    const QPixmap packPm = EmojiPackManager::Instance().StickerPixmap(stickerId, size);
    if (!packPm.isNull()) {
        return packPm;
    }
    return FallbackStickerPixmap(stickerId, size);
}

QSize layoutText(const QString &text, const QFont &font, int maxWidth) {
    const int safeWidth = qMax(1, maxWidth);
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(HasWhitespace(text) ? QTextOption::WrapAtWordBoundaryOrAnywhere
                                           : QTextOption::WrapAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    const QFontMetrics fm(font);
    qreal y = 0.0;
    int width = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(safeWidth);
        const qreal leading = qMax<qreal>(0.0, fm.lineSpacing() - fm.height());
        const qreal step = qMax<qreal>(1.0, line.height() - leading * 0.5);
        line.setPosition(QPointF(0.0, y));
        y += step;
        const int lineWidth =
            qMin(safeWidth, static_cast<int>(std::ceil(line.naturalTextWidth())));
        width = qMax(width, lineWidth);
    }
    layout.endLayout();
    width = qMin(width, safeWidth);
    return QSize(width, static_cast<int>(std::ceil(y)));
}

void DrawWrappedText(QPainter *painter, const QRect &rect, const QString &text, const QFont &font,
                     const QColor &color) {
    const int safeWidth = qMax(1, rect.width());
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(HasWhitespace(text) ? QTextOption::WrapAtWordBoundaryOrAnywhere
                                           : QTextOption::WrapAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    const QFontMetrics fm(font);
    qreal y = 0.0;
    QVector<QTextLine> lines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(safeWidth);
        const qreal leading = qMax<qreal>(0.0, fm.lineSpacing() - fm.height());
        const qreal step = qMax<qreal>(1.0, line.height() - leading * 0.5);
        line.setPosition(QPointF(0.0, y));
        y += step;
        lines.push_back(line);
    }
    layout.endLayout();

    painter->save();
    painter->setPen(color);
    painter->setFont(font);
    const QPointF origin(rect.topLeft());
    for (const auto &line : lines) {
        line.draw(painter, origin + QPointF(0.0, line.y()));
    }
    painter->restore();
}

bool IsEmojiBase(uint32_t cp) {
    if (cp >= 0x1F300 && cp <= 0x1FAFF) {
        return true;
    }
    if (cp >= 0x2600 && cp <= 0x27BF) {
        return true;
    }
    return false;
}

bool IsEmojiComponent(uint32_t cp) {
    if (cp == 0x200D || cp == 0xFE0F || cp == 0x20E3) {
        return true;
    }
    if (cp >= 0x1F3FB && cp <= 0x1F3FF) {
        return true;
    }
    return false;
}

bool IsEmojiOnlyText(const QString &text, int &emojiCount) {
    emojiCount = 0;
    for (int i = 0; i < text.size();) {
        uint32_t cp = text.at(i).unicode();
        if (QChar::isHighSurrogate(text.at(i).unicode()) &&
            i + 1 < text.size() &&
            QChar::isLowSurrogate(text.at(i + 1).unicode())) {
            cp = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
            i += 2;
        } else {
            i += 1;
        }

        if (cp <= 0xFFFF && QChar(static_cast<ushort>(cp)).isSpace()) {
            continue;
        }
        if (IsEmojiBase(cp)) {
            ++emojiCount;
            continue;
        }
        if (IsEmojiComponent(cp)) {
            continue;
        }
        return false;
    }
    return emojiCount > 0 && emojiCount <= 3;
}

bool HasWhitespace(const QString &text) {
    for (const QChar &ch : text) {
        if (ch.isSpace()) {
            return true;
        }
    }
    return false;
}

QPainterPath BubblePath(const QRect &bubbleRect, bool outgoing) {
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(QRectF(bubbleRect), BubbleTokens::radius(), BubbleTokens::radius());

    const qreal tailW = 8.0;
    const qreal tailH = 10.0;
    const qreal tailOffset =
        qMin<qreal>(18.0, static_cast<qreal>(bubbleRect.height()) * 0.45);
    const qreal tailY = bubbleRect.bottom() - tailOffset;

    QPolygonF tail;
    if (outgoing) {
        tail << QPointF(bubbleRect.right(), tailY)
             << QPointF(bubbleRect.right() + tailW, tailY + tailH * 0.5)
             << QPointF(bubbleRect.right(), tailY + tailH);
    } else {
        tail << QPointF(bubbleRect.left(), tailY)
             << QPointF(bubbleRect.left() - tailW, tailY + tailH * 0.5)
             << QPointF(bubbleRect.left(), tailY + tailH);
    }
    path.addPolygon(tail);
    return path;
}

QString StatusGlyphForMeta(bool outgoing,
                           MessageItem::Status status,
                           bool isFile,
                           MessageItem::FileTransfer fileTransfer) {
    if (!outgoing) {
        return {};
    }
    if (isFile && fileTransfer == MessageItem::FileTransfer::Uploading) {
        return UiSettings::Tr(QStringLiteral("上传中…"), QStringLiteral("Uploading…"));
    }
    if (status == MessageItem::Status::Failed || status == MessageItem::Status::Pending) {
        return StatusText(status);
    }
    if (status == MessageItem::Status::Read || status == MessageItem::Status::Delivered) {
        return QStringLiteral("✓✓");
    }
    return QStringLiteral("✓");
}

QString BuildMetaText(const QString &timeText,
                      bool outgoing,
                      MessageItem::Status status,
                      bool isFile,
                      MessageItem::FileTransfer fileTransfer) {
    if (timeText.isEmpty()) {
        return {};
    }
    const QString statusText = StatusGlyphForMeta(outgoing, status, isFile, fileTransfer);
    if (statusText.isEmpty()) {
        return timeText;
    }
    return timeText + QStringLiteral(" · ") + statusText;
}

int MinBubbleWidthForMeta(const QString &metaText, const QFont &font) {
    if (metaText.isEmpty()) {
        return 0;
    }
    QFontMetrics fm(font);
    return fm.horizontalAdvance(metaText) + BubbleTokens::paddingH() * 2 + 6;
}
}  // namespace

MessageDelegate::MessageDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void MessageDelegate::setHighlightedRow(int row) {
    highlightedRow_ = row;
}

QSize MessageDelegate::bubbleSize(const QString &text, const QFont &font, int maxWidth) const {
    QSize t = layoutText(text, font, maxWidth);
    return QSize(t.width() + BubbleTokens::paddingH() * 2,
                 t.height() + BubbleTokens::paddingV() * 2);
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
    int viewWidth = option.rect.width();
    if (const QWidget *widget = option.widget) {
        if (const auto *view = qobject_cast<const QAbstractItemView *>(widget)) {
            viewWidth = view->viewport() ? view->viewport()->width() : viewWidth;
        } else {
            viewWidth = widget->contentsRect().width();
        }
    }
    const int contentWidth = qMax(1, viewWidth - 16);
    const int bubbleMaxWidth =
        qMax(1, static_cast<int>(contentWidth * kBubbleMaxRatio));
    const int textMaxWidth =
        qMax(1, bubbleMaxWidth - BubbleTokens::paddingH() * 2);
    auto type = static_cast<MessageItem::Type>(index.data(MessageModel::TypeRole).toInt());
    if (type == MessageItem::Type::TimeDivider) {
        return QSize(viewWidth, 30);
    }
    if (type == MessageItem::Type::System) {
        QFont f = Theme::defaultFont(11);
        QSize textSize = layoutText(index.data(MessageModel::SystemTextRole).toString(), f,
                                    bubbleMaxWidth);
        return QSize(viewWidth, textSize.height() + 14);
    }
    // Text message
    QFont metaFont = Theme::defaultFont(10);
    const int metaHeight = QFontMetrics(metaFont).height() + 2;
    const bool outgoing = index.data(MessageModel::OutgoingRole).toBool();
    const QString sender = index.data(MessageModel::SenderRole).toString();
    const bool isFile = index.data(MessageModel::IsFileRole).toBool();
    const bool isSticker = index.data(MessageModel::IsStickerRole).toBool();
    const auto status =
        static_cast<MessageItem::Status>(index.data(MessageModel::StatusRole).toInt());
    const auto fileTransfer =
        static_cast<MessageItem::FileTransfer>(index.data(MessageModel::FileTransferRole).toInt());
    const QDateTime messageTime = index.data(MessageModel::TimeRole).toDateTime();
    const QString timeText =
        messageTime.isValid() ? messageTime.toString(QStringLiteral("HH:mm")) : QString();
    const QString metaText = BuildMetaText(timeText, outgoing, status, isFile, fileTransfer);
    const int metaMinWidth = MinBubbleWidthForMeta(metaText, metaFont);
    QString text = index.data(MessageModel::TextRole).toString();
    int emojiCount = 0;
    const bool emojiOnly = !isFile && !isSticker && IsEmojiOnlyText(text, emojiCount);
    QFont textFont = Theme::defaultFont(emojiOnly ? 24 : 13);
    if (isSticker) {
        const int stickerSize = 120;
        const int senderExtra = (!outgoing && !sender.isEmpty()) ? 12 : 0;
        const int bubbleH = stickerSize + BubbleTokens::paddingV() * 2 + metaHeight;
        int height = qMax(BubbleTokens::avatarSize(), bubbleH + senderExtra) + BubbleTokens::margin();
        return QSize(viewWidth, height);
    }
    if (isFile) {
        QFont title = Theme::defaultFont(13, QFont::DemiBold);
        QFont sub = Theme::defaultFont(11);
        const int icon = 44;
        const int contentH =
            QFontMetrics(title).height() + 4 + QFontMetrics(sub).height();
        const int cardH = qMax(icon, contentH);
        const int bubbleH = cardH + BubbleTokens::paddingV() * 2 + metaHeight;
        const int bubbleW = qMax(220, qMin(bubbleMaxWidth, 320));
        QSize bsize(qMax(bubbleW, metaMinWidth), bubbleH);
        const int senderExtra = (!outgoing && !sender.isEmpty()) ? 12 : 0;
        int height = qMax(BubbleTokens::avatarSize(), bsize.height() + senderExtra) +
                     BubbleTokens::margin();
        return QSize(viewWidth, height);
    }
    QSize bsize = bubbleSize(text, textFont, textMaxWidth);
    if (metaMinWidth > 0) {
        bsize.setWidth(qMax(bsize.width(), metaMinWidth));
    }
    bsize.setHeight(bsize.height() + metaHeight);
    const int senderExtra = (!outgoing && !sender.isEmpty()) ? 12 : 0;
    int height = qMax(BubbleTokens::avatarSize(), bsize.height() + senderExtra) + BubbleTokens::margin();
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
        QFont f = Theme::defaultFont(10);
        painter->setFont(f);
        const QString text = index.data(MessageModel::TextRole).toString();
        QFontMetrics fm(f);
        const int padX = 12;
        const int padY = 4;
        const int pillW = fm.horizontalAdvance(text) + padX * 2;
        const int pillH = fm.height() + padY * 2;
        QRect pillRect(0, 0, pillW, pillH);
        pillRect.moveCenter(r.center());
        QColor pillBg = Theme::uiSelectedBg();
        pillBg.setAlpha(170);
        painter->setPen(Qt::NoPen);
        painter->setBrush(pillBg);
        painter->drawRoundedRect(pillRect, pillH / 2.0, pillH / 2.0);
        painter->setPen(BubbleTokens::timeText());
        painter->drawText(pillRect, Qt::AlignCenter, text);
        painter->restore();
        return;
    }
    if (type == MessageItem::Type::System) {
        QFont f = Theme::defaultFont(11);
        painter->setFont(f);
        painter->setPen(BubbleTokens::systemText());
        const QString msg = index.data(MessageModel::SystemTextRole).toString();
        painter->drawText(r, Qt::AlignCenter, msg);
        if (highlightedRow_ >= 0 && index.row() == highlightedRow_) {
            const int pad = qMax(18, static_cast<int>(viewWidth * 0.15));
            QRect highlightRect = r.adjusted(pad, 2, -pad, -2);
            QPen pen(Theme::uiAccentBlue());
            pen.setWidthF(2.0);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(highlightRect, 10, 10);
        }
        painter->restore();
        return;
    }

    // Text bubble
    const bool outgoing = index.data(MessageModel::OutgoingRole).toBool();
    const bool isFile = index.data(MessageModel::IsFileRole).toBool();
    const bool isSticker = index.data(MessageModel::IsStickerRole).toBool();
    const auto status = static_cast<MessageItem::Status>(index.data(MessageModel::StatusRole).toInt());
    const QString sender = index.data(MessageModel::SenderRole).toString();
    QString text = index.data(MessageModel::TextRole).toString();
    const QString filePath = index.data(MessageModel::FilePathRole).toString();
    const qint64 fileSize = isFile ? index.data(MessageModel::FileSizeRole).toLongLong() : 0;
    const qint64 insertedAtMs = index.data(MessageModel::InsertedAtRole).toLongLong();
    const auto fileTransfer =
        static_cast<MessageItem::FileTransfer>(index.data(MessageModel::FileTransferRole).toInt());
    const int fileProgress = index.data(MessageModel::FileProgressRole).toInt();
    const QString stickerId = index.data(MessageModel::StickerIdRole).toString();
    const QDateTime messageTime = index.data(MessageModel::TimeRole).toDateTime();
    const QString timeText = messageTime.isValid() ? messageTime.toString(QStringLiteral("HH:mm")) : QString();
    const QString metaText = BuildMetaText(timeText, outgoing, status, isFile, fileTransfer);
    FileKind fileKind = FileKind::Generic;
    if (isFile) {
        const QString nameOrPath = filePath.isEmpty() ? text : filePath;
        fileKind = DetectFileKind(nameOrPath);
    }
    QColor avatarColor = index.data(MessageModel::AvatarRole).value<QColor>();

    int emojiCount = 0;
    const bool emojiOnly = !isFile && !isSticker && IsEmojiOnlyText(text, emojiCount);
    QFont textFont = Theme::defaultFont(emojiOnly ? 24 : 13);
    QFont metaFont = Theme::defaultFont(10);
    const int metaHeight = QFontMetrics(metaFont).height();
    const int metaReserve = metaHeight + 2;
    const int bubbleMaxWidth = qMax(1, static_cast<int>(viewWidth * kBubbleMaxRatio));
    const int textMaxWidth =
        qMax(1, bubbleMaxWidth - BubbleTokens::paddingH() * 2);
    const int metaMinWidth = MinBubbleWidthForMeta(metaText, metaFont);
    QSize bsize;
    if (isSticker) {
        bsize = QSize(120 + BubbleTokens::paddingH() * 2,
                      120 + BubbleTokens::paddingV() * 2 + metaReserve);
    } else if (isFile) {
        QFont titleFont = Theme::defaultFont(13, QFont::DemiBold);
        QFont subFont = Theme::defaultFont(11);
        const int icon = 44;
        const int contentH =
            QFontMetrics(titleFont).height() + 4 + QFontMetrics(subFont).height();
        const int cardH = qMax(icon, contentH);
        const int bubbleH = cardH + BubbleTokens::paddingV() * 2 + metaReserve;
        const int bubbleW = qMax(220, qMin(bubbleMaxWidth, 320));
        bsize = QSize(bubbleW, bubbleH);
    } else {
        bsize = bubbleSize(text, textFont, textMaxWidth);
        bsize.setHeight(bsize.height() + metaReserve);
    }
    if (metaMinWidth > 0) {
        bsize.setWidth(qMax(bsize.width(), metaMinWidth));
    }

    const int avatarSize = BubbleTokens::avatarSize();
    const int margin = BubbleTokens::margin();

    QRect avatarRect;
    QRect bubbleRect;
    const int senderExtra = (!outgoing && !sender.isEmpty()) ? 12 : 0;
    if (outgoing) {
        avatarRect = QRect(r.right() - avatarSize, r.top() + margin / 2, avatarSize, avatarSize);
        bubbleRect =
            QRect(avatarRect.left() - margin - bsize.width(), avatarRect.top(), bsize.width(),
                  bsize.height());
    } else {
        avatarRect = QRect(r.left(), r.top() + margin / 2, avatarSize, avatarSize);
        bubbleRect =
            QRect(avatarRect.right() + margin, avatarRect.top() + senderExtra, bsize.width(), bsize.height());

        if (!sender.isEmpty()) {
            QFont sf = Theme::defaultFont(9);
            painter->setFont(sf);
            painter->setPen(BubbleTokens::timeText());
            QRect senderRect(bubbleRect.left(), avatarRect.top(), bubbleRect.width(), senderExtra);
            const QString name =
                painter->fontMetrics().elidedText(sender, Qt::ElideRight, senderRect.width());
            painter->drawText(senderRect.adjusted(0, 0, 0, -2), Qt::AlignLeft | Qt::AlignVCenter, name);
        }
    }

    if (insertedAtMs > 0) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 dt = nowMs - insertedAtMs;
        const qint64 windowMs = 220;
        if (dt >= 0 && dt < windowMs) {
            const double t = 1.0 - (static_cast<double>(dt) / static_cast<double>(windowMs));
            QColor glow = Theme::uiAccentBlue();
            glow.setAlpha(qBound(0, static_cast<int>(70.0 * t), 70));
            QRect glowRect = bubbleRect.adjusted(-5, -3, 5, 3);
            painter->setPen(Qt::NoPen);
            painter->setBrush(glow);
            painter->drawRoundedRect(glowRect, BubbleTokens::radius() + 6, BubbleTokens::radius() + 6);
        }
    }

    // Bubble background
    const QPainterPath bubblePath = BubblePath(bubbleRect, outgoing);
    painter->setBrush(outgoing ? BubbleTokens::bgOutgoing() : BubbleTokens::bgIncoming());
    painter->setPen(Qt::NoPen);
    painter->drawPath(bubblePath);

    const QColor textColor = outgoing ? BubbleTokens::textOutgoing() : BubbleTokens::textIncoming();
    QColor metaColor = outgoing ? BubbleTokens::timeTextOutgoing() : BubbleTokens::timeText();
    if (outgoing && status == MessageItem::Status::Failed) {
        metaColor = Theme::uiDangerRed();
    }

    painter->save();
    painter->setClipPath(bubblePath);
    const bool hasMeta = !metaText.isEmpty();
    const int metaAvail = bubbleRect.width() - BubbleTokens::paddingH() * 2;
    const QString metaDraw =
        hasMeta ? QFontMetrics(metaFont).elidedText(metaText, Qt::ElideLeft, metaAvail) : QString();
    const QRect metaRect(bubbleRect.left() + BubbleTokens::paddingH(),
                         bubbleRect.bottom() - BubbleTokens::paddingV() - metaHeight,
                         metaAvail,
                         metaHeight);
    if (hasMeta) {
        painter->setFont(metaFont);
        painter->setPen(metaColor);
        painter->drawText(metaRect, Qt::AlignRight | Qt::AlignVCenter, metaDraw);
    }
    if (isSticker) {
        const int stickerSize = 120;
        const QRect stickerRect = QRect(bubbleRect.left() + BubbleTokens::paddingH(),
                                        bubbleRect.top() + BubbleTokens::paddingV(),
                                        stickerSize,
                                        stickerSize);
        painter->drawPixmap(stickerRect, StickerPixmap(stickerId, stickerSize, option.widget));
    } else if (isFile) {
        const QRect contentRect = bubbleRect.adjusted(BubbleTokens::paddingH(), BubbleTokens::paddingV(),
                                                      -BubbleTokens::paddingH(),
                                                      -BubbleTokens::paddingV() - metaReserve);
        const int iconSize = 44;
        const int gap = 12;
        const QRect iconRect = QRect(contentRect.left(),
                                     contentRect.top() + (contentRect.height() - iconSize) / 2,
                                     iconSize,
                                     iconSize);
        const QRect textArea = contentRect.adjusted(iconSize + gap, 0, 0, 0);

        QColor base = FileKindColor(fileKind);
        QLinearGradient g(iconRect.topLeft(), iconRect.bottomRight());
        g.setColorAt(0.0, base.lighter(118));
        g.setColorAt(1.0, base.darker(118));
        painter->setPen(Qt::NoPen);
        painter->setBrush(g);
        painter->drawRoundedRect(iconRect, 10, 10);

        QPen iconBorder(QColor(255, 255, 255, 24));
        iconBorder.setWidthF(1.0);
        painter->setPen(iconBorder);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(iconRect.adjusted(0, 0, -1, -1), 10, 10);

        const QString fileIcon = FileKindIconPath(fileKind);
        const int svgSide = 20;
        const qreal iconDpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const QPixmap pm = UiIcons::TintedSvg(fileIcon, svgSide, Qt::white, iconDpr);
        const QRect svgRect(iconRect.center().x() - svgSide / 2, iconRect.center().y() - svgSide / 2,
                            svgSide, svgSide);
        painter->drawPixmap(svgRect, pm);

        QString fileName = text.trimmed();
        if (fileName.contains(QChar('/')) || fileName.contains(QChar('\\'))) {
            fileName = QFileInfo(fileName).fileName();
        }
        if (fileName.isEmpty() && !filePath.trimmed().isEmpty()) {
            fileName = QFileInfo(filePath).fileName();
        }
        if (fileName.isEmpty()) {
            fileName = UiSettings::Tr(QStringLiteral("未命名文件"), QStringLiteral("Unnamed file"));
        }

        QFont titleFont = Theme::defaultFont(13, QFont::DemiBold);
        QFont subFont = Theme::defaultFont(11);

        painter->setFont(titleFont);
        painter->setPen(textColor);
        QFontMetrics titleFm(titleFont);
        const QString titleText = titleFm.elidedText(fileName, Qt::ElideMiddle, textArea.width());
        QRect titleRect(textArea.left(), textArea.top(), textArea.width(), titleFm.height());
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, titleText);

        const QString sizeText =
            fileSize > 0
                ? FormatFileSize(fileSize)
                : UiSettings::Tr(QStringLiteral("未知大小"), QStringLiteral("Unknown size"));

        QString transferTag;
        if (fileTransfer == MessageItem::FileTransfer::Uploading) {
            transferTag = UiSettings::Tr(QStringLiteral("上传中…"), QStringLiteral("Uploading…"));
        } else if (fileTransfer == MessageItem::FileTransfer::Downloading) {
            transferTag = UiSettings::Tr(QStringLiteral("保存中…"), QStringLiteral("Saving…"));
        } else if (!outgoing && !filePath.trimmed().isEmpty()) {
            transferTag = UiSettings::Tr(QStringLiteral("已保存"), QStringLiteral("Saved"));
        } else if (outgoing && status == MessageItem::Status::Failed) {
            transferTag = UiSettings::Tr(QStringLiteral("发送失败"), QStringLiteral("Failed"));
        }

        QString meta = QStringLiteral("%1 · %2").arg(FileKindLabel(fileKind), sizeText);
        if (!transferTag.isEmpty()) {
            meta += QStringLiteral(" · ") + transferTag;
        }
        painter->setFont(subFont);
        painter->setPen(metaColor);
        QFontMetrics subFm(subFont);
        const QString metaText = subFm.elidedText(meta, Qt::ElideRight, textArea.width());
        QRect metaRect(textArea.left(), titleRect.bottom() + 4, textArea.width(), subFm.height());
        painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter, metaText);

        if (fileTransfer != MessageItem::FileTransfer::None) {
            const int barH = 3;
            QRect barRect(contentRect.left(),
                          contentRect.bottom() - barH,
                          contentRect.width(),
                          barH);
            barRect = barRect.adjusted(0, 0, 0, -1);
            QColor track = metaColor;
            track.setAlpha(outgoing ? 70 : 60);
            painter->setPen(Qt::NoPen);
            painter->setBrush(track);
            painter->drawRoundedRect(barRect, barH / 2.0, barH / 2.0);

            QColor accent = outgoing ? QColor(255, 255, 255, 210)
                                     : Theme::uiAccentBlue();
            if (!outgoing) {
                accent.setAlpha(200);
            }
            if (fileProgress >= 0) {
                const int w = qMax(2, static_cast<int>(barRect.width() * (fileProgress / 100.0)));
                QRect fill = barRect;
                fill.setWidth(w);
                painter->setBrush(accent);
                painter->drawRoundedRect(fill, barH / 2.0, barH / 2.0);
            } else {
                const qint64 ms = QDateTime::currentMSecsSinceEpoch();
                const int periodMs = 1200;
                const double t = (ms % periodMs) / static_cast<double>(periodMs);
                const int shineW = qMax(10, barRect.width() / 3);
                const int x = barRect.left() + static_cast<int>((barRect.width() + shineW) * t) - shineW;
                QRect shineRect(x, barRect.top(), shineW, barRect.height());
                shineRect = shineRect.intersected(barRect);

                QLinearGradient grad(shineRect.topLeft(), shineRect.topRight());
                QColor c0 = accent;
                c0.setAlpha(30);
                QColor c1 = accent;
                c1.setAlpha(200);
                QColor c2 = accent;
                c2.setAlpha(30);
                grad.setColorAt(0.0, c0);
                grad.setColorAt(0.5, c1);
                grad.setColorAt(1.0, c2);
                painter->setBrush(grad);
                painter->drawRoundedRect(shineRect, barH / 2.0, barH / 2.0);
            }
        }
    } else {
        painter->setPen(textColor);
        painter->setFont(textFont);
        QRect textRect = bubbleRect.adjusted(BubbleTokens::paddingH(), BubbleTokens::paddingV(),
                                             -BubbleTokens::paddingH(),
                                             -BubbleTokens::paddingV() - metaReserve);
        painter->save();
        painter->setClipRect(textRect, Qt::IntersectClip);
        if (emojiOnly) {
            painter->drawText(textRect, Qt::AlignCenter, text);
        } else {
            DrawWrappedText(painter, textRect, text, textFont, textColor);
        }
        painter->restore();
    }
    painter->restore();

    // Avatar
    painter->setBrush(avatarColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(avatarRect);

    if (highlightedRow_ >= 0 && index.row() == highlightedRow_) {
        QPen pen(Theme::uiAccentBlue());
        pen.setWidthF(2.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(bubbleRect.adjusted(-2, -2, 2, 2),
                                 BubbleTokens::radius() + 2,
                                 BubbleTokens::radius() + 2);
    }

    painter->restore();
}


