pragma Singleton
import QtQuick 2.15
import QtCore
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: style
    readonly property var sansFontStacksZhCn: [
        "Microsoft YaHei UI",
        "Segoe UI Variable",
        "Segoe UI"
    ]
    readonly property var sansFontStacksEnUs: [
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

    readonly property string smokeThemeMode: typeof uiSmokeTheme !== "undefined"
                                             ? (uiSmokeTheme || "")
                                             : ""
    readonly property string smokeSceneName: typeof uiSmokeScene !== "undefined"
                                             ? ((uiSmokeScene || "").toLowerCase())
                                             : ""
    readonly property bool smokePostLoginScene: smokeSceneName === "post_login"
    readonly property bool smokePostLoginLightScene: smokeSceneName === "post_login_light" ||
                                                     smokeSceneName === "chat_list_light"
    readonly property bool smokeLightPrimaryPalette: smokeThemeMode === "light" && smokePostLoginScene
    readonly property bool smokeLightAltPalette: smokeThemeMode === "light" && smokePostLoginLightScene
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
    property bool isDark: {
        var activeTheme = smokeThemeMode.length > 0 ? smokeThemeMode : themeMode
        return activeTheme === "dark" || (activeTheme === "system" && systemDark)
    }

    onThemeModeChanged: {
        if (styleSettings.storedThemeMode !== themeMode) {
            styleSettings.storedThemeMode = themeMode
        }
    }

    property color windowBg: isDark ? "#0F172A" : (smokeLightAltPalette ? "#F3FAF4" : (smokeLightPrimaryPalette ? "#F4F8FF" : "#F8FAFC"))
    property color panelBg: isDark ? "#101922" : (smokeLightAltPalette ? "#FEFFFC" : "#FFFFFF")
    property color panelBgAlt: isDark ? "#182431" : (smokeLightAltPalette ? "#ECF6EF" : (smokeLightPrimaryPalette ? "#EDF3FF" : "#F1F5FD"))
    property color panelBgRaised: isDark ? "#101E2E" : (smokeLightAltPalette ? "#FFFDF8" : "#FFFFFF")
    property color hoverBg: isDark ? "#1C2A3A" : (smokeLightAltPalette ? "#EFF7F1" : "#EEF4FC")
    property color pressedBg: isDark ? "#213244" : (smokeLightAltPalette ? "#E2F0E6" : "#E4ECFC")
    property color borderSubtle: isDark ? Qt.rgba(1, 1, 1, 0.08) : "#E4ECFC"
    property color borderStrong: isDark ? Qt.rgba(1, 1, 1, 0.12) : (smokeLightAltPalette ? "#D6E7D8" : "#D6E4FA")
    property color textPrimary: isDark ? "#E8EDF4" : "#0F172A"
    property color textSecondary: isDark ? "#9DAEBC" : (smokeLightAltPalette ? "#5D746D" : "#64748B")
    property color textMuted: isDark ? "#7E92A7" : (smokeLightAltPalette ? "#788C84" : "#7B8CA1")
    property color iconMuted: isDark ? "#9DAEBC" : (smokeLightAltPalette ? "#5D746D" : "#64748B")
    property color iconActive: isDark ? "#F8FBFF" : "#0F172A"
    property color accent: smokeLightAltPalette ? "#138A72" : "#2563EB"
    property color accentHover: smokeLightAltPalette ? "#16957B" : "#2E6DF0"
    property color accentPressed: smokeLightAltPalette ? "#10735F" : "#1F57CF"
    property color accentSoft: isDark ? "#8BB5FF" : (smokeLightAltPalette ? "#138A72" : "#2563EB")
    property color link: isDark ? "#85B8FF" : (smokeLightAltPalette ? "#0F7F69" : "#2E72DE")
    property color danger: "#DC2626"
    property color success: "#059669"
    property color warning: "#D6A25A"

    property color shellGradientTop: isDark ? "#0F172A" : (smokeLightAltPalette ? "#F5FBF5" : (smokeLightPrimaryPalette ? "#F4F8FF" : "#F8FAFC"))
    property color shellGradientBottom: isDark ? "#101922" : (smokeLightAltPalette ? "#E5F2EA" : (smokeLightPrimaryPalette ? "#E8F0FF" : "#EEF4FC"))
    property color shellSurface: isDark ? Qt.rgba(16 / 255, 25 / 255, 34 / 255, 0.98) : (smokeLightAltPalette ? Qt.rgba(254 / 255, 255 / 255, 251 / 255, 0.975) : Qt.rgba(1, 1, 1, 0.97))
    property color tgCloudTop: isDark ? "#0F172A" : (smokeLightAltPalette ? "#F5FAF5" : (smokeLightPrimaryPalette ? "#F3F7FF" : "#F8FAFC"))
    property color tgCloudBottom: isDark ? "#101922" : (smokeLightAltPalette ? "#E5F1E9" : (smokeLightPrimaryPalette ? "#E7EFFF" : "#EEF4FC"))
    property color tgGlassSurface: isDark ? Qt.rgba(16 / 255, 25 / 255, 34 / 255, 0.96) : (smokeLightAltPalette ? Qt.rgba(254 / 255, 255 / 255, 250 / 255, 0.978) : Qt.rgba(1, 1, 1, 0.97))
    property color tgCardHighlight: isDark ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.10) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.06) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.06))
    property color tgCardBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.12) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.12))
    property color tgOnlineDot: "#31C38B"
    property color tgMutedBadge: isDark ? "#415264" : (smokeLightAltPalette ? "#B5C8BE" : "#B8C3CF")
    property color tgUnreadBadge: smokeLightAltPalette ? "#1B9A7C" : "#4E8FFF"
    property color tgActiveRowBg: isDark ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.18) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.10) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.09))
    property color tgActiveRowBorder: isDark ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.28) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.20) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.18))
    property color railBg: isDark ? "#17232F" : (smokeLightAltPalette ? "#F9FCF7" : "#FFFFFF")
    property color railHeaderBg: isDark ? "#1D2B39" : (smokeLightAltPalette ? "#F1F7F0" : (smokeLightPrimaryPalette ? "#F5F8FF" : "#F7FAFD"))
    property color railCardBg: isDark ? "#17232F" : (smokeLightAltPalette ? "#FFFEFB" : "#FFFFFF")
    property color railAccentBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.16) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.09) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.08))
    property color railAccentBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.24) : (smokeLightAltPalette ? Qt.rgba(19 / 255, 138 / 255, 114 / 255, 0.18) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.14))
    property color topBarBg: isDark ? "#1A2632" : (smokeLightAltPalette ? "#FEFFFC" : "#FFFFFF")
    property color topBarPillBg: isDark ? "#253444" : (smokeLightAltPalette ? "#EEF6F0" : "#EEF4FA")
    property color topBarPillBorder: isDark ? "#33485B" : (smokeLightAltPalette ? "#D5E6D8" : "#D9E4EF")

    property color authBackdropTop: isDark ? "#0F172A" : "#F8FAFC"
    property color authBackdropBottom: isDark ? "#101922" : "#EEF4FC"
    property color authGlowPrimary: isDark ? Qt.rgba(0.30, 0.54, 1.0, 0.12) : Qt.rgba(0.30, 0.54, 1.0, 0.10)
    property color authGlowSecondary: isDark ? Qt.rgba(0.13, 0.75, 0.56, 0.08) : Qt.rgba(0.13, 0.75, 0.56, 0.06)
    property color authCardBg: isDark ? Qt.rgba(16 / 255, 25 / 255, 34 / 255, 0.95) : Qt.rgba(1, 1, 1, 0.97)
    property color authCardBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.12)
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

    property color searchBg: isDark ? "#182431" : (smokeLightAltPalette ? "#F3F8F2" : "#F1F5FD")
    property color searchBorder: isDark ? Qt.rgba(1, 1, 1, 0.10) : (smokeLightAltPalette ? "#D9E8DC" : "#E4ECFC")
    property color inputBg: isDark ? "#1E2C39" : "#FFFFFF"
    property color inputBorder: isDark ? Qt.rgba(1, 1, 1, 0.12) : (smokeLightAltPalette ? "#D6E7D8" : "#D6E4FA")
    property color inputFocus: isDark ? "#8BB5FF" : (smokeLightAltPalette ? "#138A72" : "#2563EB")

    property color dialogSelectedBg: isDark ? "#2A4055" : (smokeLightAltPalette ? "#E3F3EB" : "#EAF2FD")
    property color dialogSelectedFg: isDark ? "#F1F6FC" : (smokeLightAltPalette ? "#1D3B33" : "#22303D")
    property color dialogHoverBg: isDark ? "#223545" : (smokeLightAltPalette ? "#F2F8F1" : "#F2F7FC")
    property color unreadBadgeBg: smokeLightAltPalette ? "#1B9A7C" : "#4B89FF"
    property color unreadBadgeFg: "#FFFFFF"
    property color unreadBadgeMutedBg: isDark ? "#4A5968" : (smokeLightAltPalette ? "#B5C8BE" : "#BCC7D2")
    property color unreadBadgeMutedFg: isDark ? "#E2E9F2" : "#25303A"

    property color bubbleInBg: isDark ? "#182431" : "#FFFFFF"
    property color bubbleInFg: isDark ? "#E8EDF4" : "#0F172A"
    property color bubbleOutBg: isDark ? "#1D3560" : (smokeLightAltPalette ? "#D8F0E3" : "#DCE8FF")
    property color bubbleOutFg: isDark ? "#F8FBFF" : (smokeLightAltPalette ? "#103F34" : "#17315A")
    property color bubbleMetaInFg: isDark ? "#E8F1FB" : "#90A0AF"
    property color bubbleMetaOutFg: isDark ? "#F2FFF9" : (smokeLightAltPalette ? "#5E8A79" : "#658978")

    property color messageBg: isDark ? "#101922" : (smokeLightAltPalette ? "#F3F8F3" : (smokeLightPrimaryPalette ? "#F5F8FF" : "#F8FAFC"))
    property color messageGradientStart: isDark ? "#101922" : (smokeLightAltPalette ? "#F5FBF5" : (smokeLightPrimaryPalette ? "#F5F8FF" : "#F8FAFC"))
    property color messageGradientEnd: isDark ? "#0F172A" : (smokeLightAltPalette ? "#E8F3EC" : (smokeLightPrimaryPalette ? "#EAF1FF" : "#EEF4FC"))
    property color messagePatternA: isDark ? Qt.rgba(1, 1, 1, 0.02) : Qt.rgba(15 / 255, 23 / 255, 42 / 255, 0.03)
    property color messagePatternB: isDark ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.02) : Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.018)

    property int radiusSmall: 8
    property int radiusMedium: 12
    property int radiusLarge: 16
    property int radiusXL: 20
    property int paddingXs: 4
    property int paddingXS: paddingXs
    property int paddingS: 8
    property int paddingM: 12
    property int paddingL: 16
    property int paddingXL: 24
    property int avatarSizeDialogRow: 44
    property int avatarSizeTopBar: 34
    property int dialogRowHeight: 68
    property int topBarHeight: 56
    property int authWindowTitleBarHeight: 26
    property int authWindowTitleTextSize: 12
    property int authPanelWidth: 412
    property int authStageWidth: 444
    property int authStageHeight: 460
    property int authTitleTextSize: 24
    property int authSubtitleTextSize: 13
    property int authBodyTextSize: 14
    property int authMetaTextSize: 12
    property int authFieldHeight: 40
    property int authPrimaryButtonHeight: 40
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 292
    property int centerPaneWidthMin: 560
    property int rightPaneWidth: 336
    property int rightPaneWidthMin: 290
    property int rightPaneWidthMax: 420
    property int threeColumnMinWidth: 1320
    property int iconButtonSize: 36
    property int iconButtonSmall: 24
    property int microTextSize: 13

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
            return 24
        case "title":
            return 20
        case "subtitle":
            return 16
        case "caption":
        case "button_label":
        case "code_inline":
            return 12
        case "detail":
        case "supporting":
        case "message_body":
            return 15
        default:
            return 14
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
