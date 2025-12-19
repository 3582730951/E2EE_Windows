// Delegate to draw chat bubbles, time dividers, and system messages.
#pragma once

#include <QStyledItemDelegate>

class MessageDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit MessageDelegate(QObject *parent = nullptr);

    void setHighlightedRow(int row);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QSize bubbleSize(const QString &text, const QFont &font, int maxWidth) const;

    int highlightedRow_{-1};
};
