#include "OverlayWidget.h"

#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QResizeEvent>

#include "UiRuntimePaths.h"

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
    QString resolved = path;
    if (!QFileInfo::exists(resolved)) {
        const QString name = QFileInfo(path).fileName();
        if (!name.isEmpty()) {
            const QString baseDir = UiRuntimePaths::AppRootDir();
            if (!baseDir.isEmpty()) {
                const QString candidate = baseDir + QStringLiteral("/assets/ref/") + name;
                if (QFileInfo::exists(candidate)) {
                    resolved = candidate;
                }
            }
        }
    }
    m_source = QPixmap(resolved);
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
