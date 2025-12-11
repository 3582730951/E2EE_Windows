#include "FramelessWindowBase.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "Theme.h"

FramelessWindowBase::FramelessWindowBase(QWidget *parent)
    : QWidget(parent),
      m_container(new QFrame(this)),
      m_containerLayout(new QVBoxLayout()),
      m_overlay(new OverlayWidget(m_container)),
      m_dragging(false),
      m_resizing(false),
      m_resizeEdges(Qt::Edges()) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(m_container);

    m_container->setObjectName(QStringLiteral("frameContainer"));
    m_container->setLayout(m_containerLayout);
    m_containerLayout->setContentsMargins(0, 0, 0, 0);
    m_containerLayout->setSpacing(0);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28.0);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 180));
    m_container->setGraphicsEffect(shadow);

    setStyleSheet(QStringLiteral(
        "#frameContainer { background-color: %1; border-radius: %2px; }")
                      .arg(Theme::background().name())
                      .arg(Theme::kWindowRadius));

    m_overlay->hide();
    m_overlay->raise();
}

void FramelessWindowBase::setCentralWidget(QWidget *widget) {
    if (m_centralWidget) {
        m_centralWidget->setParent(nullptr);
    }
    m_centralWidget = widget;
    if (m_centralWidget) {
        m_containerLayout->addWidget(m_centralWidget);
    }
}

QWidget *FramelessWindowBase::centralWidget() const { return m_centralWidget; }

QWidget *FramelessWindowBase::frameWidget() const { return m_container; }

void FramelessWindowBase::setTitleBar(QWidget *widget) { m_titleBar = widget; }

void FramelessWindowBase::setOverlayImage(const QString &path) {
    m_overlay->setOverlayImage(path);
}

void FramelessWindowBase::toggleOverlay() { m_overlay->toggle(); }

void FramelessWindowBase::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_resizeEdges = hitEdges(event->pos());
    if (m_resizeEdges != Qt::Edges()) {
        m_resizing = true;
        m_pressGlobal = event->globalPos();
        m_startGeometry = geometry();
        event->accept();
        return;
    }

    if (inTitleBar(event->globalPos())) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FramelessWindowBase::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizing) {
        performResize(event->globalPos());
        return;
    }
    if (m_dragging) {
        move(event->globalPos() - m_dragOffset);
        return;
    }
    updateCursorShape(event->pos());
    QWidget::mouseMoveEvent(event);
}

void FramelessWindowBase::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_resizing = false;
        m_resizeEdges = Qt::Edges();
    }
    QWidget::mouseReleaseEvent(event);
}

void FramelessWindowBase::mouseDoubleClickEvent(QMouseEvent *event) {
    QWidget::mouseDoubleClickEvent(event);
}

void FramelessWindowBase::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_O && !event->isAutoRepeat()) {
        toggleOverlay();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FramelessWindowBase::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_overlay->setGeometry(m_container->rect());
    m_overlay->raise();
}

Qt::Edges FramelessWindowBase::hitEdges(const QPoint &pos) const {
    Qt::Edges edges;
    const QRect r = rect();
    if (pos.x() <= Theme::kResizeBorder) {
        edges |= Qt::LeftEdge;
    } else if (pos.x() >= r.width() - Theme::kResizeBorder) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() <= Theme::kResizeBorder) {
        edges |= Qt::TopEdge;
    } else if (pos.y() >= r.height() - Theme::kResizeBorder) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

void FramelessWindowBase::updateCursorShape(const QPoint &pos) {
    Qt::Edges edges = hitEdges(pos);
    if (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::TopEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::TopEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void FramelessWindowBase::performResize(const QPoint &globalPos) {
    const QPoint delta = globalPos - m_pressGlobal;
    QRect geom = m_startGeometry;
    const int minW = minimumWidth();
    const int minH = minimumHeight();

    if (m_resizeEdges.testFlag(Qt::LeftEdge)) {
        int newWidth = geom.width() - delta.x();
        int newX = geom.x() + delta.x();
        if (newWidth >= minW) {
            geom.setX(newX);
            geom.setWidth(newWidth);
        }
    } else if (m_resizeEdges.testFlag(Qt::RightEdge)) {
        int newWidth = geom.width() + delta.x();
        if (newWidth >= minW) {
            geom.setWidth(newWidth);
        }
    }
    if (m_resizeEdges.testFlag(Qt::TopEdge)) {
        int newHeight = geom.height() - delta.y();
        int newY = geom.y() + delta.y();
        if (newHeight >= minH) {
            geom.setY(newY);
            geom.setHeight(newHeight);
        }
    } else if (m_resizeEdges.testFlag(Qt::BottomEdge)) {
        int newHeight = geom.height() + delta.y();
        if (newHeight >= minH) {
            geom.setHeight(newHeight);
        }
    }
    setGeometry(geom);
}

bool FramelessWindowBase::inTitleBar(const QPoint &globalPos) const {
    if (!m_titleBar) {
        return false;
    }
    const QPoint local = m_titleBar->mapFromGlobal(globalPos);
    return m_titleBar->rect().contains(local);
}
