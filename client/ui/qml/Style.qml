pragma Singleton
import QtQuick 2.15

QtObject {
    property string fontFamily: "Segoe UI Variable"

    property string themeMode: "dark"
    property bool isDark: themeMode !== "light"

    property color windowBg: isDark ? "#090F14" : "#EFF3F7"
    property color panelBg: isDark ? "#101922" : "#FFFFFF"
    property color panelBgAlt: isDark ? "#14202B" : "#F6F8FB"
    property color panelBgRaised: isDark ? "#182531" : "#FFFFFF"
    property color hoverBg: isDark ? "#1D2C39" : "#EAF0F6"
    property color pressedBg: isDark ? "#243645" : "#DEE7EF"
    property color borderSubtle: isDark ? "#213140" : "#D7E0E8"
    property color borderStrong: isDark ? "#314657" : "#C6D2DC"
    property color textPrimary: isDark ? "#E8EEF5" : "#25303A"
    property color textSecondary: isDark ? "#D7E2EC" : "#617180"
    property color textMuted: isDark ? "#BAC7D4" : "#8D9CAA"
    property color iconMuted: isDark ? "#A3B4C6" : "#617180"
    property color iconActive: isDark ? "#F2F6FB" : "#25303A"
    property color accent: "#4B89FF"
    property color accentHover: "#5A95FF"
    property color accentPressed: "#3D78E7"
    property color accentSoft: "#9CC0FF"
    property color link: isDark ? "#85B8FF" : "#2E72DE"
    property color danger: "#E76479"
    property color success: "#21BE8B"
    property color warning: "#D6A25A"

    property color shellGradientTop: isDark ? "#0B1118" : "#EEF2F7"
    property color shellGradientBottom: isDark ? "#0F1822" : "#E5EBF1"
    property color shellSurface: isDark ? Qt.rgba(11 / 255, 17 / 255, 24 / 255, 0.96) : "#FFFFFF"
    property color tgCloudTop: isDark ? "#0A1118" : "#EEF4FB"
    property color tgCloudBottom: isDark ? "#0E1A25" : "#E2EBF4"
    property color tgGlassSurface: isDark ? Qt.rgba(14 / 255, 23 / 255, 33 / 255, 0.94) : Qt.rgba(1, 1, 1, 0.93)
    property color tgCardHighlight: isDark ? Qt.rgba(140 / 255, 188 / 255, 1.0, 0.10) : Qt.rgba(43 / 255, 135 / 255, 255 / 255, 0.08)
    property color tgCardBorder: isDark ? Qt.rgba(148 / 255, 192 / 255, 1.0, 0.22) : Qt.rgba(43 / 255, 135 / 255, 255 / 255, 0.16)
    property color tgOnlineDot: "#31C38B"
    property color tgMutedBadge: isDark ? "#415264" : "#B8C3CF"
    property color tgUnreadBadge: "#4E8FFF"
    property color tgActiveRowBg: isDark ? Qt.rgba(46 / 255, 84 / 255, 126 / 255, 0.26) : Qt.rgba(78 / 255, 143 / 255, 255 / 255, 0.17)
    property color tgActiveRowBorder: isDark ? Qt.rgba(122 / 255, 171 / 255, 1.0, 0.35) : Qt.rgba(78 / 255, 143 / 255, 255 / 255, 0.26)
    property color railBg: isDark ? "#0E161F" : "#FFFFFF"
    property color railHeaderBg: isDark ? "#131E29" : "#F6F8FB"
    property color railCardBg: isDark ? "#17232F" : "#FFFFFF"
    property color railAccentBg: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.14) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.10)
    property color railAccentBorder: isDark ? Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.20) : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.18)
    property color topBarBg: isDark ? "#111A23" : "#FFFFFF"
    property color topBarPillBg: isDark ? "#182631" : "#EDF3F8"
    property color topBarPillBorder: isDark ? "#223444" : "#D7E2EC"

    property color authBackdropTop: "#071018"
    property color authBackdropBottom: "#0E1823"
    property color authGlowPrimary: Qt.rgba(0.30, 0.54, 1.0, 0.22)
    property color authGlowSecondary: Qt.rgba(0.13, 0.75, 0.56, 0.16)
    property color authCardBg: Qt.rgba(12 / 255, 18 / 255, 27 / 255, 0.92)
    property color authCardBorder: Qt.rgba(159 / 255, 187 / 255, 214 / 255, 0.10)
    property color authSurface: Qt.rgba(1, 1, 1, 0.03)
    property color authSurfaceStrong: Qt.rgba(1, 1, 1, 0.07)
    property color authFieldBg: Qt.rgba(8 / 255, 14 / 255, 20 / 255, 0.96)
    property color authFieldBorder: Qt.rgba(156 / 255, 176 / 255, 199 / 255, 0.13)
    property color authFieldFocus: "#8AB5FF"
    property color authBadgeBg: Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12)
    property color authBadgeBorder: Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.18)
    property color authBadgeText: "#D9E7FF"
    property color authTabRail: Qt.rgba(1, 1, 1, 0.03)
    property color authInfoBg: Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.12)
    property color authInfoBorder: Qt.rgba(156 / 255, 192 / 255, 255 / 255, 0.20)
    property color authSuccessBg: Qt.rgba(33 / 255, 190 / 255, 139 / 255, 0.13)
    property color authSuccessBorder: Qt.rgba(129 / 255, 228 / 255, 193 / 255, 0.18)
    property color authDangerBg: Qt.rgba(231 / 255, 100 / 255, 121 / 255, 0.13)
    property color authDangerBorder: Qt.rgba(244 / 255, 156 / 255, 171 / 255, 0.18)

    property color searchBg: isDark ? "#17232E" : "#F0F4F8"
    property color searchBorder: isDark ? "#233342" : "#D8E2EB"
    property color inputBg: isDark ? "#15212C" : "#FFFFFF"
    property color inputBorder: isDark ? "#223545" : "#CDD8E2"
    property color inputFocus: isDark ? "#7AAEFF" : "#6B96E8"

    property color dialogSelectedBg: isDark ? "#172A3A" : "#E5EFFC"
    property color dialogSelectedFg: isDark ? "#F0F5FB" : "#22303D"
    property color dialogHoverBg: isDark ? "#13212D" : "#F2F6FA"
    property color unreadBadgeBg: "#4B89FF"
    property color unreadBadgeFg: "#FFFFFF"
    property color unreadBadgeMutedBg: isDark ? "#4A5968" : "#BCC7D2"
    property color unreadBadgeMutedFg: isDark ? "#E2E9F2" : "#25303A"

    property color bubbleInBg: isDark ? "#14212C" : "#FFFFFF"
    property color bubbleInFg: isDark ? "#E8EEF5" : "#25303A"
    property color bubbleOutBg: isDark ? "#17352E" : "#DDF5EC"
    property color bubbleOutFg: isDark ? "#E8F7F1" : "#1D352D"
    property color bubbleMetaInFg: isDark ? "#C7D5E2" : "#90A0AF"
    property color bubbleMetaOutFg: isDark ? "#D4EFE5" : "#658978"

    property color messageBg: isDark ? "#101821" : "#E8F0F6"
    property color messageGradientStart: isDark ? "#101A24" : "#EFF5FA"
    property color messageGradientEnd: isDark ? "#0C141D" : "#E5EDF3"
    property color messagePatternA: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.25, 0.37, 0.49, 0.08)
    property color messagePatternB: isDark ? Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.035) : Qt.rgba(0.20, 0.32, 0.48, 0.05)

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
    property int avatarSizeDialogRow: 48
    property int avatarSizeTopBar: 34
    property int dialogRowHeight: 74
    property int topBarHeight: 54
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 324
    property int rightPaneWidth: 360
    property int iconButtonSize: 32
    property int iconButtonSmall: 26

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
