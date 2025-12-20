#include "Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
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

Scheme ResolveScheme(Scheme schemeValue) {
    if (schemeValue != Scheme::Auto) {
        return schemeValue;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme cs = hints->colorScheme();
        if (cs == Qt::ColorScheme::Dark) {
            return Scheme::Dark;
        }
        if (cs == Qt::ColorScheme::Light) {
            return Scheme::Light;
        }
    }
#endif
    // Fallback: treat unknown as light (safer for readability).
    return Scheme::Light;
}

QColor Pick(Scheme schemeValue, QColor dark, QColor light, QColor highContrast) {
    schemeValue = ResolveScheme(schemeValue);
    switch (schemeValue) {
        case Scheme::Light:
            return light;
        case Scheme::HighContrast:
            return highContrast;
        case Scheme::Dark:
        case Scheme::Auto:
        default:
            return dark;
    }
}

}  // namespace

QFont defaultFont(int pointSize, QFont::Weight weight) {
    QFont font;
    font.setFamilies(QStringList() << QStringLiteral("SF Pro Text")
                                   << QStringLiteral("SF Pro Display")
                                   << QStringLiteral("HarmonyOS Sans")
                                   << QStringLiteral("MiSans")
                                   << QStringLiteral("PingFang SC")
                                   << QStringLiteral("Microsoft YaHei UI")
                                   << QStringLiteral("Segoe UI Variable")
                                   << QStringLiteral("Segoe UI"));
    font.setPointSize(ScalePoints(pointSize));
    font.setWeight(weight);
    return font;
}

Scheme scheme() { return ResolveScheme(gScheme); }

void setScheme(Scheme newScheme) { gScheme = newScheme; }

int fontScalePercent() { return gFontScalePercent; }

void setFontScalePercent(int percent) {
    gFontScalePercent = ClampInt(percent, 50, 200);
}

void ApplyTo(QApplication &app) {
    const Scheme effective = ResolveScheme(gScheme);
    if (effective == Scheme::Dark || effective == Scheme::HighContrast) {
        app.setStyle(QStringLiteral("Fusion"));
    }

    QPalette palette = app.palette();
    if (effective == Scheme::Light) {
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
    } else if (effective == Scheme::HighContrast) {
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
    return Pick(gScheme, QColor(0x0D, 0x0F, 0x12), QColor(0xF7, 0xF8, 0xFA),
                QColor(Qt::black));
}

QColor panel() {
    return Pick(gScheme, QColor(0x15, 0x18, 0x1D), QColor(0xFF, 0xFF, 0xFF),
                QColor(Qt::black));
}

QColor panelLighter() {
    return Pick(gScheme, QColor(0x1C, 0x20, 0x26), QColor(0xFF, 0xFF, 0xFF),
                QColor(0x14, 0x16, 0x1A));
}

QColor outline() {
    return Pick(gScheme, QColor(0x20, 0x24, 0x2B), QColor(0xE1, 0xE5, 0xEB),
                QColor(Qt::white));
}

QColor accentBlue() {
    return Pick(gScheme, QColor(0x0A, 0x84, 0xFF), QColor(0x00, 0x7A, 0xFF),
                QColor(0x00, 0xAE, 0xFF));
}

QColor accentRed() {
    return Pick(gScheme, QColor(0xFF, 0x45, 0x3A), QColor(0xFF, 0x3B, 0x30),
                QColor(0xFF, 0x3B, 0x30));
}

QColor accentOrange() {
    return Pick(gScheme, QColor(0xFF, 0x9F, 0x0A), QColor(0xFF, 0x95, 0x00),
                QColor(0xFF, 0xAA, 0x00));
}

QColor accentGreen() {
    return Pick(gScheme, QColor(0x30, 0xD1, 0x58), QColor(0x34, 0xC7, 0x59),
                QColor(0x00, 0xFF, 0x88));
}

QColor textPrimary() {
    return Pick(gScheme, QColor(0xF2, 0xF3, 0xF5), QColor(0x1C, 0x1C, 0x1E),
                QColor(Qt::white));
}

QColor textSecondary() {
    return Pick(gScheme, QColor(0xC0, 0xC6, 0xD0), QColor(0x5C, 0x63, 0x70),
                QColor(Qt::white));
}

QColor textMuted() {
    return Pick(gScheme, QColor(0x8B, 0x92, 0xA0), QColor(0x8E, 0x95, 0xA3),
                QColor(0xCC, 0xCC, 0xCC));
}

QColor separator() {
    return Pick(gScheme, QColor(0x1F, 0x23, 0x29), QColor(0xE6, 0xE9, 0xEF),
                QColor(Qt::white));
}

QColor bubbleGray() {
    return Pick(gScheme, QColor(0x2C, 0x2F, 0x36), QColor(0xEE, 0xF1, 0xF5),
                QColor(0x10, 0x10, 0x10));
}

QColor uiWindowBg() {
    return Pick(gScheme, QColor(QStringLiteral("#0F1116")),
                QColor(QStringLiteral("#F7F8FA")), QColor(Qt::black));
}

QColor uiPanelBg() {
    return Pick(gScheme, QColor(QStringLiteral("#161B21")),
                QColor(QStringLiteral("#FFFFFF")), QColor(Qt::black));
}

QColor uiSidebarBg() {
    return Pick(gScheme, QColor(QStringLiteral("#11151A")),
                QColor(QStringLiteral("#F9FAFC")), QColor(Qt::black));
}

QColor uiHoverBg() {
    return Pick(gScheme, QColor(QStringLiteral("#1B2027")),
                QColor(QStringLiteral("#EEF1F6")), QColor(QStringLiteral("#101010")));
}

QColor uiSelectedBg() {
    return Pick(gScheme, QColor(QStringLiteral("#222A34")),
                QColor(QStringLiteral("#E6ECF5")), QColor(QStringLiteral("#181818")));
}

QColor uiSearchBg() {
    return Pick(gScheme, QColor(QStringLiteral("#1A1F26")),
                QColor(QStringLiteral("#F0F2F6")), QColor(QStringLiteral("#101010")));
}

QColor uiBorder() {
    return Pick(gScheme, QColor(QStringLiteral("#262C36")),
                QColor(QStringLiteral("#E1E5EB")), QColor(Qt::white));
}

QColor uiTextMain() { return Pick(gScheme, QColor(QStringLiteral("#E7E9ED")), QColor(QStringLiteral("#1C1C1E")), QColor(Qt::white)); }

QColor uiTextSub() { return Pick(gScheme, QColor(QStringLiteral("#B3BAC6")), QColor(QStringLiteral("#5C6370")), QColor(Qt::white)); }

QColor uiTextMuted() { return Pick(gScheme, QColor(QStringLiteral("#7E8694")), QColor(QStringLiteral("#8E95A3")), QColor(0xCC, 0xCC, 0xCC)); }

QColor uiInputBg() {
    return Pick(gScheme, QColor(QStringLiteral("#141922")), QColor(QStringLiteral("#FFFFFF")),
                QColor(Qt::black));
}

QColor uiInputBorder() {
    return Pick(gScheme, QColor(QStringLiteral("#262C36")), QColor(QStringLiteral("#E3E6EC")),
                QColor(Qt::white));
}

QColor uiScrollBarHandle() {
    return Pick(gScheme, QColor(QStringLiteral("#2A303A")), QColor(QStringLiteral("#C4CAD3")),
                QColor(QStringLiteral("#4A4A4A")));
}

QColor uiScrollBarHandleHover() {
    return Pick(gScheme, QColor(QStringLiteral("#37404C")), QColor(QStringLiteral("#B3BAC6")),
                QColor(QStringLiteral("#6A6A6A")));
}

QColor uiMenuBg() {
    return Pick(gScheme, QColor(QStringLiteral("#181C22")), QColor(QStringLiteral("#FFFFFF")),
                QColor(Qt::black));
}

QColor uiTagColor() { return Pick(gScheme, QColor(QStringLiteral("#F06A5C")), QColor(QStringLiteral("#D05A4F")), QColor(0xFF, 0x66, 0x66)); }

QColor uiBadgeRed() { return Pick(gScheme, QColor(QStringLiteral("#FF453A")), QColor(QStringLiteral("#FF3B30")), QColor(0xFF, 0x33, 0x33)); }

QColor uiBadgeGrey() { return Pick(gScheme, QColor(QStringLiteral("#525A66")), QColor(QStringLiteral("#B0B7C3")), QColor(Qt::white)); }

QColor uiAccentBlue() { return Pick(gScheme, QColor(QStringLiteral("#0A84FF")), QColor(QStringLiteral("#007AFF")), QColor(0x00, 0xAE, 0xFF)); }

QColor uiMessageOutgoingBg() { return Pick(gScheme, QColor(QStringLiteral("#0A84FF")), QColor(QStringLiteral("#007AFF")), QColor(QStringLiteral("#0A84FF"))); }

QColor uiMessageIncomingBg() { return Pick(gScheme, QColor(QStringLiteral("#2A2F37")), QColor(QStringLiteral("#EEF1F5")), QColor(QStringLiteral("#101010"))); }

QColor uiMessageText() { return Pick(gScheme, QColor(QStringLiteral("#F2F3F5")), QColor(QStringLiteral("#1C1C1E")), QColor(Qt::white)); }

QColor uiMessageTimeText() { return Pick(gScheme, QColor(QStringLiteral("#8A93A3")), QColor(QStringLiteral("#9BA2B0")), QColor(0xCC, 0xCC, 0xCC)); }

QColor uiMessageSystemText() { return Pick(gScheme, QColor(QStringLiteral("#A1A7B3")), QColor(QStringLiteral("#7A8594")), QColor(Qt::white)); }

QColor uiDangerRed() { return Pick(gScheme, QColor(QStringLiteral("#FF453A")), QColor(QStringLiteral("#FF3B30")), QColor(0xFF, 0x33, 0x33)); }

}  // namespace Theme
