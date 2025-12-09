#ifndef MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H

#include <QColor>
#include <QDateTime>
#include <QListWidget>
#include <QMainWindow>
#include <QVector>

#include "theme.h"

namespace mi::client::ui::widgets {

struct ListEntry {
    QString id;
    QString name;
    QString detail;
    QColor indicator;
    QDateTime lastTime;
};

class ListWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ListWindow(const QString& title, const QVector<ListEntry>& entries,
                        const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr);

signals:
    void entrySelected(const QString& name);

private:
    void populate();
    QWidget* buildItem(const ListEntry& entry, QWidget* parent);

    UiPalette palette_;
    QVector<ListEntry> entries_;
    QListWidget* list_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H
