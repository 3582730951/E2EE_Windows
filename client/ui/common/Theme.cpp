#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStringList>

#include <algorithm>

namespace Theme {

namespace {

Scheme gScheme = Scheme::Dark;
int gFontScalePercent = 100;

int ClampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

int ScalePoints(int pt) {
    const int scale = ClampInt(gFontScalePercent, 50, 200);
    const int scaled = (pt * scale) / 100;
    return std::max(6, scaled);
}

QColor Pick(Scheme schemeValue, QColor dark, QColor light, QColor highContrast) {
    switch (schemeValue) {
        case Scheme::Light:
            return light;
        case Scheme::HighContrast:
            return highContrast;
        case Scheme::Dark:
        default:
            return dark;
    }
}

}  // namespace

QFont defaultFont(int pointSize, QFont::Weight weight) {
    QFont font;
    font.setFamilies(QStringList() << QStringLiteral("Microsoft YaHei UI")
                                   << QStringLiteral("PingFang SC")
                                   << QStringLiteral("Segoe UI"));
    font.setPointSize(ScalePoints(pointSize));
    font.setWeight(weight);
    return font;
}

Scheme scheme() { return gScheme; }

void setScheme(Scheme newScheme) { gScheme = newScheme; }

int fontScalePercent() { return gFontScalePercent; }

void setFontScalePercent(int percent) {
    gFontScalePercent = ClampInt(percent, 50, 200);
}

void ApplyTo(QApplication &app) {
    if (gScheme == Scheme::HighContrast) {
        app.setStyle(QStringLiteral("Fusion"));
    }

    QPalette palette = app.palette();
    if (gScheme == Scheme::Light) {
        palette.setColor(QPalette::Window, uiWindowBg());
        palette.setColor(QPalette::Base, uiPanelBg());
        palette.setColor(QPalette::AlternateBase, uiSearchBg());
        palette.setColor(QPalette::Button, uiPanelBg());
        palette.setColor(QPalette::Text, uiTextMain());
        palette.setColor(QPalette::WindowText, uiTextMain());
        palette.setColor(QPalette::ButtonText, uiTextMain());
        palette.setColor(QPalette::Highlight, uiAccentBlue());
        palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
        palette.setColor(QPalette::Link, uiAccentBlue());
    } else if (gScheme == Scheme::HighContrast) {
        palette.setColor(QPalette::Window, QColor(Qt::black));
        palette.setColor(QPalette::Base, QColor(Qt::black));
        palette.setColor(QPalette::AlternateBase, QColor(0x10, 0x10, 0x10));
        palette.setColor(QPalette::Button, QColor(Qt::black));
        palette.setColor(QPalette::Text, QColor(Qt::white));
        palette.setColor(QPalette::WindowText, QColor(Qt::white));
        palette.setColor(QPalette::ButtonText, QColor(Qt::white));
        palette.setColor(QPalette::Highlight, uiAccentBlue());
        palette.setColor(QPalette::HighlightedText, QColor(Qt::black));
        palette.setColor(QPalette::Link, uiAccentBlue());
    } else {
        palette.setColor(QPalette::Window, uiWindowBg());
        palette.setColor(QPalette::Base, uiPanelBg());
        palette.setColor(QPalette::AlternateBase, uiSearchBg());
        palette.setColor(QPalette::Button, uiPanelBg());
        palette.setColor(QPalette::Text, uiTextMain());
        palette.setColor(QPalette::WindowText, uiTextMain());
        palette.setColor(QPalette::ButtonText, uiTextMain());
        palette.setColor(QPalette::Highlight, uiAccentBlue());
        palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
        palette.setColor(QPalette::Link, uiAccentBlue());
    }
    app.setPalette(palette);
}

QColor background() {
    return Pick(gScheme, QColor(0x12, 0x12, 0x12), QColor(0xF4, 0xF6, 0xF8),
                QColor(Qt::black));
}

QColor panel() {
    return Pick(gScheme, QColor(0x18, 0x18, 0x18), QColor(0xFF, 0xFF, 0xFF),
                QColor(Qt::black));
}

QColor panelLighter() {
    return Pick(gScheme, QColor(0x20, 0x20, 0x20), QColor(0xFF, 0xFF, 0xFF),
                QColor(0x10, 0x10, 0x10));
}

QColor outline() {
    return Pick(gScheme, QColor(0x2A, 0x2A, 0x2A), QColor(0xD7, 0xDC, 0xE3),
                QColor(Qt::white));
}

QColor accentBlue() {
    return Pick(gScheme, QColor(0x3A, 0x8D, 0xFF), QColor(0x2F, 0x81, 0xE8),
                QColor(0x00, 0xAE, 0xFF));
}

QColor accentRed() {
    return Pick(gScheme, QColor(0xF0, 0x5C, 0x5C), QColor(0xD9, 0x4E, 0x4E),
                QColor(0xFF, 0x33, 0x33));
}

QColor accentOrange() {
    return Pick(gScheme, QColor(0xF2, 0x8C, 0x48), QColor(0xF2, 0x8C, 0x48),
                QColor(0xFF, 0xAA, 0x00));
}

QColor accentGreen() {
    return Pick(gScheme, QColor(0x3F, 0xBF, 0x7F), QColor(0x35, 0xB0, 0x74),
                QColor(0x00, 0xFF, 0x88));
}

QColor textPrimary() {
    return Pick(gScheme, QColor(0xF5, 0xF5, 0xF5), QColor(0x1A, 0x1C, 0x1F),
                QColor(Qt::white));
}

QColor textSecondary() {
    return Pick(gScheme, QColor(0xB8, 0xB8, 0xB8), QColor(0x4F, 0x59, 0x65),
                QColor(Qt::white));
}

QColor textMuted() {
    return Pick(gScheme, QColor(0x7A, 0x7A, 0x7A), QColor(0x7C, 0x85, 0x92),
                QColor(0xCC, 0xCC, 0xCC));
}

QColor separator() {
    return Pick(gScheme, QColor(0x24, 0x24, 0x24), QColor(0xE3, 0xE9, 0xF2),
                QColor(Qt::white));
}

QColor bubbleGray() {
    return Pick(gScheme, QColor(0x28, 0x28, 0x28), QColor(0xEE, 0xF2, 0xF6),
                QColor(0x10, 0x10, 0x10));
}

QColor uiWindowBg() {
    return Pick(gScheme, QColor(QStringLiteral("#14161A")),
                QColor(QStringLiteral("#F4F6F8")), QColor(Qt::black));
}

QColor uiPanelBg() {
    return Pick(gScheme, QColor(QStringLiteral("#191C20")),
                QColor(QStringLiteral("#FFFFFF")), QColor(Qt::black));
}

QColor uiSidebarBg() {
    return Pick(gScheme, QColor(QStringLiteral("#1D2025")),
                QColor(QStringLiteral("#FFFFFF")), QColor(Qt::black));
}

QColor uiHoverBg() {
    return Pick(gScheme, QColor(QStringLiteral("#20242A")),
                QColor(QStringLiteral("#EEF2F6")), QColor(QStringLiteral("#101010")));
}

QColor uiSelectedBg() {
    return Pick(gScheme, QColor(QStringLiteral("#262B32")),
                QColor(QStringLiteral("#E3E9F2")), QColor(QStringLiteral("#181818")));
}

QColor uiSearchBg() {
    return Pick(gScheme, QColor(QStringLiteral("#1F2227")),
                QColor(QStringLiteral("#EEF2F6")), QColor(QStringLiteral("#101010")));
}

QColor uiBorder() {
    return Pick(gScheme, QColor(QStringLiteral("#1E2025")),
                QColor(QStringLiteral("#D7DCE3")), QColor(Qt::white));
}

QColor uiTextMain() { return Pick(gScheme, QColor(QStringLiteral("#F0F2F5")), QColor(QStringLiteral("#1A1C1F")), QColor(Qt::white)); }

QColor uiTextSub() { return Pick(gScheme, QColor(QStringLiteral("#A9ADB3")), QColor(QStringLiteral("#4F5965")), QColor(Qt::white)); }

QColor uiTextMuted() { return Pick(gScheme, QColor(QStringLiteral("#7C8087")), QColor(QStringLiteral("#7C8592")), QColor(0xCC, 0xCC, 0xCC)); }

QColor uiInputBg() {
    return Pick(gScheme, QColor(QStringLiteral("#181B1F")), QColor(QStringLiteral("#FFFFFF")),
                QColor(Qt::black));
}

QColor uiInputBorder() {
    return Pick(gScheme, QColor(QStringLiteral("#1F2025")), QColor(QStringLiteral("#D7DCE3")),
                QColor(Qt::white));
}

QColor uiScrollBarHandle() {
    return Pick(gScheme, QColor(QStringLiteral("#2A2D33")), QColor(QStringLiteral("#C9D0DA")),
                QColor(QStringLiteral("#4A4A4A")));
}

QColor uiScrollBarHandleHover() {
    return Pick(gScheme, QColor(QStringLiteral("#343842")), QColor(QStringLiteral("#B8C0CC")),
                QColor(QStringLiteral("#6A6A6A")));
}

QColor uiMenuBg() {
    return Pick(gScheme, QColor(QStringLiteral("#1B1E22")), QColor(QStringLiteral("#FFFFFF")),
                QColor(Qt::black));
}

QColor uiTagColor() { return Pick(gScheme, QColor(QStringLiteral("#E36A5C")), QColor(QStringLiteral("#C43D2E")), QColor(0xFF, 0x66, 0x66)); }

QColor uiBadgeRed() { return Pick(gScheme, QColor(QStringLiteral("#D74D4D")), QColor(QStringLiteral("#D74D4D")), QColor(0xFF, 0x33, 0x33)); }

QColor uiBadgeGrey() { return Pick(gScheme, QColor(QStringLiteral("#464A50")), QColor(QStringLiteral("#7C8592")), QColor(Qt::white)); }

QColor uiAccentBlue() { return Pick(gScheme, QColor(QStringLiteral("#5D8CFF")), QColor(QStringLiteral("#2F81E8")), QColor(0x00, 0xAE, 0xFF)); }

QColor uiMessageOutgoingBg() { return Pick(gScheme, QColor(QStringLiteral("#3A3D40")), QColor(QStringLiteral("#DDE7F7")), QColor(QStringLiteral("#101010"))); }

QColor uiMessageIncomingBg() { return Pick(gScheme, QColor(QStringLiteral("#2F3235")), QColor(QStringLiteral("#EEF2F6")), QColor(QStringLiteral("#101010"))); }

QColor uiMessageText() { return Pick(gScheme, QColor(QStringLiteral("#E6E6E6")), QColor(QStringLiteral("#1A1C1F")), QColor(Qt::white)); }

QColor uiMessageTimeText() { return Pick(gScheme, QColor(QStringLiteral("#6E737A")), QColor(QStringLiteral("#7C8592")), QColor(0xCC, 0xCC, 0xCC)); }

QColor uiMessageSystemText() { return Pick(gScheme, QColor(QStringLiteral("#9A9FA6")), QColor(QStringLiteral("#4F5965")), QColor(Qt::white)); }

QColor uiDangerRed() { return Pick(gScheme, QColor(QStringLiteral("#D95C5C")), QColor(QStringLiteral("#D74D4D")), QColor(0xFF, 0x33, 0x33)); }

}  // namespace Theme
