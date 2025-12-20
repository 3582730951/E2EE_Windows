#include "EmojiPickerDialog.h"

#include <QAbstractListModel>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListView>
#include <QPainter>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QVector>
#include <QVBoxLayout>

#include "../common/Theme.h"

namespace {

QString EmojiFromCode(const QString &code) {
    if (code.trimmed().isEmpty()) {
        return {};
    }
    const QStringList parts =
        code.split(QRegularExpression(QStringLiteral("[-\\s]+")), Qt::SkipEmptyParts);
    QVector<uint> cps;
    cps.reserve(parts.size());
    for (const QString &part : parts) {
        bool ok = false;
        const uint cp = part.toUInt(&ok, 16);
        if (ok) {
            cps.push_back(cp);
        }
    }
    if (cps.isEmpty()) {
        return {};
    }
    return QString::fromUcs4(cps.constData(), cps.size());
}

QVector<QString> LoadEmojiList() {
    QVector<QString> out;
    QFile file(QStringLiteral(":/mi/e2ee/ui/emoji/emoji.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return out;
    }
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const auto &v : arr) {
        if (!v.isString()) {
            continue;
        }
        const QString emoji = EmojiFromCode(v.toString());
        if (!emoji.isEmpty()) {
            out.push_back(emoji);
        }
    }
    return out;
}

class EmojiListModel final : public QAbstractListModel {
public:
    explicit EmojiListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent), emojis_(LoadEmojiList()) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid()) {
            return 0;
        }
        return emojis_.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= emojis_.size()) {
            return {};
        }
        if (role == Qt::DisplayRole) {
            return emojis_[index.row()];
        }
        return {};
    }

private:
    QVector<QString> emojis_;
};

class EmojiDelegate final : public QStyledItemDelegate {
public:
    explicit EmojiDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent), font_(Theme::defaultFont(18)) {
        font_.setFamilies(QStringList() << QStringLiteral("Segoe UI Emoji")
                                        << QStringLiteral("Apple Color Emoji")
                                        << QStringLiteral("Noto Color Emoji")
                                        << QStringLiteral("Segoe UI Symbol")
                                        << font_.family());
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return QSize(30, 30);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        painter->save();
        QRect r = option.rect.adjusted(2, 2, -2, -2);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::uiHoverBg());
            painter->drawRoundedRect(r, 6, 6);
        }
        painter->setFont(font_);
        painter->setPen(Theme::uiTextMain());
        painter->drawText(r, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
        painter->restore();
    }

private:
    QFont font_;
};

}  // namespace

EmojiPickerDialog::EmojiPickerDialog(QWidget *parent) : QDialog(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(360, 260);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; border-radius: 12px; }")
                      .arg(Theme::uiPanelBg().name(), Theme::uiBorder().name()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    view_ = new QListView(this);
    view_->setViewMode(QListView::IconMode);
    view_->setResizeMode(QListView::Adjust);
    view_->setMovement(QListView::Static);
    view_->setUniformItemSizes(true);
    view_->setSpacing(2);
    view_->setGridSize(QSize(32, 32));
    view_->setSelectionMode(QAbstractItemView::NoSelection);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setStyleSheet(
        QStringLiteral(
            "QListView { background: transparent; outline: none; }"
            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }")
            .arg(Theme::uiScrollBarHandle().name(),
                 Theme::uiScrollBarHandleHover().name()));

    model_ = new EmojiListModel(view_);
    view_->setModel(model_);
    view_->setItemDelegate(new EmojiDelegate(view_));

    connect(view_, &QListView::clicked, this, [this](const QModelIndex &index) {
        const QString emoji = index.data(Qt::DisplayRole).toString();
        if (!emoji.isEmpty()) {
            emit emojiSelected(emoji);
        }
    });

    layout->addWidget(view_, 1);
}
