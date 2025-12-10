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
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(QStringLiteral("background:transparent;"));
    resize(360, 520);

    auto* central = new QWidget(this);
    central->setStyleSheet(QStringLiteral("background:transparent;"));
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* shell = new QFrame(central);
    shell->setObjectName(QStringLiteral("ListShell"));
    shell->setStyleSheet(QStringLiteral(
        "QFrame#ListShell { background:%1; border-radius:14px; border:none; }")
                             .arg(QStringLiteral("#101018")));
    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(10, 10, 10, 10);
    shellLayout->setSpacing(8);

    titleBar_ = buildTitleBar(title, shell);
    shellLayout->addWidget(titleBar_);

    list_ = new QListWidget(shell);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setSpacing(8);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setStyleSheet(QStringLiteral(
        "QListWidget { background:transparent; border:none; outline:0; padding:2px; }"
        "QListWidget::item { margin:0; }"
        "QScrollBar:vertical { background:transparent; width:6px; margin:4px 0; }"
        "QScrollBar::handle:vertical { background:#3a3f47; border-radius:3px; min-height:30px; }"
        "QScrollBar::handle:vertical:hover { background:%1; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }")
                           .arg(palette_.accentHover.name()));
    shellLayout->addWidget(list_, 1);

    root->addWidget(shell);

    setCentralWidget(central);

    populate();
    refreshSelection();

    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        const QString id = item->data(Qt::UserRole).toString();
        const bool isGroup = item->data(Qt::UserRole + 1).toBool();
        const QString name = item->data(Qt::UserRole + 2).toString();
        emit entrySelected(id, isGroup, name);
    });
    connect(list_, &QListWidget::itemSelectionChanged, this, &ListWindow::refreshSelection);
}

void ListWindow::populate() {
    if (!list_) {
        return;
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const ListEntry& a, const ListEntry& b) { return a.lastTime > b.lastTime; });

    for (const auto& entry : entries_) {
        auto* item = new QListWidgetItem(list_);
        item->setSizeHint(QSize(320, 68));
        item->setData(Qt::UserRole, entry.id);
        item->setData(Qt::UserRole + 1, entry.isGroup);
        item->setData(Qt::UserRole + 2, entry.name);
        list_->addItem(item);
        list_->setItemWidget(item, buildItem(entry, list_));
    }
}

QWidget* ListWindow::buildItem(const ListEntry& entry, QWidget* parent) {
    auto* item = new QWidget(parent);
    item->setObjectName(QStringLiteral("ListItem"));
    const QString baseBg = QStringLiteral("#12121c");
    item->setStyleSheet(QStringLiteral(
        "QWidget#ListItem { background:%1; border-radius:10px; border:1px solid %2; }"
        "QWidget#ListItem[selected=\"true\"] { background:%3; border:1px solid %3; }")
                            .arg(baseBg, palette_.border.name(), palette_.accent.name()));

    auto* row = new QHBoxLayout(item);
    row->setContentsMargins(10, 10, 10, 10);
    row->setSpacing(10);

    auto* indicator = new QLabel(item);
    indicator->setFixedSize(12, 12);
    indicator->setStyleSheet(QStringLiteral("background:%1; border-radius:6px;")
                                 .arg(entry.indicator.name()));
    row->addWidget(indicator, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto* name = new QLabel(entry.name, item);
    name->setStyleSheet(QStringLiteral("color:%1; font-weight:700; font-size:13px;")
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
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(6);

    auto* lbl = new QLabel(title, bar);
    lbl->setStyleSheet(QStringLiteral("color:%1; font-weight:700; font-size:14px;")
                           .arg(palette_.textPrimary.name()));
    layout->addWidget(lbl, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    auto makeBtn = [&](const QString& text) {
        auto* btn = new QToolButton(bar);
        btn->setText(text);
        btn->setFixedSize(18, 18);
        btn->setStyleSheet(QStringLiteral(
                               "background:%1; color:%2; border:none; border-radius:9px; font-weight:900; font-size:10px; padding:0;")
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
