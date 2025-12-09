#include "navigation_button.h"

#include <QResizeEvent>
#include <QStyle>

namespace mi::client::ui::widgets {

NavigationButton::NavigationButton(const QString& text, const QIcon& icon,
                                   QWidget* parent)
    : QToolButton(parent) {
    setText(text);
    setIcon(icon);
    setIconSize(QSize(22, 22));
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    setAutoExclusive(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(72);
    setCursor(Qt::PointingHandCursor);

    badge_ = new QLabel(this);
    badge_->setFixedSize(12, 12);
    badge_->setStyleSheet(
        "background:#ff4d4f; border-radius:6px; border:1px solid #1a1a1a;");
    badge_->hide();
}

void NavigationButton::setUnreadCount(int count) {
    if (!badge_) {
        return;
    }
    badge_->setVisible(count > 0);
}

void NavigationButton::resizeEvent(QResizeEvent* event) {
    QToolButton::resizeEvent(event);
    if (!badge_ || !badge_->isVisible()) {
        return;
    }
    const int x = width() - badge_->width() - 8;
    const int y = 8;
    badge_->move(x, y);
}

}  // namespace mi::client::ui::widgets
