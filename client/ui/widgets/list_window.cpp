#include "list_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ListWindow::ListWindow(const QString& title, const QVector<ListEntry>& entries,
                       const UiPalette& palette, QWidget* parent)
    : QMainWindow(parent), palette_(palette), entries_(entries) {
    setWindowTitle(title);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    resize(360, 520);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(10, 12, 16, 12);
    root->setSpacing(10);

    titleBar_ = buildTitleBar(title, central);
    root->addWidget(titleBar_);

    list_ = new QListWidget(central);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setSpacing(8);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(list_, 1);

    setCentralWidget(central);

    populate();

    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        const QString id = item->data(Qt::UserRole).toString();
        const bool isGroup = item->data(Qt::UserRole + 1).toBool();
        const QString name = item->data(Qt::UserRole + 2).toString();
        emit entrySelected(id, isGroup, name);
    });
}

void ListWindow::populate() {
    if (!list_) {
        return;
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const ListEntry& a, const ListEntry& b) { return a.lastTime > b.lastTime; });

    for (const auto& entry : entries_) {
        auto* item = new QListWidgetItem(list_);
        item->setSizeHint(QSize(320, 64));
        item->setData(Qt::UserRole, entry.id);
        item->setData(Qt::UserRole + 1, entry.isGroup);
        item->setData(Qt::UserRole + 2, entry.name);
        list_->addItem(item);
        list_->setItemWidget(item, buildItem(entry, list_));
    }
}

QWidget* ListWindow::buildItem(const ListEntry& entry, QWidget* parent) {
    auto* item = new QWidget(parent);
    auto* row = new QHBoxLayout(item);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(10);

    auto* indicator = new QLabel(item);
    indicator->setFixedSize(10, 10);
    indicator->setStyleSheet(QStringLiteral("background:%1; border-radius:5px;")
                                 .arg(entry.indicator.name()));
    row->addWidget(indicator, 0, Qt::AlignVCenter);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto* name = new QLabel(entry.name, item);
    name->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                            .arg(palette_.textPrimary.name()));
    textCol->addWidget(name);

    auto* detail = new QLabel(entry.detail, item);
    detail->setStyleSheet(QStringLiteral("color:%1; font-size:12px;")
                              .arg(palette_.textSecondary.name()));
    textCol->addWidget(detail);

    row->addLayout(textCol, 1);

    auto* timeLabel =
        new QLabel(entry.lastTime.isValid() ? entry.lastTime.toString(QStringLiteral("hh:mm"))
                                            : QStringLiteral("--:--"),
                   item);
    timeLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;").arg(palette_.textSecondary.name()));
    row->addWidget(timeLabel, 0, Qt::AlignVCenter);

    return item;
}

QWidget* ListWindow::buildTitleBar(const QString& title, QWidget* parent) {
    auto* bar = new QWidget(parent);
    bar->setObjectName(QStringLiteral("ListTitleBar"));
    bar->setStyleSheet(QStringLiteral("QWidget#ListTitleBar { background:%1; border-radius:10px; }")
                           .arg(palette_.panel.name()));
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 18, 8);
    layout->setSpacing(8);

    auto* lbl = new QLabel(title, bar);
    lbl->setStyleSheet(QStringLiteral("color:%1; font-weight:700; font-size:14px;")
                           .arg(palette_.textPrimary.name()));
    layout->addWidget(lbl, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    auto makeBtn = [&](const QString& text) {
        auto* btn = new QToolButton(bar);
        btn->setText(text);
        btn->setFixedSize(22, 22);
        btn->setStyleSheet(QStringLiteral("background:%1; color:%2; border:none; border-radius:11px; font-weight:700;")
                               .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    btnMin_ = makeBtn(QStringLiteral("-"));
    btnMax_ = makeBtn(QStringLiteral("□"));
    btnClose_ = makeBtn(QStringLiteral("×"));
    layout->addWidget(btnMin_);
    layout->addWidget(btnMax_);
    layout->addWidget(btnClose_);

    connect(btnMin_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            window()->showMinimized();
        }
    });
    connect(btnMax_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            if (window()->isMaximized()) {
                window()->showNormal();
            } else {
                window()->showMaximized();
            }
        }
    });
    connect(btnClose_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            window()->close();
        }
    });

    bar->installEventFilter(this);
    return bar;
}

bool ListWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == titleBar_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                dragPos_ = me->globalPosition().toPoint();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                const QPoint delta = me->globalPosition().toPoint() - dragPos_;
                dragPos_ = me->globalPosition().toPoint();
                if (window()) {
                    window()->move(window()->pos() + delta);
                }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

}  // namespace mi::client::ui::widgets
