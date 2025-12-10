#ifndef MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H
#define MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H

#include <QColor>
#include <QEvent>
#include <QDateTime>
#include <QListWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QToolButton>
#include <QVector>

#include "theme.h"

namespace mi::client::ui::widgets {

struct ListEntry {
    QString id;
    QString name;
    QString detail;
    QColor indicator;
    QDateTime lastTime;
    bool isGroup{false};
};

class ListWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ListWindow(const QString& title, const QVector<ListEntry>& entries,
                        const UiPalette& palette = DefaultPalette(),
                        QWidget* parent = nullptr);

signals:
    void entrySelected(const QString& id, bool isGroup, const QString& name);

private:
    void populate();
    QWidget* buildItem(const ListEntry& entry, QWidget* parent);
    QWidget* buildTitleBar(const QString& title, QWidget* parent);
    void refreshSelection();
    bool eventFilter(QObject* watched, QEvent* event) override;

    UiPalette palette_;
    QVector<ListEntry> entries_;
    QListWidget* list_{nullptr};
    QWidget* titleBar_{nullptr};
    QToolButton* btnMin_{nullptr};
    QToolButton* btnMax_{nullptr};
    QToolButton* btnClose_{nullptr};
    QPoint dragPos_;
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_LIST_WINDOW_H
