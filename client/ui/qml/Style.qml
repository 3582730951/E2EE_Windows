pragma Singleton
import QtQuick 2.15
import Qt.labs.settings 1.1

Item {
    id: style
    property string fontFamily: "Segoe UI Variable"

    Settings {
        id: styleSettings
        category: "ui_style"
        property string storedThemeMode: "system"
    }

    SystemPalette {
        id: systemPalette
    }

    property string themeMode: (styleSettings.storedThemeMode === "dark" ||
                                styleSettings.storedThemeMode === "light" ||
                                styleSettings.storedThemeMode === "system")
                               ? styleSettings.storedThemeMode
                               : "system"
    readonly property bool systemDark: {
        var c = systemPalette.window
        var luminance = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        return luminance < 0.55
    }
    property bool isDark: themeMode === "dark" || (themeMode === "system" && systemDark)

    onThemeModeChanged: {
        if (styleSettings.storedThemeMode !== themeMode) {
            styleSettings.storedThemeMode = themeMode
        }
    }

    property color windowBg: isDark ? "#0E141C" : "#F4F7FB"
    property color panelBg: isDark ? "#151F2A" : "#FFFFFF"
    property color panelBgAlt: isDark ? "#1A2632" : "#F7FAFD"
    property color panelBgRaised: isDark ? "#20303D" : "#FFFFFF"
    property color hoverBg: isDark ? "#243341" : "#ECF2F8"
    property color pressedBg: isDark ? "#2C3D4D" : "#DEE8F2"
    property color borderSubtle: isDark ? "#2A3C4D" : "#D6E0EA"
    property color borderStrong: isDark ? "#3A4E60" : "#C6D2DE"
    property color textPrimary: isDark ? "#EAF1F8" : "#26323D"
    property color textSecondary: isDark ? "#C8D8E8" : "#637384"
    property color textMuted: isDark ? "#9FB2C6" : "#8A9AAA"
    property color iconMuted: isDark ? "#9FB2C6" : "#657585"
    property color iconActive: isDark ? "#F3F8FD" : "#26323D"
    property color accent: "#4B89FF"
    property color accentHover: "#5A95FF"
    property color accentPressed: "#3D78E7"
    property color accentSoft: isDark ? "#A9C9FF" : "#3D77D4"
    property color link: isDark ? "#85B8FF" : "#2E72DE"
    property color danger: "#E76479"
    property color success: "#21BE8B"
    property color warning: "#D6A25A"

    property color shellGradientTop: isDark ? "#131C25" : "#F0F5FA"
    property color shellGradientBottom: isDark ? "#192430" : "#E8EFF7"
    property color shellSurface: isDark ? Qt.rgba(19 / 255, 29 / 255, 40 / 255, 0.96) : "#FFFFFF"
    property color tgCloudTop: isDark ? "#121A23" : "#F3F7FC"
    property color tgCloudBottom: isDark ? "#1B2734" : "#EAF1F9"
    property color tgGlassSurface: isDark ? Qt.rgba(20 / 255, 31 / 255, 44 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.96)
    property color tgCardHighlight: isDark ? Qt.rgba(148 / 255, 196 / 255, 1.0, 0.12) : Qt.rgba(43 / 255, 135 / 255, 255 / 255, 0.07)
    property color tgCardBorder: isDark ? Qt.rgba(148 / 255, 192 / 255, 1.0, 0.20) : Qt.rgba(43 / 255, 135 / 255, 255 / 255, 0.12)
    property color tgOnlineDot: "#31C38B"
    property color tgMutedBadge: isDark ? "#415264" : "#B8C3CF"
    property color tgUnreadBadge: "#4E8FFF"
    property color tgActiveRowBg: isDark ? Qt.rgba(71 / 255, 110 / 255, 155 / 255, 0.30) : Qt.rgba(78 / 255, 143 / 255, 255 / 255, 0.15)
    property color tgActiveRowBorder: isDark ? Qt.rgba(136 / 255, 183 / 255, 1.0, 0.33) : Qt.rgba(78 / 255, 143 / 255, 255 / 255, 0.24)
    property color railBg: isDark ? "#17232F" : "#FFFFFF"
    property color railHeaderBg: isDark ? "#1D2B39" : "#F7FAFD"
    property color railCardBg: isDark ? "#17232F" : "#FFFFFF"
    property color railAccentBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.16) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.08)
    property color railAccentBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.24) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.14)
    property color topBarBg: isDark ? "#1A2632" : "#FFFFFF"
    property color topBarPillBg: isDark ? "#253444" : "#EEF4FA"
    property color topBarPillBorder: isDark ? "#33485B" : "#D9E4EF"

    property color authBackdropTop: isDark ? "#111B25" : "#F4F8FC"
    property color authBackdropBottom: isDark ? "#162330" : "#EAF1F8"
    property color authGlowPrimary: isDark ? Qt.rgba(0.30, 0.54, 1.0, 0.12) : Qt.rgba(0.30, 0.54, 1.0, 0.10)
    property color authGlowSecondary: isDark ? Qt.rgba(0.13, 0.75, 0.56, 0.08) : Qt.rgba(0.13, 0.75, 0.56, 0.06)
    property color authCardBg: isDark ? Qt.rgba(21 / 255, 32 / 255, 45 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.96)
    property color authCardBorder: isDark ? Qt.rgba(159 / 255, 187 / 255, 214 / 255, 0.12) : Qt.rgba(111 / 255, 139 / 255, 170 / 255, 0.18)
    property color authTitleBarBg: panelBg
    property color authTitleBarBorder: borderSubtle
    property color authTitleBarText: textPrimary
    property color authTitleChipBg: isDark ? Qt.rgba(16 / 255, 25 / 255, 37 / 255, 0.90) : Qt.rgba(1, 1, 1, 0.92)
    property color authTitleChipBorder: isDark ? Qt.rgba(159 / 255, 187 / 255, 214 / 255, 0.22) : Qt.rgba(111 / 255, 139 / 255, 170 / 255, 0.20)
    property color authSurface: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.09, 0.19, 0.30, 0.03)
    property color authSurfaceStrong: isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0.09, 0.19, 0.30, 0.06)
    property color authContextBg: isDark ? Qt.rgba(12 / 255, 19 / 255, 30 / 255, 0.52) : Qt.rgba(236 / 255, 242 / 255, 249 / 255, 0.82)
    property color authContextBorder: isDark ? Qt.rgba(159 / 255, 187 / 255, 214 / 255, 0.10) : Qt.rgba(111 / 255, 139 / 255, 170 / 255, 0.14)
    property color authPanelHeaderBg: isDark ? Qt.rgba(16 / 255, 25 / 255, 37 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.96)
    property color authPanelFooterBg: isDark ? Qt.rgba(9 / 255, 14 / 255, 22 / 255, 0.82) : Qt.rgba(246 / 255, 250 / 255, 253 / 255, 0.96)
    property color authFieldBg: isDark ? Qt.rgba(11 / 255, 18 / 255, 26 / 255, 0.96) : "#FFFFFF"
    property color authFieldBorder: isDark ? Qt.rgba(156 / 255, 176 / 255, 199 / 255, 0.13) : "#D0DCE8"
    property color authFieldFocus: isDark ? "#8AB5FF" : "#6F99E8"
    property color authLabelText: isDark ? "#E6F0FC" : "#425466"
    property color authPlaceholderText: isDark ? "#BBD0E8" : "#93A4B5"
    property color authBadgeBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.09)
    property color authBadgeBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.18) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.18)
    property color authBadgeText: isDark ? "#D9E7FF" : "#376EC7"
    property color authTabRail: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.09, 0.19, 0.30, 0.05)
    property color authInfoBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.09)
    property color authInfoBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.20) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.18)
    property color authSuccessBg: isDark ? Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.13) : Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.12)
    property color authSuccessBorder: isDark ? Qt.rgba(129 / 255, 228 / 255, 193 / 255, 0.18) : Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.18)
    property color authDangerBg: isDark ? Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.13) : Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.10)
    property color authDangerBorder: isDark ? Qt.rgba(244 / 255, 156 / 255, 171 / 255, 0.18) : Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.20)

    property color searchBg: isDark ? "#22313E" : "#F2F6FA"
    property color searchBorder: isDark ? "#33485B" : "#D8E2EC"
    property color inputBg: isDark ? "#1E2C39" : "#FFFFFF"
    property color inputBorder: isDark ? "#344A5D" : "#CDD8E2"
    property color inputFocus: isDark ? "#7AAEFF" : "#6B96E8"

    property color dialogSelectedBg: isDark ? "#2A4055" : "#EAF2FD"
    property color dialogSelectedFg: isDark ? "#F1F6FC" : "#22303D"
    property color dialogHoverBg: isDark ? "#223545" : "#F2F7FC"
    property color unreadBadgeBg: "#4B89FF"
    property color unreadBadgeFg: "#FFFFFF"
    property color unreadBadgeMutedBg: isDark ? "#4A5968" : "#BCC7D2"
    property color unreadBadgeMutedFg: isDark ? "#E2E9F2" : "#25303A"

    property color bubbleInBg: isDark ? "#14212C" : "#FFFFFF"
    property color bubbleInFg: isDark ? "#E8EEF5" : "#25303A"
    property color bubbleOutBg: isDark ? "#17352E" : "#DDF5EC"
    property color bubbleOutFg: isDark ? "#E8F7F1" : "#1D352D"
    property color bubbleMetaInFg: isDark ? "#E8F1FB" : "#90A0AF"
    property color bubbleMetaOutFg: isDark ? "#F2FFF9" : "#658978"

    property color messageBg: isDark ? "#1A2632" : "#ECF3FA"
    property color messageGradientStart: isDark ? "#1A2836" : "#F2F7FC"
    property color messageGradientEnd: isDark ? "#161F2A" : "#E9F1F8"
    property color messagePatternA: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.25, 0.37, 0.49, 0.06)
    property color messagePatternB: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.035) : Qt.rgba(0.20, 0.32, 0.48, 0.04)

    property int radiusSmall: 8
    property int radiusMedium: 12
    property int radiusLarge: 18
    property int radiusXL: 24
    property int paddingXs: 6
    property int paddingXS: paddingXs
    property int paddingS: 10
    property int paddingM: 14
    property int paddingL: 18
    property int paddingXL: 24
    property int avatarSizeDialogRow: 42
    property int avatarSizeTopBar: 34
    property int dialogRowHeight: 64
    property int topBarHeight: 54
    property int authWindowTitleBarHeight: 26
    property int authWindowTitleTextSize: 12
    property int authPanelWidth: 520
    property int authStageWidth: 560
    property int authStageHeight: 520
    property int authTitleTextSize: 24
    property int authSubtitleTextSize: 13
    property int authBodyTextSize: 14
    property int authMetaTextSize: 12
    property int authFieldHeight: 40
    property int authPrimaryButtonHeight: 40
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 320
    property int centerPaneWidthMin: 560
    property int rightPaneWidth: 336
    property int rightPaneWidthMin: 290
    property int rightPaneWidthMax: 420
    property int threeColumnMinWidth: 1360
    property int iconButtonSize: 32
    property int iconButtonSmall: 26
    property int microTextSize: 13

    function avatarColor(key) {
        var palette = ["#3D8AC7", "#5F7EA8", "#2F6EA5", "#3A6B8C", "#2F7A77", "#5B7A64", "#7A6B5B", "#6B5B7A"]
        if (!key || key.length === 0) {
            return palette[0]
        }
        var hash = 0
        for (var i = 0; i < key.length; ++i) {
            hash = (hash * 31 + key.charCodeAt(i)) & 0x7fffffff
        }
        return palette[hash % palette.length]
    }
}
