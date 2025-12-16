// Simple icon-styled button with hover/pressed colors.
#pragma once

#include <QPushButton>

class IconButton : public QPushButton {
    Q_OBJECT

public:
    explicit IconButton(const QString &glyph = QString(), QWidget *parent = nullptr);

    void setGlyph(const QString &glyph, int pointSize = 10);
    void setSvgIcon(const QString &resourcePath, int size = 16);
    void setRound(bool round);
    void setColors(const QColor &fg, const QColor &hoverFg, const QColor &pressedFg,
                   const QColor &bg = Qt::transparent, const QColor &hoverBg = Qt::transparent,
                   const QColor &pressedBg = Qt::transparent);
    void setPadding(int padding);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_glyph;
    int m_pointSize;
    QString m_svgPath;
    int m_svgSize;
    bool m_round;
    int m_padding;
    QColor m_fg;
    QColor m_hoverFg;
    QColor m_pressedFg;
    QColor m_bg;
    QColor m_hoverBg;
    QColor m_pressedBg;
};
