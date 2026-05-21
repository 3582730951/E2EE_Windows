pragma Singleton
import QtQuick 2.15
import QtCore
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: style
    readonly property var sansFontStacksZhCn: [
        "PingFang SC",
        "Microsoft YaHei UI",
        "Segoe UI Variable",
        "Segoe UI"
    ]
    readonly property var sansFontStacksEnUs: [
        "SF Pro Text",
        "SF Pro Display",
        "Segoe UI Variable",
        "Segoe UI",
        "Microsoft YaHei UI"
    ]
    readonly property var monoFontStacks: [
        "JetBrains Mono",
        "Consolas",
        "Cascadia Mono"
    ]
    readonly property var activeSansFontStack: Ui.I18n.usesCjkLocale
                                               ? sansFontStacksZhCn
                                               : sansFontStacksEnUs
    property string fontFamily: activeSansFontStack[0]
    property string monoFontFamily: monoFontStacks[0]

    Settings {
        id: styleSettings
        category: "ui_style"
        property string storedThemeMode: "system"
    }

    SystemPalette {
        id: systemPalette
    }

    readonly property var shellLayoutContract: ({
        shellMinWidth: typeof uiShellMinWidth !== "undefined" ? Number(uiShellMinWidth) : 760,
        compactTwoColumnMinWidth: typeof uiCompactTwoColumnMinWidth !== "undefined"
                                  ? Number(uiCompactTwoColumnMinWidth)
                                  : 760,
        twoColumnDrawerMinWidth: typeof uiTwoColumnDrawerMinWidth !== "undefined"
                                 ? Number(uiTwoColumnDrawerMinWidth)
                                 : 1120,
        threeColumnMinWidth: typeof uiThreeColumnMinWidth !== "undefined"
                             ? Number(uiThreeColumnMinWidth)
                             : 1360
    })
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

    property color windowBg: isDark ? "#0E1621" : "#F2F2F7"
    property color panelBg: isDark ? "#18222D" : "#FFFFFF"
    property color panelBgAlt: isDark ? "#15202B" : "#F8FAFD"
    property color panelBgRaised: isDark ? "#202C38" : Qt.rgba(1, 1, 1, 0.96)
    property color hoverBg: isDark ? Qt.rgba(1, 1, 1, 0.065) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.08)
    property color pressedBg: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.14)
    property color borderSubtle: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.10)
    property color borderStrong: isDark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.16)
    property color textPrimary: isDark ? "#F5F7FA" : "#111827"
    property color textSecondary: isDark ? "#AAB7C5" : "#667781"
    property color textMuted: isDark ? "#7D8EA3" : "#8B9AA9"
    property color iconMuted: isDark ? "#AAB7C5" : "#667781"
    property color iconActive: isDark ? "#F9FBFF" : "#111827"
    property color accent: "#3390EC"
    property color accentHover: "#5AA6F0"
    property color accentPressed: "#2A7DD2"
    property color accentSoft: isDark ? "#80C2FF" : "#5AA6F0"
    property color link: isDark ? "#80C2FF" : "#2A7DD2"
    property color danger: "#DC2626"
    property color success: "#059669"
    property color warning: "#D6A25A"
    property color warningBorder: alpha(warning, isDark ? 0.26 : 0.22)
    property color statusWarningBg: alpha(warning, isDark ? 0.12 : 0.10)

    property color shellGradientTop: isDark ? "#0C141E" : "#F7F8FC"
    property color shellGradientBottom: isDark ? "#0E1621" : "#E8ECF4"
    property color shellSurface: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.88)
    property color tgCloudTop: isDark ? "#0C141E" : "#F7F8FC"
    property color tgCloudBottom: isDark ? "#0E1621" : "#EBEEF5"
    property color tgGlassSurface: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.90) : Qt.rgba(1, 1, 1, 0.86)
    property color tgCardHighlight: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.10) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.05)
    property color tgCardBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.09)
    property color tgOnlineDot: "#31C38B"
    property color tgMutedBadge: isDark ? "#44576A" : "#C6CED8"
    property color tgUnreadBadge: "#3390EC"
    property color tgActiveRowBg: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.11)
    property color tgActiveRowBorder: isDark ? Qt.rgba(119 / 255, 186 / 255, 255 / 255, 0.30) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18)
    property color railBg: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.84) : Qt.rgba(1, 1, 1, 0.64)
    property color railHeaderBg: isDark ? Qt.rgba(28 / 255, 40 / 255, 52 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.60)
    property color railCardBg: isDark ? Qt.rgba(27 / 255, 38 / 255, 50 / 255, 0.80) : Qt.rgba(1, 1, 1, 0.58)
    property color railAccentBg: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.16) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.10)
    property color railAccentBorder: isDark ? Qt.rgba(126 / 255, 189 / 255, 255 / 255, 0.24) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.16)
    property color topBarBg: isDark ? Qt.rgba(27 / 255, 39 / 255, 50 / 255, 0.76) : Qt.rgba(1, 1, 1, 0.58)
    property color topBarPillBg: isDark ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(255, 255, 255, 0.76)
    property color topBarPillBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
    property color sidebarSurface: isDark ? Qt.rgba(20 / 255, 31 / 255, 43 / 255, 0.82) : Qt.rgba(248 / 255, 250 / 255, 255 / 255, 0.60)
    property color sidebarSurfaceStrong: isDark ? Qt.rgba(24 / 255, 36 / 255, 49 / 255, 0.92) : Qt.rgba(255, 255, 255, 0.78)
    property color sidebarBackdropTop: isDark ? Qt.rgba(39 / 255, 66 / 255, 97 / 255, 0.34) : Qt.rgba(255, 255, 255, 0.92)
    property color sidebarBackdropBottom: isDark ? Qt.rgba(12 / 255, 21 / 255, 30 / 255, 0.18) : Qt.rgba(231 / 255, 238 / 255, 248 / 255, 0.84)
    property color sidebarVibrancyTop: isDark ? Qt.rgba(98 / 255, 166 / 255, 255 / 255, 0.15) : Qt.rgba(123 / 255, 181 / 255, 255 / 255, 0.20)
    property color sidebarVibrancyBottom: isDark ? Qt.rgba(111 / 255, 225 / 255, 196 / 255, 0.06) : Qt.rgba(255 / 255, 255 / 255, 255 / 255, 0.0)
    property color sidebarBorder: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.09)
    property color sidebarHairline: isDark ? Qt.rgba(255, 255, 255, 0.14) : Qt.rgba(255, 255, 255, 0.84)
    property color sidebarGlow: isDark ? Qt.rgba(100 / 255, 169 / 255, 255 / 255, 0.15) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.12)
    property color sidebarHeaderSurface: isDark ? Qt.rgba(255, 255, 255, 0.045) : Qt.rgba(255, 255, 255, 0.58)
    property color sidebarHeaderBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.07)
    property color sidebarTitleText: isDark ? "#F7FBFF" : "#162334"
    property color sidebarSubtitleText: isDark ? "#A8B6C8" : "#5F6E82"
    property color sidebarSectionText: isDark ? "#7F91A4" : "#8A97A9"
    property color sidebarSearchBg: isDark ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(255, 255, 255, 0.66)
    property color sidebarSearchBorder: isDark ? Qt.rgba(255, 255, 255, 0.09) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
    property color sidebarNavBg: isDark ? Qt.rgba(255, 255, 255, 0.04) : Qt.rgba(255, 255, 255, 0.42)
    property color sidebarNavBorder: isDark ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.05)
    property color sidebarNavActiveBg: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.13)
    property color sidebarNavActiveBorder: isDark ? Qt.rgba(126 / 255, 189 / 255, 255 / 255, 0.24) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.16)
    property color sidebarListHoverBg: isDark ? Qt.rgba(255, 255, 255, 0.055) : Qt.rgba(255, 255, 255, 0.68)
    property color sidebarListSelectedBg: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.12)
    property color sidebarListSelectedBorder: isDark ? Qt.rgba(126 / 255, 189 / 255, 255 / 255, 0.26) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.15)
    property color sidebarTimestamp: isDark ? "#7F92A8" : "#8B98A8"
    property color sidebarTimestampUnread: accent
    property color sidebarUnreadBadgeBg: accent
    property color sidebarUnreadBadgeMutedBg: isDark ? "#41576B" : "#C7D0DA"
    property color sidebarUnreadBadgeFg: "#FFFFFF"
    property color sidebarStatusBg: isDark ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(255, 255, 255, 0.72)
    property color sidebarStatusBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
    property color sidebarStatusText: isDark ? "#DCE9F7" : "#3B4A5A"
    property color sidebarStatusChipBg: isDark ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(255, 255, 255, 0.62)
    property color sidebarStatusChipBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.07)
    property color sidebarComposeBg: isDark ? Qt.rgba(255, 255, 255, 0.07) : Qt.rgba(255, 255, 255, 0.74)
    property color sidebarComposeBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.07)
    property color sidebarSectionDivider: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.08)
    property color sidebarSelectionStripe: "#3390EC"
    property color sidebarPinnedTint: isDark ? "#93A9C0" : "#8C99A8"
    property color sidebarMetaChipBg: isDark ? Qt.rgba(255, 255, 255, 0.04) : Qt.rgba(255, 255, 255, 0.58)
    property color sidebarMetaChipBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.06)
    property color sidebarSurfaceOverlay: isDark ? Qt.rgba(255, 255, 255, 0.02) : Qt.rgba(255, 255, 255, 0.30)
    property int glassBlurRadius: 32
    property int sidebarBlurRadius: 36
    property real glassSurfaceOpacity: isDark ? 0.82 : 0.60
    property real sidebarSurfaceOpacity: isDark ? 0.82 : 0.60

    property color authBackdropTop: isDark ? "#0F172A" : "#F5F7FA"
    property color authBackdropBottom: isDark ? "#101922" : "#EDF1F5"
    property color authGlowPrimary: isDark ? Qt.rgba(0.30, 0.54, 1.0, 0.04) : Qt.rgba(0.30, 0.54, 1.0, 0.015)
    property color authGlowSecondary: isDark ? Qt.rgba(0.13, 0.75, 0.56, 0.03) : Qt.rgba(0.13, 0.75, 0.56, 0.010)
    property color authCardBg: isDark ? Qt.rgba(16 / 255, 25 / 255, 34 / 255, 0.95) : Qt.rgba(1, 1, 1, 0.97)
    property color authCardBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.10)
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
    property color authFieldBorder: isDark ? Qt.rgba(1, 1, 1, 0.12) : "#D6E4FA"
    property color authFieldFocus: isDark ? "#8BB5FF" : "#2563EB"
    property color authLabelText: isDark ? "#E6F0FC" : "#425466"
    property color authPlaceholderText: isDark ? "#BBD0E8" : "#93A4B5"
    property color authBadgeBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.09)
    property color authBadgeBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.18) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.18)
    property color authBadgeText: isDark ? "#D9E7FF" : "#2563EB"
    property color authTabRail: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.09, 0.19, 0.30, 0.05)
    property color authInfoBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.09)
    property color authInfoBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.20) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.18)
    property color authSuccessBg: isDark ? Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.13) : Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.12)
    property color authSuccessBorder: isDark ? Qt.rgba(129 / 255, 228 / 255, 193 / 255, 0.18) : Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.18)
    property color authDangerBg: isDark ? Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.13) : Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.10)
    property color authDangerBorder: isDark ? Qt.rgba(244 / 255, 156 / 255, 171 / 255, 0.18) : Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.20)

    property color searchBg: sidebarSearchBg
    property color searchBorder: sidebarSearchBorder
    property color inputBg: isDark ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(255, 255, 255, 0.82)
    property color inputBorder: isDark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.09)
    property color inputFocus: accent

    property color dialogSelectedBg: sidebarListSelectedBg
    property color dialogSelectedFg: isDark ? "#F7FBFF" : "#17324D"
    property color dialogHoverBg: sidebarListHoverBg
    property color unreadBadgeBg: sidebarUnreadBadgeBg
    property color unreadBadgeFg: sidebarUnreadBadgeFg
    property color unreadBadgeMutedBg: sidebarUnreadBadgeMutedBg
    property color unreadBadgeMutedFg: isDark ? "#E7EEF5" : "#4B5563"

    property color bubbleInBg: isDark ? "#182431" : "#FFFFFF"
    property color bubbleInFg: isDark ? "#E8EDF4" : "#0F172A"
    property color bubbleOutBg: isDark ? "#24476F" : "#D8E9FB"
    property color bubbleOutFg: isDark ? "#F8FBFF" : "#17324D"
    property color bubbleMetaInFg: isDark ? "#D5E0EB" : "#8B9AA9"
    property color bubbleMetaOutFg: isDark ? "#E7F3FF" : "#5F7C99"

    property color messageBg: isDark ? "#0E1621" : "#EEF2F7"
    property color messageGradientStart: isDark ? "#0D1520" : "#F3F5FA"
    property color messageGradientEnd: isDark ? "#0E1621" : "#E9EDF5"
    property color messagePatternA: isDark ? Qt.rgba(1, 1, 1, 0.010) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.008)
    property color messagePatternB: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.008) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.005)
    property color heroCardBg: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.96)
    property color heroCardBgAlt: isDark ? Qt.rgba(31 / 255, 44 / 255, 57 / 255, 0.92) : Qt.rgba(247 / 255, 249 / 255, 252 / 255, 0.94)
    property color heroCardBorder: isDark ? Qt.rgba(126 / 255, 189 / 255, 255 / 255, 0.16) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.12)
    property color heroCardSheen: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.62)
    property color heroCardGlow: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.12) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.10)
    property color iconWellBg: isDark ? Qt.rgba(8 / 255, 14 / 255, 23 / 255, 0.48) : Qt.rgba(1, 1, 1, 0.66)
    property color iconWellBorder: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(255, 255, 255, 0.82)
    property color iconWellOverlay: isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.03)
    property color badgeSurface: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.10) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.08)
    property color badgeSurfaceStrong: isDark ? Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.18) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.14)
    property color badgeBorder: isDark ? Qt.rgba(126 / 255, 189 / 255, 255 / 255, 0.22) : Qt.rgba(51 / 255, 144 / 255, 236 / 255, 0.16)
    property color badgeTextPrimary: isDark ? "#EAF4FF" : "#2264AC"
    property color badgeTextSecondary: isDark ? "#A6BAD0" : "#6B7280"
    property color statusSurfaceAlt: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.86) : Qt.rgba(1, 1, 1, 0.82)
    property color statusValueBg: isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.04)
    property color avatarHalo: isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.38)
    property color avatarBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(255, 255, 255, 0.78)
    property color avatarInset: isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(255, 255, 255, 0.16)
    property color mediaCardBg: isDark ? Qt.rgba(24 / 255, 34 / 255, 45 / 255, 0.90) : Qt.rgba(1, 1, 1, 0.90)
    property color mediaPreviewOverlay: isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.56)

    property int radiusSmall: 10
    property int radiusMedium: 14
    property int radiusLarge: 18
    property int radiusXL: 16
    property int radiusContinuous: 28
    property int radiusPill: 18
    property int radiusSidebar: 34
    property int radiusListRow: 18
    property int paddingXs: 4
    property int paddingXS: paddingXs
    property int paddingS: 8
    property int paddingM: 12
    property int paddingL: 18
    property int paddingXL: 26
    property int controlVerticalGap: 16
    property int sectionGap: 22
    property int settingsRowMinHeight: 62
    property int utilityHeaderMaxHeight: 84
    property int utilitySurfaceMaxWidth: 640
    property int compactRailHeaderHeight: 112
    property real badgeMaxWidthRatio: 0.38
    property int avatarSizeDialogRow: 40
    property int avatarSizeTopBar: 34
    property int dialogRowHeight: 72
    property int topBarHeight: 50
    property int sidebarLargeTitleSize: 33
    property int sidebarSubtitleSize: 13
    property int sidebarNavLabelSize: 12
    property int sidebarSectionLabelSize: 10
    property int sidebarRowTitleSize: 15
    property int sidebarRowPreviewSize: 12
    property int sidebarTimestampSize: 11
    property int sidebarStatusSize: 11
    property int sidebarHeaderTopInset: 28
    property int sidebarHeaderBottomInset: 18
    property int sidebarHeaderSideInset: 16
    property int sidebarHeaderStatusHeight: 32
    property int sidebarSearchHeight: 36
    property int sidebarNavHeight: 36
    property int sidebarCompactRailItemSize: 46
    property int authWindowTitleBarHeight: 26
    property int authWindowTitleTextSize: 12
    property int authPanelWidth: 360
    property int authStageWidth: 404
    property int authStageHeight: 352
    property int authTitleTextSize: 18
    property int authSubtitleTextSize: 13
    property int authBodyTextSize: 14
    property int authMetaTextSize: 12
    property int authFieldHeight: 42
    property int authPrimaryButtonHeight: 40
    property int shellMinWidth: shellLayoutContract.shellMinWidth
    property int shellMinHeight: 560
    property int leftPaneWidthUtilityRail: 60
    property int leftPaneWidthMin: 260
    property int leftPaneWidthCompact: 340
    property int leftPaneWidthDefault: 352
    property int leftPaneWidthDetailTight: 300
    property int leftPaneWidthDrawerTight: 300
    property int centerPaneWidthMin: 320
    property int rightPaneWidth: 320
    property int rightPaneWidthMin: 280
    property int rightPaneWidthTight: 280
    property int rightPaneDrawerCompactWidth: 340
    property int rightPaneWidthDrawerNarrow: 320
    property int rightPaneWidthMax: 420
    property int compactTwoColumnMinWidth: shellLayoutContract.compactTwoColumnMinWidth
    property int twoColumnDrawerMinWidth: shellLayoutContract.twoColumnDrawerMinWidth
    property int threeColumnMinWidth: shellLayoutContract.threeColumnMinWidth
    property int iconButtonSize: 38
    property int iconButtonSmall: 24
    property int microTextSize: 13
    property int motionFast: 140
    property int motionNormal: 220

    readonly property var overflowRoles: ({
        display: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        title: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        subtitle: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        caption: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        button_label: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        code_inline: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        value_single: { elide: Text.ElideRight, wrapMode: Text.NoWrap, maximumLineCount: 1 },
        detail: { elide: Text.ElideRight, wrapMode: Text.WordWrap, maximumLineCount: 2 },
        supporting: { elide: Text.ElideRight, wrapMode: Text.WordWrap, maximumLineCount: 2 },
        message_body: { elide: Text.ElideNone, wrapMode: Text.WordWrap, maximumLineCount: 0, metaInsetBottom: 20 }
    })

    function overflowRole(roleName) {
        return overflowRoles[roleName] || overflowRoles.supporting
    }

    function overflowMetaInsetBottom(roleName) {
        var role = overflowRole(roleName)
        return role.metaInsetBottom || 0
    }

    function fontPixelSize(roleName) {
        switch (roleName) {
        case "display":
            return 30
        case "title":
            return 20
        case "subtitle":
            return 17
        case "caption":
        case "button_label":
        case "code_inline":
            return 12
        case "detail":
        case "supporting":
        case "message_body":
            return 14
        default:
            return 14
        }
    }

    function fontLetterSpacing(roleName) {
        switch (roleName) {
        case "display":
            return -0.60
        case "title":
            return -0.32
        case "subtitle":
            return -0.12
        case "caption":
            return 0.16
        case "button_label":
            return 0.10
        case "detail":
        case "supporting":
        case "message_body":
            return 0.02
        default:
            return 0.0
        }
    }

    function fontWeight(roleName) {
        switch (roleName) {
        case "display":
        case "title":
            return Font.DemiBold
        case "subtitle":
        case "caption":
        case "button_label":
            return Font.Medium
        default:
            return Font.Normal
        }
    }

    function alpha(colorValue, opacity) {
        return Qt.rgba(colorValue.r, colorValue.g, colorValue.b, opacity)
    }

    function avatarColor(key) {
        var palette = isDark
                ? ["#6484B7", "#7394C8", "#5E9F9A", "#8575BE", "#B47F93", "#B69064", "#75A774", "#7B93AF"]
                : ["#BDD8FB", "#C9D4FF", "#BFE8DF", "#F5CDDF", "#F6DDB7", "#E0D2FB", "#C7E3F7", "#D1EAC0"]
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
