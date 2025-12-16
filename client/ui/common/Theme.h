// Lightweight theme constants shared by the UI demos.
#pragma once

#include <QColor>
#include <QFont>

class QApplication;

namespace Theme {

enum class Scheme : int {
    Dark = 0,
    Light = 1,
    HighContrast = 2,
};

QFont defaultFont(int pointSize = 10, QFont::Weight weight = QFont::Normal);

Scheme scheme();
void setScheme(Scheme scheme);

int fontScalePercent();
void setFontScalePercent(int percent);

void ApplyTo(QApplication &app);

QColor background();
QColor panel();
QColor panelLighter();
QColor outline();
QColor accentBlue();
QColor accentRed();
QColor accentOrange();
QColor accentGreen();
QColor textPrimary();
QColor textSecondary();
QColor textMuted();
QColor separator();
QColor bubbleGray();

QColor uiWindowBg();
QColor uiPanelBg();
QColor uiSidebarBg();
QColor uiHoverBg();
QColor uiSelectedBg();
QColor uiSearchBg();
QColor uiBorder();
QColor uiTextMain();
QColor uiTextSub();
QColor uiTextMuted();
QColor uiInputBg();
QColor uiInputBorder();
QColor uiScrollBarHandle();
QColor uiScrollBarHandleHover();
QColor uiMenuBg();
QColor uiTagColor();
QColor uiBadgeRed();
QColor uiBadgeGrey();
QColor uiAccentBlue();
QColor uiMessageOutgoingBg();
QColor uiMessageIncomingBg();
QColor uiMessageText();
QColor uiMessageTimeText();
QColor uiMessageSystemText();
QColor uiDangerRed();

constexpr int kTitleBarHeight = 56;
constexpr int kResizeBorder = 6;
constexpr int kWindowRadius = 10;

}  // namespace Theme
