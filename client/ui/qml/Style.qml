pragma Singleton
import QtQuick 2.15

QtObject {
    property string fontFamily: "Segoe UI"

    property string themeMode: "dark"
    property bool isDark: themeMode !== "light"

    property color windowBg: isDark ? "#0F141C" : "#F4F6F8"
    property color panelBg: isDark ? "#18222E" : "#FFFFFF"
    property color panelBgAlt: isDark ? "#1C2733" : "#F5F6F8"
    property color panelBgRaised: isDark ? "#1F2B39" : "#FFFFFF"
    property color hoverBg: isDark ? "#223041" : "#EEF2F6"
    property color pressedBg: isDark ? "#273647" : "#E5EBF1"
    property color borderSubtle: isDark ? "#2B3848" : "#E1E7EE"
    property color borderStrong: isDark ? "#364558" : "#D3DBE3"
    property color textPrimary: isDark ? "#E6EDF3" : "#2B2F33"
    property color textSecondary: isDark ? "#A1AFBC" : "#6E7A86"
    property color textMuted: isDark ? "#718091" : "#9AA6B2"
    property color iconMuted: isDark ? "#A1AFBC" : "#6E7A86"
    property color iconActive: isDark ? "#E6EDF3" : "#2B2F33"
    property color accent: "#3B7DD8"
    property color accentHover: "#4A8AE0"
    property color accentPressed: "#3372C6"
    property color link: isDark ? "#6CB1FF" : "#2F80ED"
    property color danger: "#E85D75"
    property color success: "#3CCF91"

    property color searchBg: isDark ? "#1E2A37" : "#F1F2F4"
    property color searchBorder: isDark ? "#2B3848" : "#E1E4E8"
    property color inputBg: isDark ? "#1A2633" : "#FFFFFF"
    property color inputBorder: isDark ? "#2B3848" : "#D6DDE4"
    property color inputFocus: isDark ? "#6AA9E9" : "#7FB3E6"

    property color dialogSelectedBg: isDark ? "#253549" : "#E8F1FB"
    property color dialogSelectedFg: isDark ? "#E6EDF3" : "#2B2F33"
    property color dialogHoverBg: isDark ? "#1F2C3C" : "#F4F6F8"
    property color unreadBadgeBg: "#4A90E2"
    property color unreadBadgeFg: "#FFFFFF"

    property color bubbleInBg: isDark ? "#1E2B39" : "#FFFFFF"
    property color bubbleInFg: isDark ? "#E6EDF3" : "#2B2F33"
    property color bubbleOutBg: isDark ? "#2E5B4B" : "#D9F2BF"
    property color bubbleOutFg: isDark ? "#EAF6F0" : "#1F2A1E"
    property color bubbleMetaInFg: isDark ? "#7E8B98" : "#9AA6B2"
    property color bubbleMetaOutFg: isDark ? "#A2BFB1" : "#7A8C6A"

    property color messageBg: isDark ? "#18232E" : "#BFD59B"
    property color messageGradientStart: isDark ? "#1B2835" : "#C3D9A6"
    property color messageGradientEnd: isDark ? "#15202B" : "#AFCB86"
    property color messagePatternA: isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0.45, 0.61, 0.45, 0.18)
    property color messagePatternB: isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0.34, 0.49, 0.38, 0.12)

    property int radiusSmall: 6
    property int radiusMedium: 10
    property int radiusLarge: 14
    property int radiusXL: 18
    property int paddingXs: 6
    property int paddingS: 8
    property int paddingM: 12
    property int paddingL: 16
    property int paddingXL: 20
    property int avatarSizeDialogRow: 44
    property int avatarSizeTopBar: 34
    property int dialogRowHeight: 66
    property int topBarHeight: 48
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 300
    property int rightPaneWidth: 360
    property int iconButtonSize: 30
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
