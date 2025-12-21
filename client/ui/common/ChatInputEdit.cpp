#include "ChatInputEdit.h"

#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QScreen>
#include <QTextCursor>
#include <QTextLayout>
#include <QVector>

#include <algorithm>

#include "Theme.h"

namespace {

struct PinyinEntry {
    const char *key;
    const char *candidates;
};

static const PinyinEntry kPinyinDict[] = {
    {"ai", u8"\u7231|\u827E|\u54C0"},
    {"an", u8"\u5B89|\u6309|\u6848"},
    {"ba", u8"\u628A|\u516B|\u5427"},
    {"ban", u8"\u534A|\u73ED|\u529E"},
    {"bei", u8"\u5317|\u88AB|\u5907"},
    {"ben", u8"\u672C|\u5954"},
    {"bi", u8"\u6BD4|\u5FC5|\u7B14"},
    {"bian", u8"\u53D8|\u8FB9|\u7F16"},
    {"bu", u8"\u4E0D|\u90E8|\u5E03"},
    {"chang", u8"\u957F|\u5E38|\u573A"},
    {"chi", u8"\u5403|\u8FDF|\u5C3A"},
    {"chu", u8"\u51FA|\u5904|\u521D"},
    {"da", u8"\u5927|\u6253|\u8FBE"},
    {"de", u8"\u7684|\u5F97|\u5FB7"},
    {"dian", u8"\u70B9|\u7535|\u5E97"},
    {"dong", u8"\u4E1C|\u52A8"},
    {"dui", u8"\u5BF9|\u961F"},
    {"en", u8"\u6069"},
    {"er", u8"\u4E8C|\u800C"},
    {"fa", u8"\u53D1|\u6CD5"},
    {"fang", u8"\u65B9|\u623F"},
    {"fei", u8"\u975E|\u98DE|\u8D39"},
    {"fu", u8"\u670D|\u590D|\u4ED8"},
    {"ge", u8"\u4E2A|\u5404|\u6B4C"},
    {"gong", u8"\u5DE5|\u516C|\u5171"},
    {"guo", u8"\u56FD|\u8FC7|\u679C"},
    {"hao", u8"\u597D|\u53F7|\u6D69"},
    {"he", u8"\u548C|\u5408|\u4F55"},
    {"hen", u8"\u5F88"},
    {"hui", u8"\u4F1A|\u56DE|\u7070"},
    {"ji", u8"\u673A|\u7EA7|\u8BB0"},
    {"jia", u8"\u5BB6|\u52A0|\u67B6"},
    {"jian", u8"\u89C1|\u4EF6|\u5EFA"},
    {"jiang", u8"\u5C06|\u8BB2|\u6C5F"},
    {"jin", u8"\u8FDB|\u4ECA|\u91D1"},
    {"jing", u8"\u7ECF|\u4EAC|\u7CBE"},
    {"jiu", u8"\u5C31|\u4E5D|\u4E45"},
    {"kan", u8"\u770B|\u780D"},
    {"ke", u8"\u53EF|\u79D1|\u5BA2"},
    {"lai", u8"\u6765"},
    {"le", u8"\u4E86|\u4E50"},
    {"li", u8"\u91CC|\u7406|\u529B"},
    {"liao", u8"\u4E86|\u6599|\u804A"},
    {"ling", u8"\u9886|\u4EE4|\u7075"},
    {"ma", u8"\u5417|\u9A6C|\u9EBB"},
    {"mei", u8"\u6CA1|\u6BCF|\u7F8E"},
    {"men", u8"\u4EEC|\u95E8"},
    {"ming", u8"\u660E|\u540D"},
    {"na", u8"\u90A3|\u62FF|\u54EA"},
    {"ne", u8"\u5462"},
    {"ni", u8"\u4F60|\u59AE|\u6CE5"},
    {"nin", u8"\u60A8"},
    {"neng", u8"\u80FD"},
    {"peng", u8"\u670B|\u78B0"},
    {"qing", u8"\u8BF7|\u60C5|\u6E05"},
    {"qu", u8"\u53BB|\u533A|\u53D6"},
    {"ren", u8"\u4EBA|\u4EFB|\u8BA4"},
    {"shi", u8"\u662F|\u65F6|\u4E8B"},
    {"shou", u8"\u624B|\u6536"},
    {"shui", u8"\u6C34"},
    {"shuo", u8"\u8BF4"},
    {"ta", u8"\u4ED6|\u5979|\u5B83"},
    {"tian", u8"\u5929"},
    {"ting", u8"\u542C"},
    {"tong", u8"\u540C|\u901A"},
    {"wan", u8"\u5B8C|\u4E07"},
    {"wei", u8"\u4E3A|\u4F4D"},
    {"wo", u8"\u6211"},
    {"xi", u8"\u897F|\u559C"},
    {"xie", u8"\u5199|\u8C22"},
    {"xin", u8"\u65B0|\u5FC3"},
    {"xiong", u8"\u96C4"},
    {"xue", u8"\u5B66"},
    {"yao", u8"\u8981"},
    {"ye", u8"\u4E5F"},
    {"yi", u8"\u4E00|\u4EE5|\u5DF2"},
    {"you", u8"\u6709|\u53C8"},
    {"yu", u8"\u4E8E|\u4E0E"},
    {"yuan", u8"\u5143|\u8FDC"},
    {"zai", u8"\u5728"},
    {"zan", u8"\u8D5E"},
    {"ze", u8"\u5219"},
    {"zhe", u8"\u8FD9|\u7740"},
    {"zhi", u8"\u53EA|\u4E4B|\u77E5"},
    {"zhong", u8"\u4E2D|\u7EC8"},
    {"zhou", u8"\u5468"},
    {"zhu", u8"\u4E3B|\u4F4F"},
    {"zuo", u8"\u505A|\u4F5C"},
    {"nihao", u8"\u4F60\u597D"},
    {"xiexie", u8"\u8C22\u8C22"},
    {"zaijian", u8"\u518D\u89C1"},
    {"zhongguo", u8"\u4E2D\u56FD"},
    {"women", u8"\u6211\u4EEC"},
    {"nimen", u8"\u4F60\u4EEC"},
    {"tamen", u8"\u4ED6\u4EEC"},
    {"haode", u8"\u597D\u7684"},
    {"meiyou", u8"\u6CA1\u6709"},
    {"mingzi", u8"\u540D\u5B57"},
    {"pengyou", u8"\u670B\u53CB"},
    {"laoshi", u8"\u8001\u5E08"},
    {"xuesheng", u8"\u5B66\u751F"},
    {"jintian", u8"\u4ECA\u5929"},
    {"mingtian", u8"\u660E\u5929"},
    {"zuotian", u8"\u6628\u5929"},
    {"haoma", u8"\u53F7\u7801"},
    {"shouji", u8"\u624B\u673A"},
    {"dianhua", u8"\u7535\u8BDD"},
    {"gongsi", u8"\u516C\u53F8"},
};

QStringList SplitCandidates(const char *raw) {
    QStringList list;
    if (!raw || !*raw) {
        return list;
    }
    QString current;
    const QByteArray bytes(raw);
    const QString text = QString::fromUtf8(bytes);
    for (QChar ch : text) {
        if (ch == QChar('|')) {
            if (!current.isEmpty()) {
                list.push_back(current);
            }
            current.clear();
        } else {
            current.append(ch);
        }
    }
    if (!current.isEmpty()) {
        list.push_back(current);
    }
    return list;
}

const QHash<QString, QStringList> &PinyinDict() {
    static QHash<QString, QStringList> dict;
    static bool built = false;
    if (!built) {
        for (const auto &entry : kPinyinDict) {
            dict.insert(QString::fromLatin1(entry.key), SplitCandidates(entry.candidates));
        }
        built = true;
    }
    return dict;
}

const QVector<QString> &PinyinKeys() {
    static QVector<QString> keys;
    static bool built = false;
    if (!built) {
        const auto &dict = PinyinDict();
        keys.reserve(dict.size());
        for (auto it = dict.constBegin(); it != dict.constEnd(); ++it) {
            keys.push_back(it.key());
        }
        std::sort(keys.begin(), keys.end(),
                  [](const QString &a, const QString &b) { return a.size() > b.size(); });
        built = true;
    }
    return keys;
}

QString SegmentFallback(const QString &pinyin) {
    if (pinyin.isEmpty()) {
        return {};
    }
    const int n = pinyin.size();
    QVector<int> score(n + 1, -1);
    QVector<int> prev(n + 1, -1);
    QVector<QString> prevKey(n + 1);
    score[0] = 0;
    const auto &dict = PinyinDict();
    const auto &keys = PinyinKeys();
    for (int i = 0; i < n; ++i) {
        if (score[i] < 0) {
            continue;
        }
        for (const auto &key : keys) {
            const int len = key.size();
            if (i + len > n) {
                continue;
            }
            if (pinyin.mid(i, len) != key) {
                continue;
            }
            const int j = i + len;
            const int nextScore = score[i] + len * 2 - 1;
            if (nextScore > score[j]) {
                score[j] = nextScore;
                prev[j] = i;
                prevKey[j] = key;
            }
        }
    }
    if (score[n] < 0) {
        return {};
    }
    QStringList chunks;
    int cur = n;
    while (cur > 0 && prev[cur] >= 0) {
        const QString key = prevKey[cur];
        const auto it = dict.constFind(key);
        if (it != dict.constEnd() && !it.value().isEmpty()) {
            chunks.push_front(it.value().front());
        }
        cur = prev[cur];
    }
    return chunks.join(QString());
}

QStringList BuildCandidates(const QString &pinyin) {
    const auto &dict = PinyinDict();
    QStringList list;
    const auto it = dict.constFind(pinyin);
    if (it != dict.constEnd()) {
        list = it.value();
    }
    const QString fallback = SegmentFallback(pinyin);
    if (!fallback.isEmpty() && !list.contains(fallback)) {
        list.push_back(fallback);
    }
    if (list.isEmpty()) {
        list.push_back(pinyin);
    }
    return list;
}

class CandidateLabel : public QLabel {
public:
    explicit CandidateLabel(QWidget *parent = nullptr) : QLabel(parent) {
        setTextFormat(Qt::RichText);
    }
};

}  // namespace

class ChatInputEdit::CandidatePopup : public QFrame {
public:
    explicit CandidatePopup(QWidget *parent = nullptr) : QFrame(parent) {
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }"
            "QLabel { color: %3; font-size: 11px; padding: 6px 8px; }")
                          .arg(Theme::uiPanelBg().name(),
                               Theme::uiBorder().name(),
                               Theme::uiTextMain().name()));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        label_ = new CandidateLabel(this);
        layout->addWidget(label_);
    }

    void setCandidates(const QString &pinyin, const QStringList &cands, int selected) {
        if (!label_) {
            return;
        }
        QStringList parts;
        const int maxCount = qMin(5, cands.size());
        for (int i = 0; i < maxCount; ++i) {
            const QString entry =
                QStringLiteral("%1.%2").arg(i + 1).arg(cands.at(i));
            if (i == selected) {
                parts.push_back(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                                    .arg(Theme::uiAccentBlue().name(),
                                         entry.toHtmlEscaped()));
            } else {
                parts.push_back(entry.toHtmlEscaped());
            }
        }
        const QString head = pinyin.isEmpty() ? QString() : pinyin.toHtmlEscaped();
        const QString body = parts.join(QStringLiteral("  "));
        if (head.isEmpty()) {
            label_->setText(body);
        } else {
            label_->setText(head + QStringLiteral("  ") + body);
        }
        adjustSize();
    }

private:
    CandidateLabel *label_{nullptr};
};

ChatInputEdit::ChatInputEdit(QWidget *parent) : QPlainTextEdit(parent) {
    setAttribute(Qt::WA_InputMethodEnabled, false);
}

bool ChatInputEdit::isComposing() const {
    return composing_;
}

bool ChatInputEdit::imeEnabled() const {
    return imeEnabled_;
}

void ChatInputEdit::setImeEnabled(bool enabled) {
    if (imeEnabled_ == enabled) {
        return;
    }
    imeEnabled_ = enabled;
    if (!imeEnabled_) {
        cancelComposition(true);
    }
}

void ChatInputEdit::keyPressEvent(QKeyEvent *event) {
    if (!event) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (event->matches(QKeySequence::InsertLineSeparator)) {
        if (composing_) {
            commitCandidate(candidateIndex_);
            event->accept();
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Space) {
        handleToggleIme();
        event->accept();
        return;
    }
    if (imeEnabled_ && handleCompositionKey(event)) {
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ChatInputEdit::focusOutEvent(QFocusEvent *event) {
    cancelComposition(true);
    QPlainTextEdit::focusOutEvent(event);
}

void ChatInputEdit::mousePressEvent(QMouseEvent *event) {
    cancelComposition(true);
    QPlainTextEdit::mousePressEvent(event);
}

void ChatInputEdit::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    updatePopupPosition();
}

void ChatInputEdit::handleToggleIme() {
    setImeEnabled(!imeEnabled_);
    hidePopup();
}

bool ChatInputEdit::handleCompositionKey(QKeyEvent *event) {
    const int key = event->key();
    const QString text = event->text();
    if (text.size() == 1) {
        const QChar ch = text.at(0);
        if (ch.isLetter()) {
            startComposition(QString(ch).toLower());
            event->accept();
            return true;
        }
    }
    if (!composing_) {
        return false;
    }
    if (key == Qt::Key_Backspace) {
        if (!composition_.isEmpty()) {
            composition_.chop(1);
            if (composition_.isEmpty()) {
                cancelComposition(false);
            } else {
                updateCompositionText();
                updateCandidates();
            }
        } else {
            cancelComposition(false);
        }
        event->accept();
        return true;
    }
    if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter) {
        commitCandidate(candidateIndex_);
        event->accept();
        return true;
    }
    if (key >= Qt::Key_1 && key <= Qt::Key_5) {
        const int index = key - Qt::Key_1;
        commitCandidate(index);
        event->accept();
        return true;
    }
    if (key == Qt::Key_Left) {
        candidateIndex_ = qMax(0, candidateIndex_ - 1);
        updateCandidates();
        event->accept();
        return true;
    }
    if (key == Qt::Key_Right) {
        candidateIndex_ = qMin(candidateIndex_ + 1, candidates_.size() - 1);
        updateCandidates();
        event->accept();
        return true;
    }
    if (key == Qt::Key_Escape) {
        cancelComposition(false);
        event->accept();
        return true;
    }
    if (text.size() == 1 && !text.at(0).isLetter() && !text.at(0).isSpace()) {
        commitCandidate(candidateIndex_);
        QPlainTextEdit::keyPressEvent(event);
        return true;
    }
    return true;
}

void ChatInputEdit::startComposition(const QString &ch) {
    if (!composing_) {
        composing_ = true;
        candidateIndex_ = 0;
        compStart_ = textCursor().position();
        compLength_ = 0;
        composition_.clear();
    }
    composition_ += ch;
    updateCompositionText();
    updateCandidates();
}

void ChatInputEdit::updateCompositionText() {
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(composition_);
    compLength_ = composition_.size();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    cursor.endEditBlock();
}

void ChatInputEdit::updateCandidates() {
    candidates_ = BuildCandidates(composition_);
    if (candidateIndex_ >= candidates_.size()) {
        candidateIndex_ = 0;
    }
    showPopup();
    if (popup_) {
        popup_->setCandidates(composition_, candidates_, candidateIndex_);
    }
    updatePopupPosition();
}

void ChatInputEdit::commitCandidate(int index) {
    if (!composing_) {
        return;
    }
    if (candidates_.isEmpty()) {
        cancelComposition(true);
        return;
    }
    const int safeIndex = qBound(0, index, candidates_.size() - 1);
    const QString candidate = candidates_.at(safeIndex);
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(compStart_);
    cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(candidate);
    cursor.endEditBlock();
    setTextCursor(cursor);
    composing_ = false;
    composition_.clear();
    candidates_.clear();
    compLength_ = 0;
    hidePopup();
}

void ChatInputEdit::cancelComposition(bool keepText) {
    if (!composing_) {
        hidePopup();
        return;
    }
    if (!keepText) {
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.setPosition(compStart_);
        cursor.setPosition(compStart_ + compLength_, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
        setTextCursor(cursor);
    } else {
        QTextCursor cursor = textCursor();
        cursor.setPosition(compStart_ + compLength_);
        setTextCursor(cursor);
    }
    composing_ = false;
    composition_.clear();
    candidates_.clear();
    compLength_ = 0;
    hidePopup();
}

void ChatInputEdit::ensurePopup() {
    if (!popup_) {
        popup_ = new CandidatePopup(this);
    }
}

void ChatInputEdit::showPopup() {
    ensurePopup();
    if (popup_ && !popup_->isVisible()) {
        popup_->show();
    }
}

void ChatInputEdit::hidePopup() {
    if (popup_) {
        popup_->hide();
    }
}

void ChatInputEdit::updatePopupPosition() {
    if (!popup_ || !popup_->isVisible()) {
        return;
    }
    const QRect cursor = cursorRect();
    QPoint global = mapToGlobal(QPoint(cursor.left(), cursor.top()));
    const QSize popupSize = popup_->sizeHint();
    global.setY(global.y() - popupSize.height() - 6);
    if (QScreen *screen = QGuiApplication::screenAt(global)) {
        const QRect bounds = screen->availableGeometry();
        if (global.y() < bounds.top()) {
            global = mapToGlobal(QPoint(cursor.left(), cursor.bottom() + 6));
        }
        if (global.x() + popupSize.width() > bounds.right()) {
            global.setX(bounds.right() - popupSize.width());
        }
        if (global.x() < bounds.left()) {
            global.setX(bounds.left());
        }
    }
    popup_->move(global);
}
