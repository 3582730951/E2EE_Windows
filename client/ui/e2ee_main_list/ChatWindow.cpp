#include "ChatWindow.h"

#include <functional>

#include <QApplication>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPalette>
#include <QLinearGradient>
#include <QRegularExpression>
#include <QSlider>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QTextDocument>
#include <QTextOption>

#if defined(MI_UI_HAS_QT_MULTIMEDIA)
#include <QAudioOutput>
#include <QBuffer>
#include <QMediaPlayer>
#include <QVideoWidget>
#endif
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QDateTime>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QTimer>

#include <cmath>
#include <memory>

#include "../common/IconButton.h"
#include "../common/ChatInputEdit.h"
#include "../common/Theme.h"
#include "../common/UiIcons.h"
#include "../common/UiSettings.h"
#include "../common/UiStyle.h"
#include "../common/Toast.h"
#include "../common/SecureClipboard.h"
#include "BackendAdapter.h"
#include "ConversationDetailsDialog.h"
#include "EmojiPickerDialog.h"
#include "MessageDelegate.h"
#include "MessageModel.h"

namespace {

struct ChatTokens {
    static QColor windowBg() { return Theme::uiWindowBg(); }
    static QColor panelBg() { return Theme::uiPanelBg(); }
    static QColor hoverBg() { return Theme::uiHoverBg(); }
    static QColor selectedBg() { return Theme::uiSelectedBg(); }
    static QColor border() { return Theme::uiBorder(); }
    static QColor textMain() { return Theme::uiTextMain(); }
    static QColor textSub() { return Theme::uiTextSub(); }
    static QColor textMuted() { return Theme::uiTextMuted(); }
    static QColor accentBlue() { return Theme::uiAccentBlue(); }
    static QColor accentGrey() { return Theme::uiBorder(); }
    static int radius() { return 16; }
};

constexpr int kComposerInputPaddingH = 10;
constexpr int kComposerInputPaddingV = 6;
constexpr int kComposerInputMinHeight = 36;
constexpr int kComposerInputMaxHeight = 220;

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

QString StickerLabel(const QString &stickerId) {
    const QString id = stickerId.trimmed().toLower();
    if (id == QStringLiteral("s1")) {
        return QStringLiteral("赞");
    }
    if (id == QStringLiteral("s2")) {
        return QStringLiteral("耶");
    }
    if (id == QStringLiteral("s3")) {
        return QStringLiteral("哈哈");
    }
    if (id == QStringLiteral("s4")) {
        return QStringLiteral("爱心");
    }
    if (id == QStringLiteral("s5")) {
        return QStringLiteral("哭");
    }
    if (id == QStringLiteral("s6")) {
        return QStringLiteral("生气");
    }
    if (id == QStringLiteral("s7")) {
        return QStringLiteral("疑问");
    }
    if (id == QStringLiteral("s8")) {
        return QStringLiteral("OK");
    }
    return stickerId.trimmed().isEmpty() ? QStringLiteral("贴纸") : stickerId;
}

QPixmap StickerIcon(const QString &stickerId, int size) {
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
    p.drawRoundedRect(bg.adjusted(1, 1, -1, -1), 16, 16);

    QFont f = QApplication::font();
    f.setBold(true);
    f.setPointSize(qMax(10, size / 7));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, StickerLabel(stickerId));
    return pm;
}

QPixmap EmptyChatIcon(int size) {
    const int s = qMax(32, size);
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bubble(s * 0.12, s * 0.14, s * 0.76, s * 0.62);
    const qreal radius = s * 0.18;
    QPen pen(Theme::uiBorder());
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.setBrush(Theme::uiSelectedBg());
    p.drawRoundedRect(bubble, radius, radius);

    QPolygonF tail;
    const qreal tailW = s * 0.18;
    const qreal tailH = s * 0.16;
    const qreal tailX = bubble.left() + s * 0.22;
    const qreal tailY = bubble.bottom() - 1.0;
    tail << QPointF(tailX, tailY) << QPointF(tailX + tailW, tailY)
         << QPointF(tailX + tailW * 0.35, tailY + tailH);
    p.drawPolygon(tail);

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::uiTextMuted());
    const qreal dotR = s * 0.05;
    const qreal cy = bubble.top() + bubble.height() * 0.55;
    const qreal startX = bubble.left() + bubble.width() * 0.35;
    const qreal gap = s * 0.14;
    for (int i = 0; i < 3; ++i) {
        p.drawEllipse(QPointF(startX + i * gap, cy), dotR, dotR);
    }

    return pm;
}

QPixmap ChatWallpaperTile(int size) {
    const int s = qMax(160, size);
    QPixmap pm(s, s);
    const QColor base = Theme::uiWindowBg();
    pm.fill(base);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor accent = Theme::uiAccentBlue();
    accent.setAlpha(18);
    QColor accent2 = Theme::uiSelectedBg();
    accent2.setAlpha(22);
    QColor line = Theme::uiBorder();
    line.setAlpha(18);

    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawEllipse(QPointF(s * 0.22, s * 0.18), s * 0.06, s * 0.06);
    p.drawEllipse(QPointF(s * 0.76, s * 0.32), s * 0.08, s * 0.08);

    p.setBrush(accent2);
    p.drawRoundedRect(QRectF(s * 0.12, s * 0.58, s * 0.22, s * 0.12), 12, 12);
    p.drawRoundedRect(QRectF(s * 0.58, s * 0.62, s * 0.26, s * 0.12), 12, 12);

    QPen pen(line);
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawLine(QPointF(s * 0.08, s * 0.42), QPointF(s * 0.92, s * 0.42));
    p.drawLine(QPointF(s * 0.2, s * 0.78), QPointF(s * 0.84, s * 0.78));

    return pm;
}

QStringList BuiltinStickers() {
    return {QStringLiteral("s1"), QStringLiteral("s2"), QStringLiteral("s3"), QStringLiteral("s4"),
            QStringLiteral("s5"), QStringLiteral("s6"), QStringLiteral("s7"), QStringLiteral("s8")};
}

QString GroupRoleText(int role) {
    switch (role) {
        case 0:
            return QStringLiteral("群主");
        case 1:
            return QStringLiteral("管理员");
        case 2:
        default:
            return QStringLiteral("成员");
    }
}

QString ExtractFirstUrl(const QString &text) {
    const int httpPos = text.indexOf(QStringLiteral("http://"), 0, Qt::CaseInsensitive);
    const int httpsPos = text.indexOf(QStringLiteral("https://"), 0, Qt::CaseInsensitive);
    int start = -1;
    if (httpPos >= 0 && httpsPos >= 0) {
        start = qMin(httpPos, httpsPos);
    } else if (httpPos >= 0) {
        start = httpPos;
    } else if (httpsPos >= 0) {
        start = httpsPos;
    }
    if (start < 0) {
        return {};
    }

    int end = start;
    while (end < text.size()) {
        const QChar ch = text.at(end);
        if (ch.isSpace() || ch == QChar::LineFeed || ch == QChar::CarriageReturn ||
            ch == QChar::Tabulation) {
            break;
        }
        ++end;
    }
    QString url = text.mid(start, end - start);
    while (!url.isEmpty()) {
        const QChar tail = url.at(url.size() - 1);
        if (tail == QChar('.') || tail == QChar(',') || tail == QChar(';') || tail == QChar(':') ||
            tail == QChar(')') || tail == QChar(']') || tail == QChar('}') ||
            tail == QChar('"') || tail == QChar('\'')) {
            url.chop(1);
            continue;
        }
        break;
    }
    return url;
}

bool IsNotFriendsError(const QString &err) {
    const QString e = err.trimmed();
    return e.compare(QStringLiteral("not friends"), Qt::CaseInsensitive) == 0 ||
           e.contains(QStringLiteral("not friends"), Qt::CaseInsensitive);
}

bool IsNonRetryableSendError(const QString &err) {
    const QString e = err.trimmed().toLower();
    if (e.isEmpty()) {
        return false;
    }
    if (e.contains(QStringLiteral("not friends"))) {
        return true;
    }
    if (e.contains(QStringLiteral("recipient not found")) ||
        e.contains(QStringLiteral("invalid recipient")) ||
        e.contains(QStringLiteral("recipient empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("payload too large")) ||
        e.contains(QStringLiteral("payload empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("peer empty"))) {
        return true;
    }
    if (e.contains(QStringLiteral("not in group"))) {
        return true;
    }
    return false;
}

class NoCookieJar : public QNetworkCookieJar {
public:
    using QNetworkCookieJar::QNetworkCookieJar;

    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override {
        Q_UNUSED(cookieList);
        Q_UNUSED(url);
        return false;
    }
};

struct LinkPreviewData {
    QString title;
    QString description;
};

QString DecodeHtmlEntities(QString s) {
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""), Qt::CaseInsensitive);
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "), Qt::CaseInsensitive);
    return s;
}

bool ParseLinkPreviewFromHtml(const QString &html, LinkPreviewData &out) {
    out = {};
    if (html.isEmpty()) {
        return false;
    }

    QString title;
    QString description;

    {
        static const QRegularExpression kTitleRe(
            QStringLiteral("<title\\b[^>]*>(.*?)</title>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        const auto m = kTitleRe.match(html);
        if (m.hasMatch()) {
            title = DecodeHtmlEntities(m.captured(1)).simplified();
        }
    }

    static const QRegularExpression kMetaRe(
        QStringLiteral("<meta\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression kAttrRe(
        QStringLiteral("([A-Za-z_:][-A-Za-z0-9_:]*)\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\\s>]+))"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = kMetaRe.globalMatch(html);
    while (it.hasNext()) {
        const QString meta = it.next().captured(0);
        QString key;
        QString content;

        auto ait = kAttrRe.globalMatch(meta);
        while (ait.hasNext()) {
            const auto am = ait.next();
            const QString attr = am.captured(1).toLower();
            QString val = am.captured(3);
            if (val.isEmpty()) {
                val = am.captured(4);
            }
            if (val.isEmpty()) {
                val = am.captured(5);
            }
            if (attr == QStringLiteral("property") || attr == QStringLiteral("name")) {
                key = val.toLower();
            } else if (attr == QStringLiteral("content")) {
                content = val;
            }
        }

        if (key.isEmpty() || content.isEmpty()) {
            continue;
        }

        const QString val = DecodeHtmlEntities(content).simplified();
        if (val.isEmpty()) {
            continue;
        }

        if (key == QStringLiteral("og:title") || key == QStringLiteral("twitter:title")) {
            if (title.isEmpty()) {
                title = val;
            }
        } else if (key == QStringLiteral("og:description") ||
                   key == QStringLiteral("description") ||
                   key == QStringLiteral("twitter:description")) {
            if (description.isEmpty()) {
                description = val;
            }
        }
    }

    out.title = title;
    out.description = description;
    return !out.title.isEmpty() || !out.description.isEmpty();
}

void ShowLinkPreviewDialog(QWidget *parent, const QUrl &url) {
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        QMessageBox::warning(parent, QStringLiteral("链接预览"), QStringLiteral("无效链接"));
        return;
    }

    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle(QStringLiteral("链接预览"));
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->resize(560, 360);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *urlLabel = new QLabel(dlg);
    urlLabel->setTextFormat(Qt::PlainText);
    urlLabel->setWordWrap(true);
    urlLabel->setText(url.toString(QUrl::FullyDecoded));

    auto *statusLabel = new QLabel(dlg);
    statusLabel->setTextFormat(Qt::PlainText);
    statusLabel->setWordWrap(true);
    statusLabel->setText(UiSettings::Tr(
        QStringLiteral("正在获取预览…（提示：将直连目标网站，可能暴露你的 IP）"),
        QStringLiteral("Fetching preview… (Privacy: direct connection may expose your IP)")));
    statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextSub().name()));

    auto *titleLabel = new QLabel(dlg);
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setWordWrap(true);
    titleLabel->setText(UiSettings::Tr(QStringLiteral("标题："), QStringLiteral("Title:")));

    auto *descLabel = new QLabel(dlg);
    descLabel->setTextFormat(Qt::PlainText);
    descLabel->setWordWrap(true);
    descLabel->setText(UiSettings::Tr(QStringLiteral("描述："), QStringLiteral("Description:")));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    auto *btnOpen = buttons->addButton(
        UiSettings::Tr(QStringLiteral("打开链接"), QStringLiteral("Open link")),
        QDialogButtonBox::ActionRole);
    auto *btnCopy = buttons->addButton(
        UiSettings::Tr(QStringLiteral("复制链接"), QStringLiteral("Copy link")),
        QDialogButtonBox::ActionRole);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    QObject::connect(btnOpen, &QPushButton::clicked, dlg, [url]() { QDesktopServices::openUrl(url); });
    QObject::connect(btnCopy, &QPushButton::clicked, dlg,
                     [url]() { SecureClipboard::SetText(url.toString(QUrl::FullyDecoded)); });

    root->addWidget(urlLabel);
    root->addWidget(statusLabel);
    root->addWidget(titleLabel);
    root->addWidget(descLabel, 1);
    root->addWidget(buttons);

    auto *manager = new QNetworkAccessManager(dlg);
    manager->setCookieJar(new NoCookieJar(manager));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MI_E2EE_LinkPreview/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(8000);

    const int kMaxHtmlBytes = 512 * 1024;
    auto htmlBytes = std::make_shared<QByteArray>();
    htmlBytes->reserve(kMaxHtmlBytes);
    auto truncated = std::make_shared<bool>(false);

    QNetworkReply *reply = manager->get(req);
    QObject::connect(reply, &QNetworkReply::readyRead, dlg, [reply, htmlBytes, truncated]() {
        const QByteArray chunk = reply->readAll();
        const int kMaxHtmlBytes = 512 * 1024;
        if (chunk.isEmpty()) {
            return;
        }
        if (htmlBytes->size() >= kMaxHtmlBytes) {
            *truncated = true;
            return;
        }
        const int remaining = kMaxHtmlBytes - htmlBytes->size();
        if (chunk.size() > remaining) {
            htmlBytes->append(chunk.left(remaining));
            *truncated = true;
        } else {
            htmlBytes->append(chunk);
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, dlg,
                     [reply, htmlBytes, truncated, statusLabel, titleLabel, descLabel]() {
        const QString contentType =
            reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            statusLabel->setText(
                err.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("预览失败"), QStringLiteral("Preview failed"))
                    : UiSettings::Tr(QStringLiteral("预览失败：%1").arg(err),
                                     QStringLiteral("Preview failed: %1").arg(err)));
            return;
        }

        if (status >= 400) {
            statusLabel->setText(
                UiSettings::Tr(QStringLiteral("预览失败：HTTP %1").arg(status),
                               QStringLiteral("Preview failed: HTTP %1").arg(status)));
            return;
        }

        if (!contentType.isEmpty() && !contentType.contains(QStringLiteral("text/html"), Qt::CaseInsensitive)) {
            statusLabel->setText(
                UiSettings::Tr(QStringLiteral("无法预览：内容类型 %1").arg(contentType),
                               QStringLiteral("Cannot preview: content type %1").arg(contentType)));
            return;
        }

        const QString html = QString::fromUtf8(*htmlBytes);
        LinkPreviewData data;
        if (!ParseLinkPreviewFromHtml(html, data)) {
            statusLabel->setText(
                UiSettings::Tr(QStringLiteral("无法预览：未找到标题/描述"),
                               QStringLiteral("Cannot preview: missing title/description")));
            return;
        }

        titleLabel->setText(QStringLiteral("标题：%1").arg(data.title.isEmpty() ? QStringLiteral("(无)") : data.title));
        descLabel->setText(QStringLiteral("描述：%1").arg(data.description.isEmpty() ? QStringLiteral("(无)") : data.description));

        if (*truncated) {
            statusLabel->setText(QStringLiteral("预览成功（内容已截断）"));
        } else {
            statusLabel->setText(QStringLiteral("预览成功"));
        }
    });

    dlg->show();
}

void ShowImageDialog(QWidget *parent, const QImage &img, const QString &title) {
    if (img.isNull()) {
        QMessageBox::warning(parent, QStringLiteral("预览图片"),
                             QStringLiteral("图片解码失败"));
        return;
    }

    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle(title.isEmpty() ? QStringLiteral("预览图片") : title);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->resize(720, 520);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *label = new QLabel(dlg);
    label->setAlignment(Qt::AlignCenter);
    label->setBackgroundRole(QPalette::Base);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    label->setScaledContents(false);

    auto *scroll = new QScrollArea(dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(label);

    QPixmap px = QPixmap::fromImage(img);
    label->setPixmap(px);
    label->adjustSize();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);

    root->addWidget(scroll, 1);
    root->addWidget(buttons);

    dlg->show();
}

#if defined(MI_UI_HAS_QT_MULTIMEDIA)
void ShowAudioDialog(QWidget *parent, const QString &title, const std::function<void(QMediaPlayer *)> &setSource) {
    auto *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle(title.isEmpty() ? UiSettings::Tr(QStringLiteral("播放语音"),
                                                         QStringLiteral("Play Audio"))
                                        : title);
    dlg->resize(520, 140);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *player = new QMediaPlayer(dlg);
    auto *audio = new QAudioOutput(dlg);
    player->setAudioOutput(audio);
    audio->setVolume(1.0);

    auto *row = new QHBoxLayout();
    auto *playBtn = new QPushButton(UiSettings::Tr(QStringLiteral("暂停"), QStringLiteral("Pause")), dlg);
    playBtn->setFixedWidth(80);
    auto *slider = new QSlider(Qt::Horizontal, dlg);
    slider->setRange(0, 0);
    auto *timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), dlg);
    timeLabel->setMinimumWidth(90);

    row->addWidget(playBtn);
    row->addWidget(slider, 1);
    row->addWidget(timeLabel);
    root->addLayout(row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    root->addWidget(buttons);

    QObject::connect(playBtn, &QPushButton::clicked, dlg, [player, playBtn]() {
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
            playBtn->setText(UiSettings::Tr(QStringLiteral("播放"), QStringLiteral("Play")));
        } else {
            player->play();
            playBtn->setText(UiSettings::Tr(QStringLiteral("暂停"), QStringLiteral("Pause")));
        }
    });

    QObject::connect(player, &QMediaPlayer::durationChanged, dlg, [slider](qint64 dur) {
        slider->setRange(0, dur > 0 ? static_cast<int>(dur) : 0);
    });
    QObject::connect(player, &QMediaPlayer::positionChanged, dlg, [slider](qint64 pos) {
        if (!slider->isSliderDown()) {
            slider->setValue(pos > 0 ? static_cast<int>(pos) : 0);
        }
    });
    QObject::connect(slider, &QSlider::sliderMoved, dlg, [player](int v) {
        player->setPosition(static_cast<qint64>(v));
    });

    const auto fmtTime = [](qint64 ms) -> QString {
        if (ms < 0) {
            ms = 0;
        }
        const qint64 sec = ms / 1000;
        const qint64 m = sec / 60;
        const qint64 s = sec % 60;
        return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    };
    QObject::connect(player, &QMediaPlayer::positionChanged, dlg,
                     [player, timeLabel, fmtTime](qint64 pos) {
                         timeLabel->setText(fmtTime(pos) + QStringLiteral(" / ") +
                                            fmtTime(player->duration()));
                     });
    QObject::connect(player, &QMediaPlayer::durationChanged, dlg,
                     [timeLabel, fmtTime](qint64 dur) {
                         timeLabel->setText(fmtTime(0) + QStringLiteral(" / ") + fmtTime(dur));
                     });

    setSource(player);
    player->play();
    dlg->show();
}

void ShowVideoDialog(QWidget *parent, const QString &title, const std::function<void(QMediaPlayer *)> &setSource) {
    auto *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle(title.isEmpty() ? UiSettings::Tr(QStringLiteral("播放视频"),
                                                         QStringLiteral("Play Video"))
                                        : title);
    dlg->resize(860, 560);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *player = new QMediaPlayer(dlg);
    auto *audio = new QAudioOutput(dlg);
    player->setAudioOutput(audio);
    audio->setVolume(1.0);

    auto *video = new QVideoWidget(dlg);
    video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    player->setVideoOutput(video);
    root->addWidget(video, 1);

    auto *row = new QHBoxLayout();
    auto *playBtn = new QPushButton(UiSettings::Tr(QStringLiteral("暂停"), QStringLiteral("Pause")), dlg);
    playBtn->setFixedWidth(80);
    auto *slider = new QSlider(Qt::Horizontal, dlg);
    slider->setRange(0, 0);
    auto *timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), dlg);
    timeLabel->setMinimumWidth(90);
    row->addWidget(playBtn);
    row->addWidget(slider, 1);
    row->addWidget(timeLabel);
    root->addLayout(row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    root->addWidget(buttons);

    QObject::connect(playBtn, &QPushButton::clicked, dlg, [player, playBtn]() {
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
            playBtn->setText(UiSettings::Tr(QStringLiteral("播放"), QStringLiteral("Play")));
        } else {
            player->play();
            playBtn->setText(UiSettings::Tr(QStringLiteral("暂停"), QStringLiteral("Pause")));
        }
    });

    QObject::connect(player, &QMediaPlayer::durationChanged, dlg, [slider](qint64 dur) {
        slider->setRange(0, dur > 0 ? static_cast<int>(dur) : 0);
    });
    QObject::connect(player, &QMediaPlayer::positionChanged, dlg, [slider](qint64 pos) {
        if (!slider->isSliderDown()) {
            slider->setValue(pos > 0 ? static_cast<int>(pos) : 0);
        }
    });
    QObject::connect(slider, &QSlider::sliderMoved, dlg, [player](int v) {
        player->setPosition(static_cast<qint64>(v));
    });

    const auto fmtTime = [](qint64 ms) -> QString {
        if (ms < 0) {
            ms = 0;
        }
        const qint64 sec = ms / 1000;
        const qint64 m = sec / 60;
        const qint64 s = sec % 60;
        return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    };
    QObject::connect(player, &QMediaPlayer::positionChanged, dlg,
                     [player, timeLabel, fmtTime](qint64 pos) {
                         timeLabel->setText(fmtTime(pos) + QStringLiteral(" / ") +
                                            fmtTime(player->duration()));
                     });
    QObject::connect(player, &QMediaPlayer::durationChanged, dlg,
                     [timeLabel, fmtTime](qint64 dur) {
                         timeLabel->setText(fmtTime(0) + QStringLiteral(" / ") + fmtTime(dur));
                     });

    setSource(player);
    player->play();
    dlg->show();
}
#endif

IconButton *titleIconSvg(const QString &svgPath, QWidget *parent) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(24, 24);
    btn->setSvgIcon(svgPath, 16);
    btn->setColors(Theme::uiTextMain(), Theme::uiTextMain(), Theme::uiTextMain(),
                   QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
    return btn;
}

IconButton *toolIcon(const QString &glyph, QWidget *parent) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(24, 24);
    btn->setColors(Theme::uiTextSub(), Theme::uiTextMain(), Theme::uiTextMain(),
                   QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
    return btn;
}

IconButton *toolIconSvg(const QString &svgPath, QWidget *parent) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(24, 24);
    btn->setSvgIcon(svgPath, 16);
    btn->setColors(Theme::uiTextSub(), Theme::uiTextMain(), Theme::uiTextMain(),
                   QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
    return btn;
}

IconButton *composerIconSvg(const QString &svgPath, QWidget *parent) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(34, 34);
    btn->setSvgIcon(svgPath, 14);
    btn->setPadding(2);
    btn->setColors(Theme::uiTextSub(), Theme::uiTextMain(), Theme::uiTextMain(),
                   QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
    return btn;
}

IconButton *accentIconSvg(const QString &svgPath, QWidget *parent) {
    auto *btn = new IconButton(QString(), parent);
    btn->setFixedSize(34, 34);
    btn->setSvgIcon(svgPath, 14);
    btn->setRound(true);
    btn->setPadding(3);
    const QColor base = Theme::uiAccentBlue();
    btn->setColors(Qt::white, Qt::white, Qt::white,
                   base, base.lighter(110), base.darker(115));
    return btn;
}

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedSize(82, 32);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; "
            "border-radius: 12px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %5; }")
            .arg(Theme::uiTextMain().name(),
                 Theme::uiPanelBg().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name(),
                 Theme::uiSelectedBg().name()));
    return btn;
}

QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedHeight(32);
    const QColor base = Theme::uiAccentBlue();
    const QColor hover = base.lighter(115);
    const QColor pressed = base.darker(110);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: white; background: %1; border: none; "
            "border-radius: 16px; padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %3; }"
            "QPushButton:pressed { background: %4; }")
            .arg(base.name(), base.name(), hover.name(), pressed.name()));
    return btn;
}

}  // namespace

ChatWindow::ChatWindow(BackendAdapter *backend, QWidget *parent)
    : FramelessWindowBase(parent), backend_(backend) {
    embeddedMode_ = isEmbedded();
    if (!embeddedMode_) {
        resize(906, 902);
        setMinimumSize(640, 540);
    } else {
        setMinimumSize(320, 360);
    }
    buildUi();
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
}

QString ChatWindow::conversationId() const { return conversationId_; }

bool ChatWindow::isChineseInputMode() const {
    if (!inputEdit_) {
        return true;
    }
    return inputEdit_->isChineseMode();
}

void ChatWindow::setEmbeddedMode(bool embedded) {
    embeddedMode_ = embedded;
    if (windowDownBtn_) {
        windowDownBtn_->setVisible(!embeddedMode_);
    }
    if (windowMinBtn_) {
        windowMinBtn_->setVisible(!embeddedMode_);
    }
    if (windowCloseBtn_) {
        windowCloseBtn_->setVisible(!embeddedMode_);
    }
}

void ChatWindow::focusMessageInput() {
    if (!inputEdit_ || !inputEdit_->isEnabled() || !inputEdit_->isVisible()) {
        return;
    }
    inputEdit_->setFocus(Qt::OtherFocusReason);
    QTextCursor cursor = inputEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    inputEdit_->setTextCursor(cursor);
}

void ChatWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(Theme::kTitleBarHeight);
    titleBar->setStyleSheet(QStringLiteral("background: %1;").arg(ChatTokens::windowBg().name()));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 8, 12, 8);
    titleLayout->setSpacing(8);

    titleIcon_ = new QLabel(titleBar);
    titleIcon_->setFixedSize(20, 20);
    titleIcon_->setAlignment(Qt::AlignCenter);
    titleIcon_->setVisible(false);
    titleLayout->addWidget(titleIcon_);

    auto *titleTextWrap = new QWidget(titleBar);
    auto *titleTextLayout = new QVBoxLayout(titleTextWrap);
    titleTextLayout->setContentsMargins(0, 0, 0, 0);
    titleTextLayout->setSpacing(0);

    titleLabel_ = new QLabel(UiSettings::Tr(QStringLiteral("会话"),
                                            QStringLiteral("Chat")),
                             titleBar);
    titleLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
                                   .arg(ChatTokens::textMain().name()));
    titleTextLayout->addWidget(titleLabel_);

    presenceLabel_ = new QLabel(titleBar);
    presenceLabel_->setVisible(false);
    presenceLabel_->setTextFormat(Qt::PlainText);
    presenceLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                      .arg(ChatTokens::textMuted().name()));
    presenceLabel_->setText(QString());
    titleTextLayout->addWidget(presenceLabel_);

    titleLayout->addWidget(titleTextWrap);
    titleLayout->addStretch();

    auto addTitleAction = [&](const QString &svg, const QString &tip, std::function<void()> onClick) {
        auto *btn = titleIconSvg(svg, titleBar);
        btn->setColors(ChatTokens::textSub(), ChatTokens::textMain(), ChatTokens::textMain(),
                       QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
        btn->setToolTip(tip);
        connect(btn, &QAbstractButton::clicked, this, std::move(onClick));
        titleActionButtons_.push_back(btn);
        titleLayout->addWidget(btn);
    };

    addTitleAction(QStringLiteral(":/mi/e2ee/ui/icons/phone.svg"),
                   UiSettings::Tr(QStringLiteral("语音通话（未实现）"),
                                  QStringLiteral("Voice call (TODO)")),
                   [this]() {
                       Toast::Show(this,
                                   UiSettings::Tr(QStringLiteral("暂未实现语音通话"),
                                                  QStringLiteral("Voice call is not implemented yet.")),
                                   Toast::Level::Info);
                   });
    addTitleAction(QStringLiteral(":/mi/e2ee/ui/icons/video.svg"),
                   UiSettings::Tr(QStringLiteral("视频通话（未实现）"),
                                  QStringLiteral("Video call (TODO)")),
                   [this]() {
                       Toast::Show(this,
                                   UiSettings::Tr(QStringLiteral("暂未实现视频通话"),
                                                  QStringLiteral("Video call is not implemented yet.")),
                                   Toast::Level::Info);
                   });
    addTitleAction(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"),
                   UiSettings::Tr(QStringLiteral("会话内搜索"),
                                  QStringLiteral("Search in chat")),
                   [this]() { toggleSearchBar(); });
    addTitleAction(QStringLiteral(":/mi/e2ee/ui/icons/more.svg"),
                   UiSettings::Tr(QStringLiteral("更多"),
                                  QStringLiteral("More")),
                   [this]() {
                       if (conversationId_.trimmed().isEmpty()) {
                           Toast::Show(this,
                                       UiSettings::Tr(QStringLiteral("请先选择会话"),
                                                      QStringLiteral("Select a chat first")),
                                       Toast::Level::Info);
                           return;
                       }
                       ConversationDetailsDialog dlg(backend_,
                                                     conversationId_,
                                                     titleLabel_ ? titleLabel_->text() : conversationId_,
                                                     isGroup_,
                                                     this);
                       dlg.setStartPage(ConversationDetailsDialog::StartPage::Info);
                       dlg.exec();
                   });

    windowDownBtn_ = titleIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/chevron-down.svg"), titleBar);
    windowMinBtn_ = titleIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/minimize.svg"), titleBar);
    windowCloseBtn_ = titleIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), titleBar);
    connect(windowMinBtn_, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(windowCloseBtn_, &QPushButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(windowDownBtn_);
    titleLayout->addWidget(windowMinBtn_);
    titleLayout->addWidget(windowCloseBtn_);
    setEmbeddedMode(embeddedMode_);

    root->addWidget(titleBar);
    setTitleBar(titleBar);

    // Message area
    auto *body = new QWidget(central);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *messageArea = new QWidget(body);
    messageArea->setStyleSheet(QString());
    messageArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *msgLayout = new QVBoxLayout(messageArea);
    msgLayout->setContentsMargins(6, 6, 6, 0);
    msgLayout->setSpacing(0);
    messageModel_ = new MessageModel(this);
    messageStack_ = new QStackedWidget(messageArea);
    messageStack_->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
    messageStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *emptyState = new QWidget(messageStack_);
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch();
    auto *emptyIcon = new QLabel(emptyState);
    emptyIcon->setPixmap(EmptyChatIcon(72));
    emptyIcon->setAlignment(Qt::AlignHCenter);
    emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);
    emptyTitleLabel_ = new QLabel(QString(), emptyState);
    emptyTitleLabel_->setAlignment(Qt::AlignHCenter);
    emptyTitleLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
                                        .arg(ChatTokens::textMain().name()));
    emptyLayout->addWidget(emptyTitleLabel_);
    emptySubLabel_ = new QLabel(QString(), emptyState);
    emptySubLabel_->setAlignment(Qt::AlignHCenter);
    emptySubLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                      .arg(ChatTokens::textMuted().name()));
    emptyLayout->addWidget(emptySubLabel_);
    emptyLayout->addStretch();
    messageStack_->addWidget(emptyState);

    searchBar_ = new QWidget(messageArea);
    searchBar_->setVisible(false);
    auto *searchBarLayout = new QHBoxLayout(searchBar_);
    searchBarLayout->setContentsMargins(10, 0, 10, 8);
    searchBarLayout->setSpacing(0);

    auto *searchBox = new QFrame(searchBar_);
    searchBox->setFixedHeight(34);
    searchBox->setStyleSheet(
        QStringLiteral(
            "QFrame { background: %1; border-radius: 17px; border: 1px solid %2; }"
            "QLineEdit { background: transparent; border: none; color: %3; font-size: 12px; }"
            "QLabel { color: %4; font-size: 12px; }")
            .arg(Theme::uiSearchBg().name(),
                 ChatTokens::border().name(),
                 ChatTokens::textMain().name(),
                 ChatTokens::textMuted().name()));
    auto *searchBoxLayout = new QHBoxLayout(searchBox);
    searchBoxLayout->setContentsMargins(10, 4, 8, 4);
    searchBoxLayout->setSpacing(6);

    auto *searchIcon = new QLabel(searchBox);
    searchIcon->setFixedSize(16, 16);
    searchIcon->setPixmap(
        UiIcons::TintedSvg(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"),
                           16,
                           ChatTokens::textMuted(),
                           searchIcon->devicePixelRatioF()));
    searchIcon->setAlignment(Qt::AlignCenter);

    searchEdit_ = new QLineEdit(searchBox);
    searchEdit_->setPlaceholderText(
        UiSettings::Tr(QStringLiteral("搜索消息"), QStringLiteral("Search messages")));
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->installEventFilter(this);

    searchCountLabel_ = new QLabel(searchBox);
    searchCountLabel_->setText(QString());
    searchCountLabel_->setMinimumWidth(46);
    searchCountLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto configureNavBtn = [&](IconButton *btn) {
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFixedSize(24, 24);
        btn->setPadding(6);
        btn->setColors(ChatTokens::textSub(), ChatTokens::textMain(), ChatTokens::textMain(),
                       QColor(0, 0, 0, 0), ChatTokens::hoverBg(), ChatTokens::selectedBg());
    };

    searchPrevBtn_ = new IconButton(QStringLiteral("↑"), searchBox);
    searchPrevBtn_->setGlyph(QStringLiteral("↑"), 10);
    configureNavBtn(searchPrevBtn_);
    searchPrevBtn_->setToolTip(UiSettings::Tr(QStringLiteral("上一个"), QStringLiteral("Previous")));

    searchNextBtn_ = new IconButton(QStringLiteral("↓"), searchBox);
    searchNextBtn_->setGlyph(QStringLiteral("↓"), 10);
    configureNavBtn(searchNextBtn_);
    searchNextBtn_->setToolTip(UiSettings::Tr(QStringLiteral("下一个"), QStringLiteral("Next")));

    searchCloseBtn_ = new IconButton(QString(), searchBox);
    searchCloseBtn_->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 12);
    searchCloseBtn_->setFocusPolicy(Qt::NoFocus);
    searchCloseBtn_->setFixedSize(24, 24);
    searchCloseBtn_->setPadding(6);
    searchCloseBtn_->setToolTip(UiSettings::Tr(QStringLiteral("关闭搜索"), QStringLiteral("Close search")));
    searchCloseBtn_->setColors(ChatTokens::textSub(), ChatTokens::textMain(), Theme::uiDangerRed(),
                               QColor(0, 0, 0, 0), ChatTokens::hoverBg(), ChatTokens::selectedBg());

    searchBoxLayout->addWidget(searchIcon);
    searchBoxLayout->addWidget(searchEdit_, 1);
    searchBoxLayout->addWidget(searchCountLabel_);
    searchBoxLayout->addWidget(searchPrevBtn_);
    searchBoxLayout->addWidget(searchNextBtn_);
    searchBoxLayout->addWidget(searchCloseBtn_);
    searchBarLayout->addWidget(searchBox, 1);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this]() { updateSearchResults(); });
    connect(searchPrevBtn_, &QAbstractButton::clicked, this, [this]() { stepSearchResult(-1); });
    connect(searchNextBtn_, &QAbstractButton::clicked, this, [this]() { stepSearchResult(1); });
    connect(searchCloseBtn_, &QAbstractButton::clicked, this, [this]() { setSearchActive(false); });

    searchPrevBtn_->setEnabled(false);
    searchNextBtn_->setEnabled(false);

    messageView_ = new QListView(messageStack_);
    messageView_->setFrameShape(QFrame::NoFrame);
    messageDelegate_ = new MessageDelegate(messageView_);
    messageView_->setItemDelegate(messageDelegate_);
    messageView_->setModel(messageModel_);
    messageView_->setSelectionMode(QAbstractItemView::NoSelection);
    messageView_->setFocusPolicy(Qt::NoFocus);
    messageView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    messageView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageView_->setStyleSheet(
        QStringLiteral(
            "QListView { background: transparent; outline: none; border: 1px solid transparent; border-radius: 10px; }"
            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }")
            .arg(Theme::uiScrollBarHandle().name(),
                 Theme::uiScrollBarHandleHover().name()));
    const QPixmap wallpaper = ChatWallpaperTile(240);
    messageArea->setAutoFillBackground(true);
    QPalette areaPal = messageArea->palette();
    areaPal.setBrush(QPalette::Window, QBrush(wallpaper));
    messageArea->setPalette(areaPal);
    messageView_->viewport()->setAutoFillBackground(true);
    QPalette viewPal = messageView_->viewport()->palette();
    viewPal.setBrush(QPalette::Base, QBrush(wallpaper));
    viewPal.setBrush(QPalette::Window, QBrush(wallpaper));
    messageView_->viewport()->setPalette(viewPal);
    messageView_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(messageView_, &QListView::customContextMenuRequested, this, &ChatWindow::showMessageMenu);
    connect(messageModel_, &QAbstractItemModel::modelReset, this, [this]() { updateEmptyState(); });
    connect(messageModel_, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int, int) { updateEmptyState(); });
    connect(messageModel_, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex &, int, int) { updateEmptyState(); });

    newMessagePill_ = new QPushButton(messageView_->viewport());
    newMessagePill_->setVisible(false);
    newMessagePill_->setCursor(Qt::PointingHandCursor);
    newMessagePill_->setFocusPolicy(Qt::NoFocus);
    newMessagePill_->setStyleSheet(
        QStringLiteral(
            "QPushButton { background: %1; color: white; border: 1px solid rgba(255,255,255,30); "
            "border-radius: 16px; padding: 6px 12px; font-size: 12px; }"
            "QPushButton:hover { background: %2; }"
            "QPushButton:pressed { background: %3; }")
            .arg(Theme::uiAccentBlue().name(),
                 Theme::uiAccentBlue().lighter(110).name(),
                 Theme::uiAccentBlue().darker(110).name()));
    connect(newMessagePill_, &QPushButton::clicked, this, [this]() {
        clearNewMessagePill();
        if (messageView_) {
            messageView_->scrollToBottom();
        }
    });
    messageView_->viewport()->installEventFilter(this);
    if (auto *sb = messageView_->verticalScrollBar()) {
        connect(sb, &QScrollBar::valueChanged, this, [this](int) {
            if (isNearBottom()) {
                clearNewMessagePill();
            }
        });
    }
    connect(messageModel_, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int first, int last) {
                if (!messageView_ || !messageModel_) {
                    return;
                }
                bool anyOutgoing = false;
                int newCount = 0;
                for (int row = first; row <= last; ++row) {
                    const QModelIndex idx = messageModel_->index(row, 0);
                    if (!idx.isValid()) {
                        continue;
                    }
                    const auto t =
                        static_cast<MessageItem::Type>(idx.data(MessageModel::TypeRole).toInt());
                    if (t == MessageItem::Type::TimeDivider) {
                        continue;
                    }
                    ++newCount;
                    if (idx.data(MessageModel::OutgoingRole).toBool()) {
                        anyOutgoing = true;
                    }
                }
                if (newCount <= 0) {
                    return;
                }
                lastMessageInsertMs_ = QDateTime::currentMSecsSinceEpoch();
                refreshFileTransferAnimation();
                if (anyOutgoing || isNearBottom()) {
                    clearNewMessagePill();
                    messageView_->scrollToBottom();
                    return;
                }
                bumpNewMessagePill(newCount);
            });

    connect(messageView_, &QListView::doubleClicked, this, &ChatWindow::activateMessage);

    fileTransferAnimTimer_ = new QTimer(this);
    fileTransferAnimTimer_->setInterval(50);
    connect(fileTransferAnimTimer_, &QTimer::timeout, this, [this]() {
        if (messageView_) {
            messageView_->viewport()->update();
        }
        refreshFileTransferAnimation();
    });
    messageStack_->addWidget(messageView_);
    msgLayout->addWidget(searchBar_, 0);
    msgLayout->addWidget(messageStack_, 1);
    bodyLayout->addWidget(messageArea, 1);

    // Composer
    composer_ = new QWidget(body);
    composer_->setStyleSheet(QStringLiteral("background: transparent;"));
    composer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *composerLayout = new QVBoxLayout(composer_);
    composerLayout->setContentsMargins(12, 4, 12, 6);
    composerLayout->setSpacing(4);

    replyBar_ = new QWidget(composer_);
    replyBar_->setVisible(false);
    replyBar_->setStyleSheet(
        QStringLiteral(
            "QWidget { background: %1; border: 1px solid %2; border-radius: 10px; }")
            .arg(Theme::uiInputBg().name(), Theme::uiInputBorder().name()));
    auto *replyLayout = new QHBoxLayout(replyBar_);
    replyLayout->setContentsMargins(8, 5, 8, 5);
    replyLayout->setSpacing(6);
    replyLabel_ = new QLabel(replyBar_);
    replyLabel_->setTextFormat(Qt::PlainText);
    replyLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                   .arg(ChatTokens::textSub().name()));
    replyLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    replyLabel_->setText(QStringLiteral(""));
    replyLayout->addWidget(replyLabel_, 1);
    auto *replyCancel = new IconButton(QString(), replyBar_);
    replyCancel->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 12);
    replyCancel->setFixedSize(22, 22);
    replyCancel->setPadding(4);
    replyCancel->setColors(Theme::uiTextSub(), Theme::uiTextMain(), Theme::uiTextMain(),
                           QColor(0, 0, 0, 0), Theme::uiHoverBg(), Theme::uiSelectedBg());
    connect(replyCancel, &QAbstractButton::clicked, this, &ChatWindow::clearReplyContext);
    replyLayout->addWidget(replyCancel, 0, Qt::AlignRight);
    composerLayout->addWidget(replyBar_);

    typingLabel_ = new QLabel(composer_);
    typingLabel_->setVisible(false);
    typingLabel_->setTextFormat(Qt::PlainText);
    typingLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                    .arg(ChatTokens::textSub().name()));
    typingLabel_->setText(
        UiSettings::Tr(QStringLiteral("对方正在输入..."), QStringLiteral("Typing...")));
    composerLayout->addWidget(typingLabel_);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    auto *attachBtn = composerIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/paperclip.svg"), composer_);
    attachBtn->setFocusPolicy(Qt::NoFocus);
    attachBtn->setToolTip(UiSettings::Tr(QStringLiteral("附件"), QStringLiteral("Attachments")));
    inputRow->addWidget(attachBtn);

    attachMenu_ = new QMenu(attachBtn);
    UiStyle::ApplyMenuStyle(*attachMenu_);
    QAction *sendImageAction = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("图片"), QStringLiteral("Photo")));
    connect(sendImageAction, &QAction::triggered, this, &ChatWindow::sendImagePlaceholder);
    QAction *sendVideoAction = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("视频"), QStringLiteral("Video")));
    connect(sendVideoAction, &QAction::triggered, this, &ChatWindow::sendVideoPlaceholder);
    QAction *sendFileAction = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("文件"), QStringLiteral("Document")));
    connect(sendFileAction, &QAction::triggered, this, &ChatWindow::sendFilePlaceholder);
    QAction *sendVoiceAction = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("语音"), QStringLiteral("Voice")));
    connect(sendVoiceAction, &QAction::triggered, this, &ChatWindow::sendVoicePlaceholder);
    attachMenu_->addSeparator();
    sendLocationAction_ = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("位置"), QStringLiteral("Location")));
    connect(sendLocationAction_, &QAction::triggered, this, &ChatWindow::sendLocationPlaceholder);
    sendCardAction_ = attachMenu_->addAction(
        UiSettings::Tr(QStringLiteral("名片"), QStringLiteral("Contact card")));
    connect(sendCardAction_, &QAction::triggered, this, &ChatWindow::sendContactCardPlaceholder);
    connect(attachBtn, &QAbstractButton::clicked, this, [this, attachBtn]() {
        if (attachMenu_) {
            attachMenu_->exec(attachBtn->mapToGlobal(QPoint(0, attachBtn->height())));
        }
    });

    inputEdit_ = new ChatInputEdit(composer_);
    inputEdit_->setPlaceholderText(
        UiSettings::Tr(QStringLiteral("输入消息..."), QStringLiteral("Type a message...")));
    inputEdit_->setTabChangesFocus(true);
    inputEdit_->setStyleSheet(
        QStringLiteral(
            "QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 12px; "
            "color: %3; font-size: 12px; }"
            "QPlainTextEdit:focus { border-color: %4; }")
            .arg(Theme::uiInputBg().name(),
                 Theme::uiInputBorder().name(),
                 Theme::uiTextMain().name(),
                 Theme::uiAccentBlue().name()));
    inputEdit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    inputEdit_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    inputEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    inputEdit_->setMinimumHeight(kComposerInputMinHeight);
    inputEdit_->setMaximumHeight(kComposerInputMaxHeight);
    inputEdit_->setViewportMargins(kComposerInputPaddingH, kComposerInputPaddingV,
                                   kComposerInputPaddingH, kComposerInputPaddingV);
    inputEdit_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputEdit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputEdit_->document()->setDocumentMargin(0.0);
    inputEdit_->installEventFilter(this);
    connect(inputEdit_, &ChatInputEdit::inputModeChanged, this, [this](bool chinese) {
        emit inputModeChanged(chinese);
    });
    emit inputModeChanged(inputEdit_->isChineseMode());
    updateInputHeight();
    inputRow->addWidget(inputEdit_, 1);

    emojiBtn_ = composerIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/emoji.svg"), composer_);
    emojiBtn_->setFocusPolicy(Qt::NoFocus);
    emojiBtn_->setToolTip(UiSettings::Tr(QStringLiteral("表情"), QStringLiteral("Emoji")));
    connect(emojiBtn_, &QAbstractButton::clicked, this, &ChatWindow::showEmojiPicker);
    inputRow->addWidget(emojiBtn_);

    auto *sendBtn = accentIconSvg(QStringLiteral(":/mi/e2ee/ui/icons/send.svg"), composer_);
    sendBtn->setToolTip(UiSettings::Tr(QStringLiteral("发送"), QStringLiteral("Send")));
    connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    inputRow->addWidget(sendBtn);

    composerLayout->addLayout(inputRow);

    typingStopSendTimer_ = new QTimer(this);
    typingStopSendTimer_->setSingleShot(true);
    connect(typingStopSendTimer_, &QTimer::timeout, this, [this]() {
        if (!typingSent_ || isGroup_ || !backend_ || conversationId_.trimmed().isEmpty() ||
            !typingAction_ || !typingAction_->isChecked() || isStealthActive()) {
            typingSent_ = false;
            return;
        }
        QString err;
        backend_->sendTyping(conversationId_, false, err);
        typingSent_ = false;
    });

    typingHideTimer_ = new QTimer(this);
    typingHideTimer_->setSingleShot(true);
    connect(typingHideTimer_, &QTimer::timeout, this, [this]() {
        if (typingLabel_) {
            typingLabel_->setVisible(false);
        }
    });

    presenceHideTimer_ = new QTimer(this);
    presenceHideTimer_->setSingleShot(true);
    connect(presenceHideTimer_, &QTimer::timeout, this, [this]() {
        if (presenceLabel_) {
            presenceLabel_->setVisible(false);
        }
    });

    presencePingTimer_ = new QTimer(this);
    connect(presencePingTimer_, &QTimer::timeout, this, [this]() {
        if (isGroup_ || !backend_ || conversationId_.trimmed().isEmpty() ||
            !presenceAction_ || !presenceAction_->isChecked() || isStealthActive()) {
            return;
        }
        if (!isVisible() || isMinimized() || !isActiveWindow()) {
            return;
        }
        QString err;
        backend_->sendPresence(conversationId_, true, err);
    });

    connect(inputEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        updateInputHeight();
        if (isGroup_ || !backend_ || conversationId_.trimmed().isEmpty() ||
            !typingAction_ || !typingAction_->isChecked() || isStealthActive()) {
            return;
        }
        const QString content = inputEdit_->toPlainText();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (content.trimmed().isEmpty()) {
            if (typingSent_) {
                QString err;
                backend_->sendTyping(conversationId_, false, err);
                typingSent_ = false;
            }
            if (typingStopSendTimer_) {
                typingStopSendTimer_->stop();
            }
            return;
        }
        if (!typingSent_ || now - lastTypingSentMs_ > 2000) {
            QString err;
            if (backend_->sendTyping(conversationId_, true, err)) {
                typingSent_ = true;
                lastTypingSentMs_ = now;
            }
        }
        if (typingStopSendTimer_) {
            typingStopSendTimer_->start(2500);
        }
    });

    sendMenu_ = new QMenu(sendBtn);
    UiStyle::ApplyMenuStyle(*sendMenu_);
    sendStickerAction_ = sendMenu_->addAction(
        UiSettings::Tr(QStringLiteral("发送贴纸..."), QStringLiteral("Send sticker...")));
    connect(sendStickerAction_, &QAction::triggered, this, &ChatWindow::sendStickerPlaceholder);
    sendMenu_->addSeparator();
    readReceiptAction_ = sendMenu_->addAction(UiSettings::Tr(
        QStringLiteral("发送已读回执（默认关闭）"),
        QStringLiteral("Send read receipts (default off)")));
    readReceiptAction_->setCheckable(true);
    readReceiptAction_->setChecked(false);
    typingAction_ = sendMenu_->addAction(
        UiSettings::Tr(QStringLiteral("发送输入状态（默认关闭）"),
                       QStringLiteral("Send typing status (default off)")));
    typingAction_->setCheckable(true);
    typingAction_->setChecked(false);
    connect(typingAction_, &QAction::toggled, this, [this](bool on) {
        if (!on && typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty()) {
            QString err;
            backend_->sendTyping(conversationId_, false, err);
            typingSent_ = false;
            if (typingStopSendTimer_) {
                typingStopSendTimer_->stop();
            }
        }
    });
    presenceAction_ =
        sendMenu_->addAction(UiSettings::Tr(QStringLiteral("在线状态"),
                                            QStringLiteral("Presence")));
    presenceAction_->setCheckable(true);
    presenceAction_->setChecked(false);
    connect(presenceAction_, &QAction::toggled, this, [this](bool on) {
        if (presenceHideTimer_) {
            presenceHideTimer_->stop();
        }
        if (!presencePingTimer_) {
            return;
        }
        presencePingTimer_->stop();
        if (isStealthActive()) {
            if (!isGroup_ && backend_ && !conversationId_.trimmed().isEmpty()) {
                QString err;
                backend_->sendPresence(conversationId_, false, err);
            }
            return;
        }
        if (!on || isGroup_ || !backend_ || conversationId_.trimmed().isEmpty()) {
            if (!isGroup_ && backend_ && !conversationId_.trimmed().isEmpty()) {
                QString err;
                backend_->sendPresence(conversationId_, false, err);
            }
            setPresenceIndicator(false);
            return;
        }
        presencePingTimer_->setInterval(30000);
        presencePingTimer_->start();
        if (!isVisible() || isMinimized() || !isActiveWindow()) {
            return;
        }
        QString err;
        backend_->sendPresence(conversationId_, true, err);
        setPresenceIndicator(true);
    });
    stealthAction_ = sendMenu_->addAction(
        UiSettings::Tr(QStringLiteral("对该好友隐身"),
                       QStringLiteral("Stealth for this chat")));
    stealthAction_->setCheckable(true);
    stealthAction_->setChecked(false);
    stealthAction_->setToolTip(
        UiSettings::Tr(QStringLiteral("不发送已读/输入/在线状态"),
                       QStringLiteral("Hide read receipts, typing, and presence")));
    connect(stealthAction_, &QAction::toggled, this, [this](bool on) {
        const QString convId = conversationId_.trimmed();
        if (convId.isEmpty() || isGroup_) {
            if (stealthAction_) {
                const QSignalBlocker blocker(stealthAction_);
                stealthAction_->setChecked(false);
            }
            return;
        }
        if (on) {
            stealthConversations_.insert(convId);
        } else {
            stealthConversations_.remove(convId);
        }
        applyStealthState();
    });
    exportEvidenceAction_ = sendMenu_->addAction(QStringLiteral("导出举报证据包..."));
    connect(exportEvidenceAction_, &QAction::triggered, this, &ChatWindow::exportEvidencePackage);
    sendMenu_->addSeparator();
    membersAction_ = sendMenu_->addAction(QStringLiteral("群成员..."));
    connect(membersAction_, &QAction::triggered, this, &ChatWindow::manageGroupMembers);
    inviteAction_ = sendMenu_->addAction(QStringLiteral("邀请成员..."));
    leaveAction_ = sendMenu_->addAction(QStringLiteral("退出群聊"));
    connect(inviteAction_, &QAction::triggered, this, &ChatWindow::inviteMember);
    connect(leaveAction_, &QAction::triggered, this, &ChatWindow::leaveGroup);
    membersAction_->setEnabled(false);
    inviteAction_->setEnabled(false);
    leaveAction_->setEnabled(false);
    if (sendLocationAction_) {
        sendLocationAction_->setEnabled(false);
    }
    if (sendCardAction_) {
        sendCardAction_->setEnabled(false);
    }
    if (sendStickerAction_) {
        sendStickerAction_->setEnabled(false);
    }
    if (readReceiptAction_) {
        readReceiptAction_->setEnabled(false);
    }
    if (typingAction_) {
        typingAction_->setEnabled(false);
    }
    if (presenceAction_) {
        presenceAction_->setEnabled(false);
    }
    if (exportEvidenceAction_) {
        exportEvidenceAction_->setEnabled(false);
    }
    sendBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sendBtn, &QWidget::customContextMenuRequested, this, [this, sendBtn]() {
        if (sendMenu_) {
            sendMenu_->exec(sendBtn->mapToGlobal(QPoint(sendBtn->width() / 2, 0)));
        }
    });
    setTabOrder(messageView_, inputEdit_);
    setTabOrder(inputEdit_, sendBtn);

    bodyLayout->addWidget(composer_);
    root->addWidget(body);

    updateEmptyPrompt();
    updateConversationUiState();
    updateEmptyState();
    setCentralWidget(central);
}

void ChatWindow::updateEmptyState() {
    if (!messageStack_ || !messageModel_) {
        return;
    }
    const bool empty = messageModel_->rowCount() == 0;
    const int targetIndex = empty ? 0 : 1;
    const bool changed = messageStack_->currentIndex() != targetIndex;
    messageStack_->setCurrentIndex(targetIndex);
    if (changed) {
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(messageStack_->graphicsEffect());
        if (!effect) {
            effect = new QGraphicsOpacityEffect(messageStack_);
            messageStack_->setGraphicsEffect(effect);
        }
        effect->setOpacity(0.0);
        auto *anim = new QPropertyAnimation(effect, "opacity", messageStack_);
        anim->setDuration(120);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    if (empty) {
        clearNewMessagePill();
    }
    refreshFileTransferAnimation();
}

void ChatWindow::updateEmptyPrompt() {
    if (!emptyTitleLabel_ || !emptySubLabel_) {
        return;
    }

    const bool hasConversation = !conversationId_.trimmed().isEmpty();
    if (hasConversation) {
        emptyTitleLabel_->setText(UiSettings::Tr(QStringLiteral("暂无消息"),
                                                 QStringLiteral("No messages yet")));
        emptySubLabel_->setText(QString());
        emptySubLabel_->setVisible(false);
        return;
    }

    emptyTitleLabel_->setText(
        UiSettings::Tr(QStringLiteral("请选择会话"), QStringLiteral("Select a chat")));
    emptySubLabel_->setText(QString());
    emptySubLabel_->setVisible(false);
}

void ChatWindow::updateConversationUiState() {
    const bool hasConversation = !conversationId_.trimmed().isEmpty();

    if (titleLabel_) {
        titleLabel_->setVisible(hasConversation);
    }
    if (titleIcon_) {
        titleIcon_->setVisible(hasConversation && isGroup_);
    }
    if (presenceLabel_) {
        const bool showPresence =
            hasConversation && !presenceLabel_->text().trimmed().isEmpty();
        presenceLabel_->setVisible(showPresence);
    }
    for (auto *btn : titleActionButtons_) {
        if (btn) {
            btn->setVisible(hasConversation);
        }
    }

    if (composer_) {
        composer_->setVisible(hasConversation);
        composer_->setEnabled(hasConversation);
    }

    if (exportEvidenceAction_) {
        exportEvidenceAction_->setEnabled(hasConversation);
    }

    if (membersAction_) {
        membersAction_->setEnabled(hasConversation && isGroup_);
    }
    if (inviteAction_) {
        inviteAction_->setEnabled(hasConversation && isGroup_);
    }
    if (leaveAction_) {
        leaveAction_->setEnabled(hasConversation && isGroup_);
    }

    if (sendLocationAction_) {
        sendLocationAction_->setEnabled(hasConversation && !isGroup_);
    }
    if (sendCardAction_) {
        sendCardAction_->setEnabled(hasConversation && !isGroup_);
    }
    if (sendStickerAction_) {
        sendStickerAction_->setEnabled(hasConversation && !isGroup_);
    }
    if (readReceiptAction_) {
        readReceiptAction_->setEnabled(hasConversation && !isGroup_);
    }
    if (typingAction_) {
        typingAction_->setEnabled(hasConversation && !isGroup_);
    }
    if (presenceAction_) {
        presenceAction_->setEnabled(hasConversation && !isGroup_);
    }
    applyStealthState();

    if (!hasConversation) {
        setSearchActive(false);
    }
}

void ChatWindow::updateInputHeight() {
    if (!inputEdit_) {
        return;
    }
    const int viewportWidth = qMax(1, inputEdit_->viewport()->width());
    const QMargins margins = inputEdit_->viewportMargins();
    const int textWidth = qMax(1, viewportWidth);
    QTextDocument *doc = inputEdit_->document();
    doc->setTextWidth(textWidth);
    qreal docHeight = doc->size().height();
    if (inputEdit_->toPlainText().isEmpty()) {
        docHeight = QFontMetrics(inputEdit_->font()).lineSpacing();
    }
    const int target =
        qBound(kComposerInputMinHeight,
               static_cast<int>(std::ceil(docHeight)) + margins.top() + margins.bottom(),
               kComposerInputMaxHeight);
    if (inputEdit_->height() != target) {
        inputEdit_->setFixedHeight(target);
        inputEdit_->updateGeometry();
    }
    const bool atMax = (target >= kComposerInputMaxHeight);
    inputEdit_->setVerticalScrollBarPolicy(atMax ? Qt::ScrollBarAsNeeded
                                                  : Qt::ScrollBarAlwaysOff);
}

bool ChatWindow::ensureConversationSelected() {
    if (!conversationId_.trimmed().isEmpty()) {
        return true;
    }

    Toast::Show(this,
                UiSettings::Tr(QStringLiteral("请先选择一个会话"),
                               QStringLiteral("Please select a chat first.")),
                Toast::Level::Info);
    return false;
}

void ChatWindow::toggleSearchBar() {
    if (!searchBar_ || !searchEdit_) {
        return;
    }
    setSearchActive(!searchBar_->isVisible());
}

void ChatWindow::setSearchActive(bool active) {
    if (!searchBar_ || !searchEdit_) {
        return;
    }

    if (!active) {
        searchBar_->setVisible(false);
        searchEdit_->blockSignals(true);
        searchEdit_->clear();
        searchEdit_->blockSignals(false);
        clearSearchState();
        return;
    }

    if (!ensureConversationSelected()) {
        return;
    }

    searchBar_->setVisible(true);
    searchEdit_->setFocus();
    searchEdit_->selectAll();
    updateSearchResults();
}

void ChatWindow::clearSearchState() {
    searchMatchRows_.clear();
    searchMatchIndex_ = -1;
    if (searchCountLabel_) {
        searchCountLabel_->setText(QString());
    }
    if (searchPrevBtn_) {
        searchPrevBtn_->setEnabled(false);
    }
    if (searchNextBtn_) {
        searchNextBtn_->setEnabled(false);
    }
    if (messageDelegate_) {
        messageDelegate_->setHighlightedRow(-1);
    }
    if (messageView_) {
        messageView_->viewport()->update();
    }
}

void ChatWindow::updateSearchResults() {
    if (!searchEdit_ || !searchCountLabel_ || !messageModel_) {
        return;
    }

    const QString query = searchEdit_->text().trimmed();
    searchMatchRows_.clear();
    searchMatchIndex_ = -1;

    if (query.isEmpty()) {
        searchCountLabel_->setText(QString());
        if (searchPrevBtn_) {
            searchPrevBtn_->setEnabled(false);
        }
        if (searchNextBtn_) {
            searchNextBtn_->setEnabled(false);
        }
        if (messageDelegate_) {
            messageDelegate_->setHighlightedRow(-1);
        }
        if (messageView_) {
            messageView_->viewport()->update();
        }
        return;
    }

    const int rows = messageModel_->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = messageModel_->index(row, 0);
        if (!idx.isValid()) {
            continue;
        }
        const auto type =
            static_cast<MessageItem::Type>(idx.data(MessageModel::TypeRole).toInt());
        if (type == MessageItem::Type::TimeDivider) {
            continue;
        }

        QString haystack;
        if (type == MessageItem::Type::System) {
            haystack = idx.data(MessageModel::SystemTextRole).toString();
        } else {
            haystack = idx.data(MessageModel::TextRole).toString();
            if (idx.data(MessageModel::IsStickerRole).toBool()) {
                const QString stickerId = idx.data(MessageModel::StickerIdRole).toString().trimmed();
                if (!stickerId.isEmpty()) {
                    haystack += QStringLiteral(" ") + stickerId;
                }
            }
        }

        if (haystack.contains(query, Qt::CaseInsensitive)) {
            searchMatchRows_.append(row);
        }
    }

    if (searchMatchRows_.isEmpty()) {
        searchCountLabel_->setText(UiSettings::Tr(QStringLiteral("无匹配"), QStringLiteral("No results")));
        if (searchPrevBtn_) {
            searchPrevBtn_->setEnabled(false);
        }
        if (searchNextBtn_) {
            searchNextBtn_->setEnabled(false);
        }
        if (messageDelegate_) {
            messageDelegate_->setHighlightedRow(-1);
        }
        if (messageView_) {
            messageView_->viewport()->update();
        }
        return;
    }

    if (searchPrevBtn_) {
        searchPrevBtn_->setEnabled(true);
    }
    if (searchNextBtn_) {
        searchNextBtn_->setEnabled(true);
    }

    goToSearchResult(0);
}

void ChatWindow::goToSearchResult(int index) {
    if (!messageModel_ || !messageView_ || searchMatchRows_.isEmpty()) {
        return;
    }

    const int total = searchMatchRows_.size();
    const int wrapped = ((index % total) + total) % total;
    searchMatchIndex_ = wrapped;

    const int row = searchMatchRows_.at(searchMatchIndex_);
    if (messageDelegate_) {
        messageDelegate_->setHighlightedRow(row);
    }
    const QModelIndex idx = messageModel_->index(row, 0);
    if (idx.isValid()) {
        messageView_->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
    if (searchCountLabel_) {
        searchCountLabel_->setText(QStringLiteral("%1/%2").arg(searchMatchIndex_ + 1).arg(total));
    }
    messageView_->viewport()->update();
}

void ChatWindow::stepSearchResult(int delta) {
    if (searchMatchRows_.isEmpty()) {
        return;
    }
    if (searchMatchIndex_ < 0) {
        goToSearchResult(0);
        return;
    }
    goToSearchResult(searchMatchIndex_ + delta);
}

bool ChatWindow::isNearBottom() const {
    if (!messageView_) {
        return true;
    }
    auto *sb = messageView_->verticalScrollBar();
    if (!sb) {
        return true;
    }
    const int threshold = 20;
    const int maxValue = sb->maximum();
    return sb->value() >= maxValue - qMin(threshold, maxValue);
}

void ChatWindow::clearNewMessagePill() {
    pendingNewMessages_ = 0;
    if (newMessagePill_) {
        newMessagePill_->setVisible(false);
    }
}

void ChatWindow::bumpNewMessagePill(int count) {
    if (count <= 0) {
        return;
    }
    pendingNewMessages_ = qMin(999, pendingNewMessages_ + count);
    if (!newMessagePill_) {
        return;
    }
    const int n = pendingNewMessages_;
    const QString zh = QStringLiteral("%1 条新消息 ↓").arg(n);
    const QString en = (n == 1) ? QStringLiteral("1 new message ↓")
                                : QStringLiteral("%1 new messages ↓").arg(n);
    newMessagePill_->setText(UiSettings::Tr(zh, en));
    newMessagePill_->adjustSize();
    updateNewMessagePillGeometry();
    newMessagePill_->setVisible(true);
    newMessagePill_->raise();
}

void ChatWindow::updateNewMessagePillGeometry() {
    if (!newMessagePill_ || !messageView_) {
        return;
    }
    QWidget *vp = messageView_->viewport();
    if (!vp) {
        return;
    }
    const int margin = 12;
    const QSize s = newMessagePill_->sizeHint();
    newMessagePill_->resize(s);
    const int x = vp->width() - s.width() - margin;
    const int y = vp->height() - s.height() - margin;
    newMessagePill_->move(qMax(margin, x), qMax(margin, y));
}

void ChatWindow::refreshFileTransferAnimation() {
    if (!fileTransferAnimTimer_ || !messageModel_) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool highlightActive = lastMessageInsertMs_ > 0 && (nowMs - lastMessageInsertMs_) < 260;
    const bool active = messageModel_->hasActiveFileTransfers() || highlightActive;
    if (active && !fileTransferAnimTimer_->isActive()) {
        fileTransferAnimTimer_->start();
    } else if (!active && fileTransferAnimTimer_->isActive()) {
        fileTransferAnimTimer_->stop();
        lastMessageInsertMs_ = 0;
    }
}

void ChatWindow::setConversation(const QString &id, const QString &title, bool isGroup) {
    const QString previousConvId = conversationId_;
    conversationId_ = id;
    isGroup_ = isGroup;
    if (titleIcon_) {
        if (isGroup_) {
            titleIcon_->setPixmap(UiIcons::TintedSvg(
                QStringLiteral(":/mi/e2ee/ui/icons/group.svg"), 14, ChatTokens::textMuted(),
                titleIcon_->devicePixelRatioF()));
        } else {
            titleIcon_->setPixmap(QPixmap());
        }
    }
    titleLabel_->setText(title);
    updateOverlayForTitle(title);
    setSearchActive(false);
    clearReplyContext();
    clearNewMessagePill();
    if (inputEdit_ && previousConvId != conversationId_) {
        inputEdit_->clear();
        updateInputHeight();
    }
    lastMessageInsertMs_ = 0;
    refreshFileTransferAnimation();
    readReceiptSent_.clear();
    typingSent_ = false;
    lastTypingSentMs_ = 0;
    if (typingStopSendTimer_) {
        typingStopSendTimer_->stop();
    }
    if (typingHideTimer_) {
        typingHideTimer_->stop();
    }
    if (typingLabel_) {
        typingLabel_->setVisible(false);
    }
    if (presencePingTimer_) {
        presencePingTimer_->stop();
    }
    if (presenceHideTimer_) {
        presenceHideTimer_->stop();
    }
    if (presenceLabel_) {
        presenceLabel_->setText(QString());
        presenceLabel_->setVisible(false);
    }

    updateEmptyPrompt();
    updateConversationUiState();
    messageModel_->setConversation(id);
    if (backend_ && !conversationId_.trimmed().isEmpty()) {
        QVector<BackendAdapter::HistoryMessageEntry> entries;
        QString histErr;
        if (backend_->loadChatHistory(conversationId_, isGroup_, 200, entries, histErr)) {
            auto toStatus = [](int st) {
                switch (st) {
                case 2:
                    return MessageItem::Status::Read;
                case 1:
                    return MessageItem::Status::Delivered;
                case 3:
                    return MessageItem::Status::Failed;
                case 0:
                default:
                    return MessageItem::Status::Sent;
                }
            };
            for (const auto &h : entries) {
                const QDateTime t = h.timestampSec > 0
                                        ? QDateTime::fromSecsSinceEpoch(static_cast<qint64>(h.timestampSec))
                                        : QDateTime::currentDateTime();
                auto st = toStatus(h.status);
                if (h.status == 3 && backend_ && backend_->isPendingOutgoingMessage(h.messageId)) {
                    st = MessageItem::Status::Pending;
                }
                const QString sender = (!h.outgoing && isGroup_) ? h.sender : QString();
                if (h.kind == 4) {
                    messageModel_->appendSystemMessage(conversationId_, h.text, t);
                } else if (h.kind == 2) {
                    const QString name = !h.fileName.trimmed().isEmpty() ? h.fileName : h.text;
                    messageModel_->appendFileMessage(conversationId_, h.outgoing, name, h.fileSize, QString(), t,
                                                     h.messageId, st, sender);
                } else if (h.kind == 3) {
                    messageModel_->appendStickerMessage(conversationId_, h.outgoing, h.stickerId, t, h.messageId, st,
                                                        sender);
                } else {
                    messageModel_->appendTextMessage(conversationId_, h.outgoing, h.text, t, h.messageId, st, sender);
                }
            }
            clearNewMessagePill();
            messageView_->scrollToBottom();
        }
    }
    if (!isGroup_ && presenceAction_ && presenceAction_->isChecked() && presencePingTimer_) {
        presencePingTimer_->setInterval(30000);
        presencePingTimer_->start();
    }
}

void ChatWindow::setReplyContext(const QString &messageId, const QString &preview) {
    replyToMessageId_ = messageId.trimmed();
    replyPreview_ = preview.trimmed();
    if (replyPreview_.size() > 80) {
        replyPreview_ = replyPreview_.left(80) + QStringLiteral("…");
    }
    if (replyLabel_) {
        const QString shown = replyPreview_.isEmpty() ? QStringLiteral("（引用）") : replyPreview_;
        replyLabel_->setText(QStringLiteral("回复：%1").arg(shown));
    }
    if (replyBar_) {
        replyBar_->setVisible(!replyToMessageId_.isEmpty());
    }
}

void ChatWindow::clearReplyContext() {
    replyToMessageId_.clear();
    replyPreview_.clear();
    if (replyLabel_) {
        replyLabel_->setText(QStringLiteral(""));
    }
    if (replyBar_) {
        replyBar_->setVisible(false);
    }
}

void ChatWindow::activateMessage(const QModelIndex &index) {
    if (!backend_ || !messageModel_ || !index.isValid()) {
        return;
    }
    const auto type = static_cast<MessageItem::Type>(
        index.data(MessageModel::TypeRole).toInt());
    if (type != MessageItem::Type::Text) {
        return;
    }
    const bool isFile = index.data(MessageModel::IsFileRole).toBool();
    if (!isFile) {
        return;
    }

    const bool outgoing = index.data(MessageModel::OutgoingRole).toBool();
    const auto status =
        static_cast<MessageItem::Status>(index.data(MessageModel::StatusRole).toInt());
    const QString messageId = index.data(MessageModel::MessageIdRole).toString().trimmed();
    const QString text = index.data(MessageModel::TextRole).toString();
    const QString filePath = index.data(MessageModel::FilePathRole).toString().trimmed();
    const qint64 fileSize = index.data(MessageModel::FileSizeRole).toLongLong();

    const QString nameOrPath = filePath.isEmpty() ? text : filePath;
    const bool looksImage = LooksLikeImageFile(nameOrPath);
    const bool looksAudio = LooksLikeAudioFile(nameOrPath);
    const bool looksVideo = LooksLikeVideoFile(nameOrPath);

    if (outgoing && status == MessageItem::Status::Failed && !messageId.isEmpty()) {
        if (filePath.isEmpty()) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("缺少本地文件路径，无法重试"),
                                       QStringLiteral("Missing local file path; can't retry.")),
                        Toast::Level::Warning);
            return;
        }
        QString err;
        const bool ok = isGroup_ ? backend_->resendGroupFile(conversationId_, messageId, filePath, err)
                                 : backend_->resendFile(conversationId_, messageId, filePath, err);
        messageModel_->updateMessageStatus(messageId, ok ? MessageItem::Status::Sent : MessageItem::Status::Failed);
        if (ok) {
            setFileTransferState(messageId, FileTransferState::Uploading);
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("开始重试发送…"),
                                       QStringLiteral("Retrying…")),
                        Toast::Level::Info);
        } else {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("重试失败：%1").arg(err),
                                       QStringLiteral("Retry failed: %1").arg(err)),
                        Toast::Level::Error);
        }
        return;
    }

    if (looksImage && !messageId.isEmpty()) {
        if (!filePath.isEmpty()) {
            QImage img(filePath);
            if (img.isNull()) {
                Toast::Show(this,
                            UiSettings::Tr(QStringLiteral("图片解码失败"),
                                           QStringLiteral("Failed to decode image.")),
                            Toast::Level::Error);
                return;
            }
            ShowImageDialog(this, img, text);
            return;
        }

        const qint64 maxPreviewBytes = 25 * 1024 * 1024;
        if (fileSize > 0 && fileSize > maxPreviewBytes) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("图片过大，无法预览；请先保存。"),
                                       QStringLiteral("Image too large to preview; please save first.")),
                        Toast::Level::Warning);
            return;
        }
        QByteArray bytes;
        QString err;
        if (!backend_->loadReceivedFileBytes(conversationId_, messageId, bytes, maxPreviewBytes, false, err)) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("预览失败：%1").arg(err),
                                       QStringLiteral("Preview failed: %1").arg(err)),
                        Toast::Level::Error);
            return;
        }
        QImage img;
        if (!img.loadFromData(bytes)) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("图片解码失败"),
                                       QStringLiteral("Failed to decode image.")),
                        Toast::Level::Error);
            return;
        }
        ShowImageDialog(this, img, text);
        return;
    }

    if ((looksAudio || looksVideo) && !messageId.isEmpty()) {
        if (!filePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            return;
        }
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("请先保存该文件再打开。"),
                                   QStringLiteral("Please save the file before opening it.")),
                    Toast::Level::Info);
        return;
    }

    if (!filePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }

    if (!outgoing && !messageId.isEmpty()) {
        const QString outPath = QFileDialog::getSaveFileName(
            this,
            UiSettings::Tr(QStringLiteral("保存文件"),
                           QStringLiteral("Save file")),
            text);
        if (outPath.isEmpty()) {
            return;
        }
        QString err;
        if (!backend_->saveReceivedFile(conversationId_, messageId, outPath, err)) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("保存失败：%1").arg(err),
                                       QStringLiteral("Save failed: %1").arg(err)),
                        Toast::Level::Error);
            setFileTransferState(messageId, FileTransferState::None);
            return;
        }
        setFileTransferState(messageId, FileTransferState::Downloading);
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("开始保存…"),
                                   QStringLiteral("Saving…")),
                    Toast::Level::Info);
        return;
    }

    Toast::Show(this,
                UiSettings::Tr(QStringLiteral("缺少本地文件路径"),
                               QStringLiteral("Missing local file path.")),
                Toast::Level::Warning);
}

void ChatWindow::appendIncomingMessage(const QString &sender, const QString &messageId, const QString &text,
                                       bool isFile, qint64 fileSize, const QDateTime &time) {
    if (isFile) {
        messageModel_->appendFileMessage(conversationId_, false, text, fileSize, QString(), time, messageId,
                                         MessageItem::Status::Sent, sender);
    } else {
        messageModel_->appendTextMessage(conversationId_, false, text, time, messageId,
                                         MessageItem::Status::Sent, sender);
    }
    if (!isGroup_ && !isStealthActive() && readReceiptAction_ && readReceiptAction_->isChecked() &&
        backend_ && !messageId.trimmed().isEmpty()) {
        if (!readReceiptSent_.contains(messageId)) {
            readReceiptSent_.insert(messageId);
            QString ignore;
            backend_->sendReadReceipt(conversationId_, messageId, ignore);
        }
    }
}

void ChatWindow::appendIncomingSticker(const QString &sender, const QString &messageId,
                                      const QString &stickerId, const QDateTime &time) {
    messageModel_->appendStickerMessage(conversationId_, false, stickerId, time, messageId,
                                        MessageItem::Status::Sent, sender);
    if (!isGroup_ && !isStealthActive() && readReceiptAction_ && readReceiptAction_->isChecked() &&
        backend_ && !messageId.trimmed().isEmpty()) {
        if (!readReceiptSent_.contains(messageId)) {
            readReceiptSent_.insert(messageId);
            QString ignore;
            backend_->sendReadReceipt(conversationId_, messageId, ignore);
        }
    }
}

bool ChatWindow::isStealthActive() const {
    const QString convId = conversationId_.trimmed();
    return !convId.isEmpty() && !isGroup_ && stealthConversations_.contains(convId);
}

void ChatWindow::applyStealthState() {
    const QString convId = conversationId_.trimmed();
    const bool hasConversation = !convId.isEmpty();
    const bool allowStealth = hasConversation && !isGroup_;
    const bool stealth = allowStealth && stealthConversations_.contains(convId);

    if (stealthAction_) {
        const QSignalBlocker blocker(stealthAction_);
        stealthAction_->setEnabled(allowStealth);
        stealthAction_->setChecked(stealth);
    }
    if (readReceiptAction_) {
        readReceiptAction_->setEnabled(allowStealth && !stealth);
    }
    if (typingAction_) {
        typingAction_->setEnabled(allowStealth && !stealth);
    }
    if (presenceAction_) {
        presenceAction_->setEnabled(allowStealth && !stealth);
    }
    if (!stealth) {
        return;
    }

    if (typingStopSendTimer_) {
        typingStopSendTimer_->stop();
    }
    if (typingSent_ && backend_) {
        QString err;
        backend_->sendTyping(convId, false, err);
        typingSent_ = false;
    }
    if (presencePingTimer_) {
        presencePingTimer_->stop();
    }
    if (backend_) {
        QString err;
        backend_->sendPresence(convId, false, err);
    }
}

void ChatWindow::appendSyncedOutgoingMessage(const QString &messageId, const QString &text,
                                             bool isFile, qint64 fileSize, const QDateTime &time) {
    if (isFile) {
        messageModel_->appendFileMessage(conversationId_, true, text, fileSize, QString(), time, messageId,
                                         MessageItem::Status::Sent);
    } else {
        messageModel_->appendTextMessage(conversationId_, true, text, time, messageId,
                                         MessageItem::Status::Sent);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::appendSyncedOutgoingSticker(const QString &messageId, const QString &stickerId,
                                             const QDateTime &time) {
    messageModel_->appendStickerMessage(conversationId_, true, stickerId, time, messageId,
                                        MessageItem::Status::Sent);
    messageView_->scrollToBottom();
}

void ChatWindow::appendSystemMessage(const QString &text, const QDateTime &time) {
    if (!messageModel_) {
        return;
    }
    messageModel_->appendSystemMessage(conversationId_, text, time);
}

void ChatWindow::markDelivered(const QString &messageId) {
    if (!messageModel_) {
        return;
    }
    if (messageModel_->updateMessageStatus(messageId, MessageItem::Status::Delivered)) {
        messageView_->viewport()->update();
    }
}

void ChatWindow::markRead(const QString &messageId) {
    if (!messageModel_) {
        return;
    }
    if (messageModel_->updateMessageStatus(messageId, MessageItem::Status::Read)) {
        messageView_->viewport()->update();
    }
}

void ChatWindow::markSent(const QString &messageId) {
    if (!messageModel_) {
        return;
    }
    if (messageModel_->updateMessageStatus(messageId, MessageItem::Status::Sent)) {
        messageView_->viewport()->update();
    }
}

void ChatWindow::markFailed(const QString &messageId) {
    if (!messageModel_) {
        return;
    }
    if (messageModel_->updateMessageStatus(messageId, MessageItem::Status::Failed)) {
        messageView_->viewport()->update();
    }
}

void ChatWindow::setFileTransferState(const QString &messageId, FileTransferState state, int progress) {
    if (!messageModel_) {
        return;
    }
    const auto transfer = static_cast<MessageItem::FileTransfer>(static_cast<int>(state));
    if (messageModel_->updateFileTransfer(messageId, transfer, progress)) {
        refreshFileTransferAnimation();
        if (messageView_) {
            messageView_->viewport()->update();
        }
    }
}

void ChatWindow::setFileLocalPath(const QString &messageId, const QString &filePath) {
    if (!messageModel_) {
        return;
    }
    if (messageModel_->updateFilePath(messageId, filePath)) {
        if (messageView_) {
            messageView_->viewport()->update();
        }
    }
}

void ChatWindow::setTypingIndicator(bool typing) {
    if (isGroup_ || !typingLabel_) {
        return;
    }
    if (typing) {
        typingLabel_->setVisible(true);
        if (typingHideTimer_) {
            typingHideTimer_->start(4500);
        }
        return;
    }
    if (typingHideTimer_) {
        typingHideTimer_->stop();
    }
    typingLabel_->setVisible(false);
}

void ChatWindow::setPresenceIndicator(bool online) {
    if (isGroup_ || !presenceLabel_) {
        return;
    }
    if (online) {
        presenceLabel_->setText(UiSettings::Tr(QStringLiteral("在线"),
                                               QStringLiteral("Online")));
        presenceLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                          .arg(Theme::accentGreen().name()));
        presenceLabel_->setVisible(true);
        if (presenceHideTimer_) {
            presenceHideTimer_->start(75000);
        }
        return;
    }
    if (presenceHideTimer_) {
        presenceHideTimer_->stop();
    }
    presenceLabel_->setText(UiSettings::Tr(QStringLiteral("离线"),
                                           QStringLiteral("Offline")));
    presenceLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                      .arg(ChatTokens::textMuted().name()));
    presenceLabel_->setVisible(true);
}

void ChatWindow::setPresenceEnabled(bool enabled) {
    if (!presenceAction_ || isGroup_) {
        return;
    }
    if (presenceAction_->isChecked() == enabled) {
        return;
    }
    presenceAction_->setChecked(enabled);
}

bool ChatWindow::eventFilter(QObject *obj, QEvent *event) {
    if (messageView_ && obj == messageView_->viewport() && event->type() == QEvent::Resize) {
        updateNewMessagePillGeometry();
    }
    if (inputEdit_ && obj == inputEdit_ && event->type() == QEvent::Resize) {
        updateInputHeight();
    }
    if (obj == searchEdit_ && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            setSearchActive(false);
            return true;
        }
        if (ke->key() == Qt::Key_Up) {
            stepSearchResult(-1);
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            stepSearchResult(1);
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers().testFlag(Qt::ShiftModifier)) {
                stepSearchResult(-1);
            } else {
                stepSearchResult(1);
            }
            return true;
        }
    }
    if (obj == inputEdit_ && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers().testFlag(Qt::ControlModifier) && ke->key() == Qt::Key_F) {
            setSearchActive(true);
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (auto *edit = qobject_cast<ChatInputEdit *>(inputEdit_)) {
                if (edit->isComposing() || edit->isNativeComposing()) {
                    return false;
                }
            }
            if (ke->modifiers().testFlag(Qt::ShiftModifier)) {
                // New line with Shift+Enter
                return false;
            }
            sendMessage();
            return true;
        }
    }
    return FramelessWindowBase::eventFilter(obj, event);
}

void ChatWindow::sendMessage() {
    if (!ensureConversationSelected()) {
        return;
    }
    if (!inputEdit_) {
        return;
    }
    if (auto *edit = qobject_cast<ChatInputEdit *>(inputEdit_)) {
        if (edit->isComposing()) {
            edit->commitDefaultCandidate();
        }
        if (edit->isNativeComposing()) {
            return;
        }
    }
    const QString text = inputEdit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    bool ok = false;
    if (backend_) {
        if (isGroup_) {
            ok = backend_->sendGroupText(conversationId_, text, messageId, err);
        } else if (!replyToMessageId_.trimmed().isEmpty()) {
            ok = backend_->sendTextWithReply(conversationId_, text,
                                             replyToMessageId_.trimmed(),
                                             replyPreview_.trimmed(),
                                             messageId, err);
        } else {
            ok = backend_->sendText(conversationId_, text, messageId, err);
        }
    }

    const QString errTrimmed = err.trimmed();
    if (!ok) {
        if (!isGroup_ && backend_ && IsNotFriendsError(errTrimmed)) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(UiSettings::Tr(QStringLiteral("无法发送"),
                                              QStringLiteral("Cannot send")));
            box.setText(UiSettings::Tr(QStringLiteral("你和 %1 不是好友，无法发送消息。")
                                           .arg(conversationId_),
                                       QStringLiteral("You are not friends with %1; message can't be sent.")
                                           .arg(conversationId_)));
            auto *sendReqBtn = box.addButton(UiSettings::Tr(QStringLiteral("发送好友申请"),
                                                           QStringLiteral("Send friend request")),
                                             QMessageBox::AcceptRole);
            box.addButton(UiSettings::Tr(QStringLiteral("取消"), QStringLiteral("Cancel")),
                          QMessageBox::RejectRole);
            box.exec();
            if (box.clickedButton() == sendReqBtn) {
                bool okInput = false;
                const QString remark = QInputDialog::getText(
                    this,
                    UiSettings::Tr(QStringLiteral("好友申请"), QStringLiteral("Friend Request")),
                    UiSettings::Tr(QStringLiteral("备注（可选）"), QStringLiteral("Remark (optional)")),
                    QLineEdit::Normal,
                    QString(),
                    &okInput);
                if (!okInput) {
                    return;
                }
                QString reqErr;
                if (backend_->sendFriendRequest(conversationId_, remark.trimmed(), reqErr)) {
                    Toast::Show(this,
                                UiSettings::Tr(QStringLiteral("好友申请已发送"),
                                               QStringLiteral("Friend request sent")),
                                Toast::Level::Success);
                } else {
                    Toast::Show(this,
                                reqErr.trimmed().isEmpty()
                                    ? UiSettings::Tr(QStringLiteral("发送好友申请失败"),
                                                     QStringLiteral("Failed to send friend request"))
                                    : reqErr.trimmed(),
                                Toast::Level::Error);
                }
            }
            return;
        }

        if (IsNonRetryableSendError(errTrimmed) || messageId.trimmed().isEmpty()) {
            const QString shown =
                errTrimmed.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("发送失败"),
                                     QStringLiteral("Send failed"))
                    : UiSettings::Tr(QStringLiteral("发送失败：%1").arg(errTrimmed),
                                     QStringLiteral("Send failed: %1").arg(errTrimmed));
            Toast::Show(this, shown, Toast::Level::Error, 3200);
            return;
        }
    }

    inputEdit_->clear();
    updateInputHeight();

    const auto status = ok ? MessageItem::Status::Sent
                           : MessageItem::Status::Pending;
    QString displayText = text;
    if (!isGroup_ && !replyToMessageId_.trimmed().isEmpty()) {
        const QString preview = replyPreview_.trimmed().isEmpty()
                                    ? QStringLiteral("（引用）")
                                    : replyPreview_.trimmed();
        displayText = QStringLiteral("【回复】%1\n%2").arg(preview, text);
    }
    messageModel_->appendTextMessage(conversationId_, true, displayText, now, messageId, status);
    messageView_->scrollToBottom();
    clearReplyContext();

    if (ok && !errTrimmed.isEmpty()) {
        Toast::Show(this, errTrimmed, Toast::Level::Info);
    } else if (!ok && !errTrimmed.isEmpty()) {
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("已加入发送队列：%1").arg(errTrimmed),
                                   QStringLiteral("Queued to retry: %1").arg(errTrimmed)),
                    Toast::Level::Warning,
                    2600);
    }
}

void ChatWindow::showEmojiPicker() {
    if (!emojiBtn_ || !inputEdit_) {
        return;
    }
    if (!emojiPicker_) {
        emojiPicker_ = new EmojiPickerDialog(this);
        connect(emojiPicker_, &EmojiPickerDialog::emojiSelected, this, [this](const QString &emoji) {
            if (!inputEdit_ || emoji.isEmpty()) {
                return;
            }
            inputEdit_->insertPlainText(emoji);
            inputEdit_->setFocus(Qt::OtherFocusReason);
        });
    }
    if (emojiPicker_->isVisible()) {
        emojiPicker_->hide();
        return;
    }

    const QPoint anchor = emojiBtn_->mapToGlobal(QPoint(0, emojiBtn_->height()));
    const QSize popupSize = emojiPicker_->size();
    QPoint pos(anchor.x(), anchor.y() - popupSize.height());
    if (QScreen *screen = QGuiApplication::screenAt(anchor)) {
        const QRect bounds = screen->availableGeometry();
        if (pos.x() + popupSize.width() > bounds.right()) {
            pos.setX(bounds.right() - popupSize.width());
        }
        if (pos.x() < bounds.left()) {
            pos.setX(bounds.left());
        }
        if (pos.y() < bounds.top()) {
            pos.setY(anchor.y());
        }
        if (pos.y() + popupSize.height() > bounds.bottom()) {
            pos.setY(bounds.bottom() - popupSize.height());
        }
    }
    emojiPicker_->move(pos);
    emojiPicker_->show();
    emojiPicker_->raise();
    emojiPicker_->activateWindow();
}

void ChatWindow::sendStickerPlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    if (isGroup_) {
        messageModel_->appendSystemMessage(
            conversationId_,
            UiSettings::Tr(QStringLiteral("群聊暂不支持贴纸"),
                           QStringLiteral("Stickers are not supported in group chats yet.")),
            QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }
    if (!backend_) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(UiSettings::Tr(QStringLiteral("发送贴纸"), QStringLiteral("Send Sticker")));
    dlg.setModal(true);
    dlg.setStyleSheet(QStringLiteral("QDialog { background: %1; color: %2; }")
                          .arg(Theme::uiWindowBg().name(),
                               Theme::uiTextMain().name()));

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    root->addLayout(grid);

    QString picked;
    const QStringList stickers = BuiltinStickers();
    const int perRow = 4;
    const int iconSize = 64;
    for (int i = 0; i < stickers.size(); ++i) {
        const QString sid = stickers[i];
        auto *btn = new QToolButton(&dlg);
        btn->setIcon(QIcon(StickerIcon(sid, iconSize)));
        btn->setIconSize(QSize(iconSize, iconSize));
        btn->setToolTip(sid);
        btn->setAutoRaise(true);
        btn->setStyleSheet(
            QStringLiteral(
                "QToolButton { background: %1; border: 1px solid %2; border-radius: 10px; padding: 6px; }"
                "QToolButton:hover { background: %3; }"
                "QToolButton:pressed { background: %4; }")
                .arg(Theme::uiPanelBg().name(),
                     Theme::uiBorder().name(),
                     Theme::uiHoverBg().name(),
                     Theme::uiSelectedBg().name()));
        connect(btn, &QToolButton::clicked, &dlg, [&dlg, &picked, sid]() {
            picked = sid;
            dlg.accept();
        });
        grid->addWidget(btn, i / perRow, i % perRow);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    buttons->setStyleSheet(
        QStringLiteral(
            "QDialogButtonBox QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 10px; padding: 8px 16px; }"
            "QDialogButtonBox QPushButton:hover { background: %4; }"
            "QDialogButtonBox QPushButton:pressed { background: %5; }")
            .arg(Theme::uiPanelBg().name(),
                 Theme::uiTextMain().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name(),
                 Theme::uiSelectedBg().name()));
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted || picked.trimmed().isEmpty()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    const bool ok = backend_->sendSticker(conversationId_, picked, messageId, err);
    const auto status = ok ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendStickerMessage(conversationId_, true, picked, now, messageId, status);
    messageView_->scrollToBottom();

    if (!err.isEmpty()) {
        const QString prefix =
            ok ? UiSettings::Tr(QStringLiteral("提示"), QStringLiteral("Info"))
               : UiSettings::Tr(QStringLiteral("发送贴纸失败"),
                                QStringLiteral("Failed to send sticker"));
        messageModel_->appendSystemMessage(conversationId_,
                                           QStringLiteral("%1：%2").arg(prefix, err),
                                           now);
        messageView_->scrollToBottom();
    }
}

void ChatWindow::sendLocationPlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    if (isGroup_) {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("群聊暂不支持位置消息"), QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }
    if (!backend_) {
        return;
    }

    bool ok = false;
    const QString label =
        QInputDialog::getText(this, QStringLiteral("发送位置"),
                              QStringLiteral("位置名称（可留空）"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    const QString latStr =
        QInputDialog::getText(this, QStringLiteral("发送位置"),
                              QStringLiteral("纬度（-90 ~ 90）"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    const QString lonStr =
        QInputDialog::getText(this, QStringLiteral("发送位置"),
                              QStringLiteral("经度（-180 ~ 180）"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    bool okLat = false;
    bool okLon = false;
    const double lat = latStr.trimmed().toDouble(&okLat);
    const double lon = lonStr.trimmed().toDouble(&okLon);
    if (!okLat || !okLon) {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("坐标格式无效"), QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("坐标范围无效"), QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }

    const qint64 latE7_64 = std::llround(lat * 10000000.0);
    const qint64 lonE7_64 = std::llround(lon * 10000000.0);
    const qint32 latE7 = static_cast<qint32>(latE7_64);
    const qint32 lonE7 = static_cast<qint32>(lonE7_64);

    const auto formatCoordE7 = [](qint32 vE7) -> QString {
        const qint64 v64 = static_cast<qint64>(vE7);
        const bool neg = v64 < 0;
        const qint64 abs = neg ? -v64 : v64;
        const qint64 deg = abs / 10000000;
        const qint64 frac = abs % 10000000;
        return QStringLiteral("%1%2.%3")
            .arg(neg ? QStringLiteral("-") : QStringLiteral(""))
            .arg(deg)
            .arg(frac, 7, 10, QLatin1Char('0'));
    };

    const QString shownLabel = label.trimmed().isEmpty() ? QStringLiteral("（未命名）") : label.trimmed();
    const QString displayText =
        QStringLiteral("【位置】%1\nlat:%2, lon:%3").arg(shownLabel, formatCoordE7(latE7), formatCoordE7(lonE7));

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }
    const bool sent = backend_->sendLocation(conversationId_, latE7, lonE7, shownLabel, messageId, err);
    const auto status = sent ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendTextMessage(conversationId_, true, displayText, now, messageId, status);
    messageView_->scrollToBottom();

    if (!err.isEmpty()) {
        const QString prefix = sent ? QStringLiteral("提示") : QStringLiteral("发送失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
        messageView_->scrollToBottom();
    }
}

void ChatWindow::sendContactCardPlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    if (isGroup_) {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("群聊暂不支持名片消息"), QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }
    if (!backend_) {
        return;
    }

    bool ok = false;
    const QString cardUsername =
        QInputDialog::getText(this, QStringLiteral("发送名片"),
                              QStringLiteral("名片账号"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok || cardUsername.trimmed().isEmpty()) {
        return;
    }
    const QString cardDisplay =
        QInputDialog::getText(this, QStringLiteral("发送名片"),
                              QStringLiteral("名片备注（可留空）"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    const QString shownUser = cardUsername.trimmed();
    const QString shownDisplay = cardDisplay.trimmed();
    QString displayText = QStringLiteral("【名片】%1").arg(shownUser);
    if (!shownDisplay.isEmpty()) {
        displayText += QStringLiteral(" (%1)").arg(shownDisplay);
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }
    const bool sent = backend_->sendContactCard(conversationId_, shownUser, shownDisplay, messageId, err);
    const auto status = sent ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendTextMessage(conversationId_, true, displayText, now, messageId, status);
    messageView_->scrollToBottom();

    if (!err.isEmpty()) {
        const QString prefix = sent ? QStringLiteral("提示") : QStringLiteral("发送失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
        messageView_->scrollToBottom();
    }
}

void ChatWindow::exportEvidencePackage() {
    if (!messageModel_ || conversationId_.trimmed().isEmpty()) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(UiSettings::Tr(QStringLiteral("导出举报证据包"),
                                      QStringLiteral("Export evidence package")));
    dlg.setModal(true);
    dlg.setStyleSheet(QStringLiteral(
                          "QDialog { background: %1; color: %2; }"
                          "QLabel { color: %2; }"
                          "QSpinBox { background: %3; border: 1px solid %4; border-radius: 6px; padding: 4px; color: %2; }"
                          "QCheckBox { color: %2; }")
                          .arg(Theme::uiWindowBg().name(),
                               Theme::uiTextMain().name(),
                               Theme::uiPanelBg().name(),
                               Theme::uiBorder().name()));

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto *tip = new QLabel(&dlg);
    tip->setTextFormat(Qt::PlainText);
    tip->setWordWrap(true);
    tip->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextSub().name()));
    tip->setText(UiSettings::Tr(
        QStringLiteral(
            "将导出本地聊天记录的“证据包”到文件。\n"
            "默认不包含消息内容，以减少隐私泄露风险。\n\n"
            "注意：勾选“包含消息内容”后，导出的文件将包含明文消息（以及可能的文件名/贴纸 ID）。请自行保管。"),
        QStringLiteral(
            "Exports a local evidence package to a file.\n"
            "By default it excludes message contents to reduce privacy exposure.\n\n"
            "Warning: enabling “Include message contents” will export plaintext messages (and possibly filenames/sticker IDs). Keep it safe.")));
    root->addWidget(tip);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    auto *countSpin = new QSpinBox(&dlg);
    countSpin->setRange(1, 1000);
    countSpin->setValue(50);
    form->addRow(UiSettings::Tr(QStringLiteral("导出最近消息条数"),
                                QStringLiteral("Recent messages to export")),
                 countSpin);
    root->addLayout(form);

    auto *includeContentBox = new QCheckBox(
        UiSettings::Tr(QStringLiteral("包含消息内容（明文，可能泄露隐私）"),
                       QStringLiteral("Include message contents (plaintext, may leak privacy)")),
        &dlg);
    includeContentBox->setChecked(false);
    root->addWidget(includeContentBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(
        UiSettings::Tr(QStringLiteral("选择保存位置..."),
                       QStringLiteral("Choose save location...")));
    buttons->button(QDialogButtonBox::Cancel)->setText(
        UiSettings::Tr(QStringLiteral("取消"), QStringLiteral("Cancel")));
    buttons->setStyleSheet(
        QStringLiteral(
            "QDialogButtonBox QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 10px; padding: 8px 16px; }"
            "QDialogButtonBox QPushButton:hover { background: %4; }")
            .arg(Theme::uiBorder().name(),
                 Theme::uiTextMain().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name()));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const int limit = countSpin->value();
    const bool includeContent = includeContentBox->isChecked();

    QString safeConv = conversationId_.trimmed();
    safeConv.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9._-]+")), QStringLiteral("_"));
    if (safeConv.isEmpty()) {
        safeConv = QStringLiteral("conv");
    }
    safeConv = safeConv.left(32);
    const QString ts = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString suggested = QStringLiteral("mi_e2ee_report_%1_%2.mireport").arg(safeConv, ts);

    QString path = QFileDialog::getSaveFileName(
        this,
        UiSettings::Tr(QStringLiteral("保存举报证据包"),
                       QStringLiteral("Save evidence package")),
        suggested,
        QStringLiteral("MI Report (*.mireport);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".mireport"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".mireport");
    }

    auto statusToString = [](int s) -> QString {
        switch (static_cast<MessageItem::Status>(s)) {
        case MessageItem::Status::Pending:
            return QStringLiteral("pending");
        case MessageItem::Status::Sent:
            return QStringLiteral("sent");
        case MessageItem::Status::Delivered:
            return QStringLiteral("delivered");
        case MessageItem::Status::Read:
            return QStringLiteral("read");
        case MessageItem::Status::Failed:
            return QStringLiteral("failed");
        }
        return QStringLiteral("unknown");
    };

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("schema_version"), 1);
    rootObj.insert(QStringLiteral("exported_at_utc"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    rootObj.insert(QStringLiteral("conversation_id"), conversationId_);
    rootObj.insert(QStringLiteral("conversation_title"), titleLabel_ ? titleLabel_->text() : QString());
    rootObj.insert(QStringLiteral("is_group"), isGroup_);
    rootObj.insert(QStringLiteral("include_content"), includeContent);
    if (backend_) {
        rootObj.insert(QStringLiteral("local_user"), backend_->currentUser());
        rootObj.insert(QStringLiteral("local_device_id"), backend_->currentDeviceId());
    }

    QJsonArray messages;
    int exported = 0;
    for (int row = messageModel_->rowCount() - 1; row >= 0 && exported < limit; --row) {
        const QModelIndex idx = messageModel_->index(row, 0);
        const auto type = static_cast<MessageItem::Type>(idx.data(MessageModel::TypeRole).toInt());
        if (type != MessageItem::Type::Text) {
            continue;
        }

        const QString messageId = idx.data(MessageModel::MessageIdRole).toString();
        const bool outgoing = idx.data(MessageModel::OutgoingRole).toBool();
        const bool isFile = idx.data(MessageModel::IsFileRole).toBool();
        const bool isSticker = idx.data(MessageModel::IsStickerRole).toBool();
        const auto status = idx.data(MessageModel::StatusRole).toInt();
        const QDateTime t = idx.data(MessageModel::TimeRole).toDateTime();

        QJsonObject m;
        m.insert(QStringLiteral("message_id"), messageId);
        m.insert(QStringLiteral("outgoing"), outgoing);
        m.insert(QStringLiteral("kind"),
                 isSticker ? QStringLiteral("sticker")
                           : (isFile ? QStringLiteral("file") : QStringLiteral("text")));
        m.insert(QStringLiteral("status"), statusToString(status));
        if (t.isValid()) {
            m.insert(QStringLiteral("time_utc"), t.toUTC().toString(Qt::ISODateWithMs));
        }

        if (includeContent) {
            const QString sender = idx.data(MessageModel::SenderRole).toString();
            if (!sender.trimmed().isEmpty()) {
                m.insert(QStringLiteral("sender"), sender);
            }
            if (isSticker) {
                m.insert(QStringLiteral("sticker_id"), idx.data(MessageModel::StickerIdRole).toString());
            } else if (isFile) {
                m.insert(QStringLiteral("file_name"), idx.data(MessageModel::TextRole).toString());
                m.insert(QStringLiteral("file_size"), static_cast<double>(idx.data(MessageModel::FileSizeRole).toLongLong()));
            } else {
                m.insert(QStringLiteral("text"), idx.data(MessageModel::TextRole).toString());
            }
        }

        messages.prepend(m);
        exported++;
    }
    rootObj.insert(QStringLiteral("messages"), messages);

    const QJsonDocument doc(rootObj);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：%1").arg(path));
        return;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("写入失败：%1").arg(path));
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出：%1").arg(path));
}

void ChatWindow::sendFilePlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, UiSettings::Tr(QStringLiteral("选择要发送的文件"),
                             QStringLiteral("Select a file to send")));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    bool ok = false;
    if (backend_) {
        ok = isGroup_ ? backend_->sendGroupFile(conversationId_, path, messageId, err)
                      : backend_->sendFile(conversationId_, path, messageId, err);
    }
    const auto status = ok ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendFileMessage(conversationId_, true, fi.fileName(), fi.size(), path, now, messageId, status);
    if (ok && !messageId.trimmed().isEmpty()) {
        setFileTransferState(messageId, FileTransferState::Uploading);
    }

    if (!err.isEmpty()) {
        const QString prefix = ok ? QStringLiteral("提示") : QStringLiteral("发送文件失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::sendImagePlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        UiSettings::Tr(QStringLiteral("选择要发送的图片"),
                       QStringLiteral("Select an image")),
        QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;所有文件 (*)"));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    bool ok = false;
    if (backend_) {
        ok = isGroup_ ? backend_->sendGroupFile(conversationId_, path, messageId, err)
                      : backend_->sendFile(conversationId_, path, messageId, err);
    }
    const auto status = ok ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendFileMessage(conversationId_, true, fi.fileName(), fi.size(), path, now, messageId, status);
    if (ok && !messageId.trimmed().isEmpty()) {
        setFileTransferState(messageId, FileTransferState::Uploading);
    }

    if (!err.isEmpty()) {
        const QString prefix = ok ? QStringLiteral("提示") : QStringLiteral("发送图片失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::sendVoicePlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, UiSettings::Tr(QStringLiteral("选择要发送的语音文件"),
                             QStringLiteral("Select an audio file")),
        QString(), QStringLiteral("音频文件 (*.wav *.mp3 *.m4a *.aac *.ogg *.opus *.flac);;所有文件 (*)"));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    bool ok = false;
    if (backend_) {
        ok = isGroup_ ? backend_->sendGroupFile(conversationId_, path, messageId, err)
                      : backend_->sendFile(conversationId_, path, messageId, err);
    }
    const auto status = ok ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendFileMessage(conversationId_, true, fi.fileName(), fi.size(), path, now, messageId, status);
    if (ok && !messageId.trimmed().isEmpty()) {
        setFileTransferState(messageId, FileTransferState::Uploading);
    }

    if (!err.isEmpty()) {
        const QString prefix = ok ? QStringLiteral("提示") : QStringLiteral("发送语音失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::sendVideoPlaceholder() {
    if (!ensureConversationSelected()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, UiSettings::Tr(QStringLiteral("选择要发送的视频文件"),
                             QStringLiteral("Select a video file")),
        QString(), QStringLiteral("视频文件 (*.mp4 *.mkv *.mov *.webm *.avi *.flv *.m4v);;所有文件 (*)"));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    if (typingSent_ && !isGroup_ && backend_ && !conversationId_.trimmed().isEmpty() &&
        typingAction_ && typingAction_->isChecked()) {
        QString ignore;
        backend_->sendTyping(conversationId_, false, ignore);
        typingSent_ = false;
        if (typingStopSendTimer_) {
            typingStopSendTimer_->stop();
        }
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString messageId;
    QString err;
    bool ok = false;
    if (backend_) {
        ok = isGroup_ ? backend_->sendGroupFile(conversationId_, path, messageId, err)
                      : backend_->sendFile(conversationId_, path, messageId, err);
    }
    const auto status = ok ? MessageItem::Status::Sent : MessageItem::Status::Failed;
    messageModel_->appendFileMessage(conversationId_, true, fi.fileName(), fi.size(), path, now, messageId, status);
    if (ok && !messageId.trimmed().isEmpty()) {
        setFileTransferState(messageId, FileTransferState::Uploading);
    }

    if (!err.isEmpty()) {
        const QString prefix = ok ? QStringLiteral("提示") : QStringLiteral("发送视频失败");
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("%1：%2").arg(prefix, err), now);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::showMessageMenu(const QPoint &pos) {
    if (!backend_ || !messageView_ || !messageModel_) {
        return;
    }
    const QModelIndex idx = messageView_->indexAt(pos);
    if (!idx.isValid()) {
        return;
    }
    const auto type = static_cast<MessageItem::Type>(idx.data(MessageModel::TypeRole).toInt());
    if (type != MessageItem::Type::Text) {
        return;
    }
    const bool outgoing = idx.data(MessageModel::OutgoingRole).toBool();
    const bool isFile = idx.data(MessageModel::IsFileRole).toBool();
    const bool isSticker = idx.data(MessageModel::IsStickerRole).toBool();
    const auto status = static_cast<MessageItem::Status>(idx.data(MessageModel::StatusRole).toInt());
    const QString messageId = idx.data(MessageModel::MessageIdRole).toString();
    const QString text = idx.data(MessageModel::TextRole).toString();
    const QString filePath = idx.data(MessageModel::FilePathRole).toString();
    const qint64 fileSize = idx.data(MessageModel::FileSizeRole).toLongLong();
    const QString stickerId = idx.data(MessageModel::StickerIdRole).toString();

    QMenu menu(this);
    UiStyle::ApplyMenuStyle(menu);
    QAction *copyText = menu.addAction(isSticker ? QStringLiteral("复制贴纸 ID")
                                                 : (isFile ? QStringLiteral("复制文件名") : QStringLiteral("复制文本")));
    QAction *copyMessageId = nullptr;
    if (!messageId.trimmed().isEmpty()) {
        copyMessageId = menu.addAction(QStringLiteral("复制消息 ID"));
    }
    QAction *replyAction = nullptr;
    QString replyPreview;
    if (!isGroup_ && !messageId.trimmed().isEmpty()) {
        const QString nameOrPath = filePath.isEmpty() ? text : filePath;
        if (isSticker) {
            replyPreview = QStringLiteral("[贴纸]");
        } else if (isFile) {
            replyPreview = LooksLikeImageFile(nameOrPath) ? QStringLiteral("[图片] %1").arg(text)
                                                         : QStringLiteral("[文件] %1").arg(text);
        } else {
            replyPreview = text;
        }
        replyPreview = replyPreview.simplified();
        if (replyPreview.size() > 80) {
            replyPreview = replyPreview.left(80) + QStringLiteral("…");
        }
        replyAction = menu.addAction(QStringLiteral("引用回复"));
    }
    QAction *openLink = nullptr;
    QAction *previewLink = nullptr;
    QString url;
    if (!isFile && !isSticker) {
        url = ExtractFirstUrl(text);
        if (!url.isEmpty()) {
            openLink = menu.addAction(QStringLiteral("打开链接"));
            previewLink = menu.addAction(QStringLiteral("链接预览..."));
        }
    }

    QAction *openMap = nullptr;
    QAction *copyCoords = nullptr;
    QAction *copyCard = nullptr;
    QString cardUsername;
    double mapLat = 0.0;
    double mapLon = 0.0;
    if (!isFile && !isSticker) {
        if (text.startsWith(QStringLiteral("【位置】"))) {
            const QStringList lines = text.split(QChar::LineFeed);
            if (lines.size() >= 2) {
                QRegularExpression re(QStringLiteral("lat:([+-]?\\d+\\.\\d+),\\s*lon:([+-]?\\d+\\.\\d+)"));
                const auto m = re.match(lines[1].trimmed());
                if (m.hasMatch()) {
                    bool okLat = false;
                    bool okLon = false;
                    mapLat = m.captured(1).toDouble(&okLat);
                    mapLon = m.captured(2).toDouble(&okLon);
                    if (okLat && okLon) {
                        openMap = menu.addAction(QStringLiteral("打开地图"));
                        copyCoords = menu.addAction(QStringLiteral("复制坐标"));
                    }
                }
            }
        }
        if (text.startsWith(QStringLiteral("【名片】"))) {
            QString rest = text.mid(QStringLiteral("【名片】").size()).trimmed();
            const int spacePos = rest.indexOf(QChar::Space);
            const int parenPos = rest.indexOf(QChar('('));
            int cut = rest.size();
            if (spacePos >= 0) {
                cut = qMin(cut, spacePos);
            }
            if (parenPos >= 0) {
                cut = qMin(cut, parenPos);
            }
            cardUsername = rest.left(cut).trimmed();
            if (!cardUsername.isEmpty()) {
                copyCard = menu.addAction(QStringLiteral("复制名片账号"));
            }
        }
    }

    QAction *retry = nullptr;
    QAction *save = nullptr;
    QAction *previewImage = nullptr;
    QAction *playAudio = nullptr;
    QAction *playVideo = nullptr;
    QAction *openLocal = nullptr;

    const QString nameOrPath = filePath.isEmpty() ? text : filePath;
    const bool looksImage = isFile && LooksLikeImageFile(nameOrPath);
    const bool looksAudio = isFile && LooksLikeAudioFile(nameOrPath);
    const bool looksVideo = isFile && LooksLikeVideoFile(nameOrPath);

    if (outgoing && (status == MessageItem::Status::Failed || status == MessageItem::Status::Pending) &&
        !messageId.isEmpty()) {
        retry = menu.addAction(isFile ? QStringLiteral("重试发送文件")
                                      : (isSticker ? QStringLiteral("重试发送贴纸") : QStringLiteral("重试发送")));
    }
    if (!outgoing && isFile && !messageId.isEmpty()) {
        save = menu.addAction(QStringLiteral("保存文件..."));
    }
    if (isFile && looksImage && !messageId.isEmpty()) {
        previewImage = menu.addAction(outgoing ? QStringLiteral("查看图片") : QStringLiteral("预览图片..."));
    }
    if (isFile && looksAudio && !messageId.isEmpty()) {
        playAudio = menu.addAction(outgoing ? UiSettings::Tr(QStringLiteral("播放语音"),
                                                             QStringLiteral("Play Audio"))
                                            : UiSettings::Tr(QStringLiteral("播放语音..."),
                                                             QStringLiteral("Play Audio...")));
    }
    if (isFile && looksVideo && !messageId.isEmpty()) {
        playVideo = menu.addAction(outgoing ? UiSettings::Tr(QStringLiteral("播放视频"),
                                                             QStringLiteral("Play Video"))
                                            : UiSettings::Tr(QStringLiteral("播放视频..."),
                                                             QStringLiteral("Play Video...")));
    }
    if (outgoing && isFile && !filePath.trimmed().isEmpty()) {
        openLocal = menu.addAction(QStringLiteral("打开本地文件"));
    }
    if (!copyText && !copyMessageId && !openLink && !previewLink && !retry && !save &&
        !previewImage && !playAudio && !playVideo && !openLocal) {
        return;
    }
    QAction *picked = menu.exec(messageView_->viewport()->mapToGlobal(pos));
    if (picked == copyText) {
        SecureClipboard::SetText(isSticker ? stickerId : text);
        return;
    }
    if (picked == copyMessageId) {
        SecureClipboard::SetText(messageId);
        return;
    }
    if (picked == replyAction) {
        setReplyContext(messageId, replyPreview);
        if (inputEdit_) {
            inputEdit_->setFocus();
        }
        return;
    }
    if (picked == openMap) {
        const QString urlStr =
            QStringLiteral("https://www.openstreetmap.org/?mlat=%1&mlon=%2#map=16/%1/%2")
                .arg(mapLat, 0, 'f', 7)
                .arg(mapLon, 0, 'f', 7);
        QDesktopServices::openUrl(QUrl(urlStr));
        return;
    }
    if (picked == copyCoords) {
        SecureClipboard::SetText(QStringLiteral("%1,%2")
                                     .arg(mapLat, 0, 'f', 7)
                                     .arg(mapLon, 0, 'f', 7));
        return;
    }
    if (picked == copyCard) {
        SecureClipboard::SetText(cardUsername);
        return;
    }
    if (picked == openLink) {
        QDesktopServices::openUrl(QUrl(url));
        return;
    }
    if (picked == previewLink) {
        ShowLinkPreviewDialog(this, QUrl(url));
        return;
    }
    if (picked == retry) {
        QString err;
        bool ok = false;
        if (isFile) {
            if (filePath.isEmpty()) {
                err = UiSettings::Tr(QStringLiteral("缺少本地文件路径，无法重试"),
                                     QStringLiteral("Missing local file path; can't retry."));
            } else {
                ok = isGroup_ ? backend_->resendGroupFile(conversationId_, messageId, filePath, err)
                              : backend_->resendFile(conversationId_, messageId, filePath, err);
            }
        } else if (isSticker) {
            ok = backend_->resendSticker(conversationId_, messageId, stickerId, err);
        } else {
            ok = isGroup_ ? backend_->resendGroupText(conversationId_, messageId, text, err)
                          : backend_->resendText(conversationId_, messageId, text, err);
        }
        MessageItem::Status nextStatus = MessageItem::Status::Sent;
        const QString errTrimmed = err.trimmed();
        if (!ok) {
            const bool retryable =
                !messageId.trimmed().isEmpty() &&
                !IsNonRetryableSendError(errTrimmed) &&
                !IsNotFriendsError(errTrimmed);
            nextStatus = retryable ? MessageItem::Status::Pending : MessageItem::Status::Failed;
        }
        messageModel_->updateMessageStatus(messageId, nextStatus);
        if (ok && isFile) {
            setFileTransferState(messageId, FileTransferState::Uploading);
        }
        if (!errTrimmed.isEmpty()) {
            if (ok) {
                Toast::Show(this, errTrimmed, Toast::Level::Info);
            } else if (nextStatus == MessageItem::Status::Pending) {
                Toast::Show(this,
                            UiSettings::Tr(QStringLiteral("已加入发送队列：%1").arg(errTrimmed),
                                           QStringLiteral("Queued to retry: %1").arg(errTrimmed)),
                            Toast::Level::Warning,
                            2600);
            } else {
                Toast::Show(this,
                            UiSettings::Tr(QStringLiteral("重试失败：%1").arg(errTrimmed),
                                           QStringLiteral("Retry failed: %1").arg(errTrimmed)),
                            Toast::Level::Error);
            }
        }
        return;
    }
    if (picked == playAudio) {
        if (!filePath.trimmed().isEmpty()) {
#if defined(MI_UI_HAS_QT_MULTIMEDIA)
            const QString local = filePath;
            ShowAudioDialog(this, text, [local](QMediaPlayer *player) {
                player->setSource(QUrl::fromLocalFile(local));
            });
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
#endif
            return;
        }

        const qint64 maxPreviewBytes = 25 * 1024 * 1024;
        if (fileSize > 0 && fileSize > maxPreviewBytes) {
            messageModel_->appendSystemMessage(
                conversationId_,
                UiSettings::Tr(QStringLiteral("语音过大，无法播放；请保存后用外部播放器打开。"),
                               QStringLiteral("Audio too large to play; please save and open externally.")),
                QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        QByteArray bytes;
        QString err;
        if (!backend_->loadReceivedFileBytes(conversationId_, messageId, bytes, maxPreviewBytes, false, err)) {
            messageModel_->appendSystemMessage(
                conversationId_,
                UiSettings::Tr(QStringLiteral("播放失败：%1").arg(err),
                               QStringLiteral("Play failed: %1").arg(err)),
                QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
#if defined(MI_UI_HAS_QT_MULTIMEDIA)
        ShowAudioDialog(this, text, [bytes](QMediaPlayer *player) {
            auto *buf = new QBuffer(player);
            buf->setData(bytes);
            buf->open(QIODevice::ReadOnly);
            player->setSourceDevice(buf, QUrl(QStringLiteral("mem:///audio")));
        });
#else
        messageModel_->appendSystemMessage(
            conversationId_,
            UiSettings::Tr(QStringLiteral("当前构建未启用 Qt Multimedia，无法直接播放；请保存文件后打开。"),
                           QStringLiteral("Qt Multimedia not enabled; please save then open.")),
            QDateTime::currentDateTime());
        messageView_->scrollToBottom();
#endif
        return;
    }
    if (picked == playVideo) {
        if (!filePath.trimmed().isEmpty()) {
#if defined(MI_UI_HAS_QT_MULTIMEDIA)
            const QString local = filePath;
            ShowVideoDialog(this, text, [local](QMediaPlayer *player) {
                player->setSource(QUrl::fromLocalFile(local));
            });
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
#endif
            return;
        }

        const qint64 maxPreviewBytes = 120ll * 1024ll * 1024ll;
        if (fileSize > 0 && fileSize > maxPreviewBytes) {
            messageModel_->appendSystemMessage(
                conversationId_,
                UiSettings::Tr(QStringLiteral("视频过大，无法预览；请保存后用外部播放器打开。"),
                               QStringLiteral("Video too large to preview; please save and open externally.")),
                QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        QByteArray bytes;
        QString err;
        if (!backend_->loadReceivedFileBytes(conversationId_, messageId, bytes, maxPreviewBytes, false, err)) {
            messageModel_->appendSystemMessage(
                conversationId_,
                UiSettings::Tr(QStringLiteral("播放失败：%1").arg(err),
                               QStringLiteral("Play failed: %1").arg(err)),
                QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
#if defined(MI_UI_HAS_QT_MULTIMEDIA)
        ShowVideoDialog(this, text, [bytes](QMediaPlayer *player) {
            auto *buf = new QBuffer(player);
            buf->setData(bytes);
            buf->open(QIODevice::ReadOnly);
            player->setSourceDevice(buf, QUrl(QStringLiteral("mem:///video")));
        });
#else
        messageModel_->appendSystemMessage(
            conversationId_,
            UiSettings::Tr(QStringLiteral("当前构建未启用 Qt Multimedia，无法直接播放；请保存文件后打开。"),
                           QStringLiteral("Qt Multimedia not enabled; please save then open.")),
            QDateTime::currentDateTime());
        messageView_->scrollToBottom();
#endif
        return;
    }
    if (picked == previewImage) {
        if (outgoing && !filePath.trimmed().isEmpty()) {
            const QImage img(filePath);
            ShowImageDialog(this, img, text);
            return;
        }

        const qint64 maxPreviewBytes = 25 * 1024 * 1024;
        if (fileSize > 0 && fileSize > maxPreviewBytes) {
            messageModel_->appendSystemMessage(conversationId_, QStringLiteral("图片过大，无法预览"), QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        QByteArray bytes;
        QString err;
        if (!backend_->loadReceivedFileBytes(conversationId_, messageId, bytes, maxPreviewBytes, false, err)) {
            messageModel_->appendSystemMessage(conversationId_, QStringLiteral("预览失败：%1").arg(err), QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        QImage img;
        if (!img.loadFromData(bytes)) {
            messageModel_->appendSystemMessage(conversationId_, QStringLiteral("预览失败：图片解码失败"), QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        ShowImageDialog(this, img, text);
        return;
    }
    if (picked == openLocal) {
        if (filePath.trimmed().isEmpty()) {
            messageModel_->appendSystemMessage(conversationId_, QStringLiteral("缺少本地路径，无法打开"), QDateTime::currentDateTime());
            messageView_->scrollToBottom();
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }
    if (picked == save) {
        const QString outPath = QFileDialog::getSaveFileName(
            this,
            UiSettings::Tr(QStringLiteral("保存文件"), QStringLiteral("Save file")),
            text);
        if (outPath.isEmpty()) {
            return;
        }
        QString err;
        if (!backend_->saveReceivedFile(conversationId_, messageId, outPath, err)) {
            Toast::Show(this,
                        UiSettings::Tr(QStringLiteral("保存失败：%1").arg(err),
                                       QStringLiteral("Save failed: %1").arg(err)),
                        Toast::Level::Error);
            setFileTransferState(messageId, FileTransferState::None);
            return;
        }
        setFileTransferState(messageId, FileTransferState::Downloading);
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("开始保存…"), QStringLiteral("Saving…")),
                    Toast::Level::Info);
        return;
    }
}

void ChatWindow::updateOverlayForTitle(const QString &title) {
    if (title.contains(QStringLiteral("群"))) {
        setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_group_chat.png"));
    } else {
        setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
    }
}

void ChatWindow::manageGroupMembers() {
    if (!backend_ || !isGroup_) {
        return;
    }

    const QString groupId = conversationId_.trimmed();
    if (groupId.isEmpty()) {
        return;
    }

    QString err;
    const auto initial = backend_->listGroupMembersInfo(groupId, err);
    if (initial.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("群成员"),
                             err.isEmpty() ? QStringLiteral("获取成员信息失败") : err);
        return;
    }

    const QString self = backend_->currentUser().trimmed();

    struct DialogState {
        QVector<BackendAdapter::GroupMemberRoleEntry> members;
        int selfRole{2};
    };

    auto state = std::make_shared<DialogState>();
    state->members = initial;
    for (const auto &m : state->members) {
        if (!self.isEmpty() && m.username == self) {
            state->selfRole = m.role;
            break;
        }
    }

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle(QStringLiteral("群成员"));
    dlg->resize(520, 440);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *hint = new QLabel(dlg);
    hint->setTextFormat(Qt::PlainText);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral("我的角色：%1").arg(GroupRoleText(state->selfRole)));
    root->addWidget(hint);

    auto *table = new QTableWidget(dlg);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({QStringLiteral("成员"), QStringLiteral("角色")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    root->addWidget(table, 1);

    auto *buttonsRow = new QHBoxLayout();
    buttonsRow->setSpacing(8);

    auto *refreshBtn = outlineButton(QStringLiteral("刷新"), dlg);
    auto *promoteBtn = outlineButton(QStringLiteral("设为管理员"), dlg);
    auto *demoteBtn = outlineButton(QStringLiteral("设为成员"), dlg);
    auto *kickBtn = outlineButton(QStringLiteral("踢出"), dlg);
    auto *closeBtn = primaryButton(QStringLiteral("关闭"), dlg);

    buttonsRow->addWidget(refreshBtn);
    buttonsRow->addStretch();
    buttonsRow->addWidget(promoteBtn);
    buttonsRow->addWidget(demoteBtn);
    buttonsRow->addWidget(kickBtn);
    buttonsRow->addWidget(closeBtn);
    root->addLayout(buttonsRow);

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

    auto populate = [table](const QVector<BackendAdapter::GroupMemberRoleEntry> &list) {
        table->clearContents();
        table->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            const auto &m = list[i];
            auto *userItem = new QTableWidgetItem(m.username);
            auto *roleItem = new QTableWidgetItem(GroupRoleText(m.role));
            table->setItem(i, 0, userItem);
            table->setItem(i, 1, roleItem);
        }
        table->resizeColumnsToContents();
    };

    auto refresh = [this, groupId, hint, self, state, populate]() -> bool {
        QString err;
        const auto list = backend_->listGroupMembersInfo(groupId, err);
        if (list.isEmpty()) {
            if (!err.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("群成员"), err);
            }
            return false;
        }
        state->members = list;
        state->selfRole = 2;
        for (const auto &m : state->members) {
            if (!self.isEmpty() && m.username == self) {
                state->selfRole = m.role;
                break;
            }
        }
        hint->setText(QStringLiteral("我的角色：%1").arg(GroupRoleText(state->selfRole)));
        populate(state->members);
        return true;
    };

    populate(state->members);

    auto updateButtons = [=]() {
        const QString selected = currentSelected();
        int selectedRole = -1;
        for (const auto &m : state->members) {
            if (m.username == selected) {
                selectedRole = m.role;
                break;
            }
        }
        const bool hasSel = !selected.trimmed().isEmpty() && selectedRole >= 0;
        const bool selIsSelf = hasSel && !self.isEmpty() && selected == self;
        const bool selIsOwner = hasSel && selectedRole == 0;

        const bool canManageRoles = (state->selfRole == 0);
        const bool canKick = (state->selfRole == 0) || (state->selfRole == 1);

        promoteBtn->setEnabled(canManageRoles && hasSel && !selIsSelf && !selIsOwner && selectedRole != 1);
        demoteBtn->setEnabled(canManageRoles && hasSel && !selIsSelf && !selIsOwner && selectedRole != 2);
        if (!canKick || !hasSel || selIsSelf || selIsOwner) {
            kickBtn->setEnabled(false);
        } else if (state->selfRole == 1) {
            kickBtn->setEnabled(selectedRole == 2);
        } else {
            kickBtn->setEnabled(true);
        }
    };

    QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    QObject::connect(refreshBtn, &QPushButton::clicked, dlg, [=]() {
        if (!refresh()) {
            return;
        }
        updateButtons();
    });

    QObject::connect(table, &QTableWidget::itemSelectionChanged, dlg, updateButtons);

    QObject::connect(promoteBtn, &QPushButton::clicked, dlg, [=]() {
        const QString selected = currentSelected();
        if (selected.trimmed().isEmpty()) {
            return;
        }
        QString err;
        if (!backend_->setGroupMemberRole(groupId, selected, 1, err)) {
            QMessageBox::warning(this, QStringLiteral("设置角色"),
                                 err.isEmpty() ? QStringLiteral("设置失败") : err);
            return;
        }
        if (!refresh()) {
            return;
        }
        updateButtons();
    });

    QObject::connect(demoteBtn, &QPushButton::clicked, dlg, [=]() {
        const QString selected = currentSelected();
        if (selected.trimmed().isEmpty()) {
            return;
        }
        QString err;
        if (!backend_->setGroupMemberRole(groupId, selected, 2, err)) {
            QMessageBox::warning(this, QStringLiteral("设置角色"),
                                 err.isEmpty() ? QStringLiteral("设置失败") : err);
            return;
        }
        if (!refresh()) {
            return;
        }
        updateButtons();
    });

    QObject::connect(kickBtn, &QPushButton::clicked, dlg, [=]() {
        const QString selected = currentSelected();
        if (selected.trimmed().isEmpty()) {
            return;
        }
        if (QMessageBox::question(this, QStringLiteral("踢出成员"),
                                  QStringLiteral("确认踢出：%1 ?").arg(selected)) != QMessageBox::Yes) {
            return;
        }
        QString err;
        if (!backend_->kickGroupMember(groupId, selected, err)) {
            QMessageBox::warning(this, QStringLiteral("踢出成员"),
                                 err.isEmpty() ? QStringLiteral("踢出失败") : err);
            return;
        }
        if (!refresh()) {
            return;
        }
        updateButtons();
    });

    updateButtons();
    dlg->show();
}

void ChatWindow::inviteMember() {
    if (!backend_ || !isGroup_) {
        return;
    }
    bool ok = false;
    const QString who = QInputDialog::getText(this, QStringLiteral("邀请成员"),
                                              QStringLiteral("输入对方账号"),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok || who.trimmed().isEmpty()) {
        return;
    }
    QString messageId;
    QString err;
    const bool sent = backend_->sendGroupInvite(conversationId_, who.trimmed(), messageId, err);
    const QDateTime now = QDateTime::currentDateTime();
    if (!sent) {
        messageModel_->appendSystemMessage(conversationId_, err.isEmpty() ? QStringLiteral("邀请失败") : QStringLiteral("邀请失败：%1").arg(err), now);
        messageView_->scrollToBottom();
        return;
    }
    if (!err.isEmpty()) {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("提示：%1").arg(err), now);
    } else {
        messageModel_->appendSystemMessage(conversationId_, QStringLiteral("已邀请：%1").arg(who.trimmed()), now);
    }
    messageView_->scrollToBottom();
}

void ChatWindow::leaveGroup() {
    if (!backend_ || !isGroup_) {
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("退出群聊"),
                              QStringLiteral("确认退出群聊？")) != QMessageBox::Yes) {
        return;
    }
    QString err;
    if (!backend_->leaveGroup(conversationId_, err)) {
        messageModel_->appendSystemMessage(conversationId_, err.isEmpty() ? QStringLiteral("退出失败") : QStringLiteral("退出失败：%1").arg(err), QDateTime::currentDateTime());
        messageView_->scrollToBottom();
        return;
    }
    close();
}
