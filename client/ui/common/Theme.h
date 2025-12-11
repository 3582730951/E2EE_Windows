// Lightweight theme constants shared by the UI demos.
#pragma once

#include <QColor>
#include <QFont>
#include <QStringList>

namespace Theme {

inline QFont defaultFont(int pointSize = 10, QFont::Weight weight = QFont::Normal) {
    QFont font;
    font.setFamilies(QStringList() << QStringLiteral("Microsoft YaHei UI")
                                   << QStringLiteral("PingFang SC")
                                   << QStringLiteral("Segoe UI"));
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

inline QColor background() { return QColor(0x12, 0x12, 0x12); }
inline QColor panel() { return QColor(0x18, 0x18, 0x18); }
inline QColor panelLighter() { return QColor(0x20, 0x20, 0x20); }
inline QColor outline() { return QColor(0x2A, 0x2A, 0x2A); }
inline QColor accentBlue() { return QColor(0x3A, 0x8D, 0xFF); }
inline QColor accentRed() { return QColor(0xF05C5C); }
inline QColor accentOrange() { return QColor(0xF28C48); }
inline QColor accentGreen() { return QColor(0x3FBF7F); }
inline QColor textPrimary() { return QColor(0xF5, 0xF5, 0xF5); }
inline QColor textSecondary() { return QColor(0xB8, 0xB8, 0xB8); }
inline QColor textMuted() { return QColor(0x7A, 0x7A, 0x7A); }
inline QColor separator() { return QColor(0x24, 0x24, 0x24); }
inline QColor bubbleGray() { return QColor(0x28, 0x28, 0x28); }

constexpr int kTitleBarHeight = 56;
constexpr int kResizeBorder = 6;
constexpr int kWindowRadius = 10;

}  // namespace Theme
