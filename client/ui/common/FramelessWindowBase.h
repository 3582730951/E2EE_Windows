// Base frameless window with drag/resize logic and overlay support.
#pragma once

#include <QFrame>
#include <QPointer>
#include <QWidget>

#include "OverlayWidget.h"

class QVBoxLayout;

class FramelessWindowBase : public QWidget {
    Q_OBJECT

public:
    explicit FramelessWindowBase(QWidget *parent = nullptr);

    bool isEmbedded() const;

    void setCentralWidget(QWidget *widget);
    QWidget *centralWidget() const;
    QWidget *frameWidget() const;

    void setTitleBar(QWidget *widget);
    void setOverlayImage(const QString &path);
    void toggleOverlay();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Qt::Edges hitEdges(const QPoint &pos) const;
    void updateCursorShape(const QPoint &pos);
    void performResize(const QPoint &globalPos);
    bool inTitleBar(const QPoint &globalPos) const;

    QFrame *m_container;
    QVBoxLayout *m_containerLayout;
    QPointer<QWidget> m_centralWidget;
    QPointer<QWidget> m_titleBar;
    OverlayWidget *m_overlay;

    bool m_embedded;
    bool m_dragging;
    bool m_resizing;
    QPoint m_dragOffset;
    QPoint m_pressGlobal;
    QRect m_startGeometry;
    Qt::Edges m_resizeEdges;
};
