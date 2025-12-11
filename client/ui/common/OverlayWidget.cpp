#include "OverlayWidget.h"

#include <QGraphicsOpacityEffect>
#include <QResizeEvent>

OverlayWidget::OverlayWidget(QWidget *parent) : QWidget(parent), m_label(new QLabel(this)) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setVisible(false);
    m_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto *opacity = new QGraphicsOpacityEffect(this);
    opacity->setOpacity(0.35);
    m_label->setGraphicsEffect(opacity);
}

void OverlayWidget::setOverlayImage(const QString &path) {
    m_path = path;
    m_source = QPixmap(path);
    refreshPixmap();
}

bool OverlayWidget::isOverlayVisible() const { return isVisible(); }

void OverlayWidget::toggle() {
    setVisible(!isVisible());
}

void OverlayWidget::showOverlay() { setVisible(true); }

void OverlayWidget::hideOverlay() { setVisible(false); }

void OverlayWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    refreshPixmap();
}

void OverlayWidget::refreshPixmap() {
    m_label->setGeometry(rect());
    if (m_source.isNull()) {
        m_label->clear();
        return;
    }
    m_label->setPixmap(m_source);
}
