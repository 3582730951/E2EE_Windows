#ifndef MI_E2EE_CLIENT_UI_WIDGETS_THEME_H
#define MI_E2EE_CLIENT_UI_WIDGETS_THEME_H

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QString>

namespace mi::client::ui::widgets {

struct UiPalette {
    QColor background{QStringLiteral("#1a1a1a")};
    QColor panel{QStringLiteral("#20242b")};
    QColor panelMuted{QStringLiteral("#1f1f1f")};
    QColor accent{QStringLiteral("#0066ff")};
    QColor accentHover{QStringLiteral("#1c7dff")};
    QColor textPrimary{QStringLiteral("#ffffff")};
    QColor textSecondary{QStringLiteral("#b8c0cc")};
    QColor bubbleSelf{QStringLiteral("#0f52b6")};
    QColor bubblePeer{QStringLiteral("#2b2f36")};
    QColor danger{QStringLiteral("#ff4d4f")};
};

inline UiPalette DefaultPalette() {
    return UiPalette{};
}

inline QString BuildGlobalStyleSheet(const UiPalette& c = DefaultPalette()) {
    return QStringLiteral(R"(
        QWidget { background: %1; color: %2; font-family: "Microsoft YaHei", "Segoe UI", sans-serif; }
        QDialog, QMainWindow { background: %1; }
        QFrame#Panel, QWidget#Panel { background: %3; border-radius: 12px; border: none; }
        QLineEdit, QComboBox, QTextEdit, QListWidget, QScrollArea { background: %3; border: none; border-radius: 10px; padding: 8px; selection-background-color: %4; selection-color: %2; }
        QLineEdit::focus, QComboBox::focus, QTextEdit::focus { border: 1px solid %4; }
        QToolButton, QPushButton { background: %4; color: %2; border: none; border-radius: 10px; padding: 8px 14px; }
        QPushButton:flat, QToolButton:flat { background: transparent; color: %2; }
        QPushButton:hover, QToolButton:hover { background: %5; }
        QPushButton:disabled, QToolButton:disabled { background: #3a3a3a; color: #8a8a8a; }
        QListWidget::item { padding: 10px; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }
        QScrollBar::handle:vertical { background: #3a3f47; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: %5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )")
        .arg(c.background.name(), c.textPrimary.name(), c.panel.name(), c.accent.name(),
             c.accentHover.name());
}

inline QPixmap BuildAvatar(const QString& text, const QColor& color, int diameter = 40) {
    QPixmap avatar(diameter, diameter);
    avatar.fill(Qt::transparent);
    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, diameter, diameter);

    painter.setPen(Qt::white);
    QFont font(QStringLiteral("Microsoft YaHei"), diameter / 3);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(avatar.rect(), Qt::AlignCenter, text.left(2).toUpper());
    return avatar;
}

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_THEME_H
