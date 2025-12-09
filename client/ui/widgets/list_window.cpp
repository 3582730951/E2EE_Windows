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
    resize(360, 520);

    auto* central = new QWidget(this);
    central->setAutoFillBackground(true);
    QPalette pal = central->palette();
    pal.setColor(QPalette::Window, palette_.background);
    central->setPalette(pal);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto* panel = new QFrame(central);
    panel->setObjectName(QStringLiteral("Panel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* heading = new QLabel(title, panel);
    heading->setStyleSheet(
        QStringLiteral("color:%1; font-weight:700; font-size:16px;")
            .arg(palette_.textPrimary.name()));
    layout->addWidget(heading);

    list_ = new QListWidget(panel);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setSpacing(8);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(list_, 1);

    root->addWidget(panel);
    setCentralWidget(central);

    populate();

    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        emit entrySelected(item->data(Qt::UserRole).toString());
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
        item->setData(Qt::UserRole, entry.name);
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

}  // namespace mi::client::ui::widgets
