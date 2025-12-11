#include "IconButton.h"

#include <QEnterEvent>
#include <QPainter>

IconButton::IconButton(const QString &glyph, QWidget *parent)
    : QPushButton(parent),
      m_glyph(glyph),
      m_pointSize(10),
      m_round(false),
      m_padding(6),
      m_fg(Qt::white),
      m_hoverFg(Qt::white),
      m_pressedFg(Qt::white),
      m_bg(Qt::transparent),
      m_hoverBg(QColor(255, 255, 255, 30)),
      m_pressedBg(QColor(255, 255, 255, 50)) {
    setFlat(true);
    setCheckable(false);
    setCursor(Qt::PointingHandCursor);
    setGlyph(glyph, 10);
}

void IconButton::setGlyph(const QString &glyph, int pointSize) {
    m_glyph = glyph;
    m_pointSize = pointSize;
    QFont f = font();
    f.setPointSize(pointSize);
    setFont(f);
    update();
}

void IconButton::setRound(bool round) {
    m_round = round;
    update();
}

void IconButton::setPadding(int padding) {
    m_padding = padding;
    update();
}

void IconButton::setColors(const QColor &fg, const QColor &hoverFg, const QColor &pressedFg,
                           const QColor &bg, const QColor &hoverBg, const QColor &pressedBg) {
    m_fg = fg;
    m_hoverFg = hoverFg;
    m_pressedFg = pressedFg;
    m_bg = bg;
    m_hoverBg = hoverBg;
    m_pressedBg = pressedBg;
    update();
}

void IconButton::enterEvent(QEnterEvent *event) {
    QPushButton::enterEvent(event);
    update();
}

void IconButton::leaveEvent(QEvent *event) {
    QPushButton::leaveEvent(event);
    update();
}

void IconButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor bg = m_bg;
    QColor fg = m_fg;
    if (!isEnabled()) {
        fg.setAlphaF(0.35);
        bg.setAlphaF(bg.alphaF() * 0.35);
    } else if (isDown()) {
        fg = m_pressedFg;
        bg = m_pressedBg;
    } else if (underMouse()) {
        fg = m_hoverFg;
        bg = m_hoverBg;
    }

    QRect r = rect().adjusted(m_padding, m_padding, -m_padding, -m_padding);
    if (bg.alpha() > 0) {
        const int radius = m_round ? qMin(r.width(), r.height()) / 2 : 6;
        painter.setBrush(bg);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect(), radius, radius);
    }

    painter.setPen(fg);
    painter.drawText(r, Qt::AlignCenter, m_glyph);
}
