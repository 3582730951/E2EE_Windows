pragma Singleton
import QtQuick 2.15

QtObject {
    property string fontFamily: "Segoe UI"

    property color windowBg: "#F4F6F8"
    property color panelBg: "#FFFFFF"
    property color panelBgAlt: "#F5F6F8"
    property color panelBgRaised: "#FFFFFF"
    property color hoverBg: "#EEF2F6"
    property color pressedBg: "#E5EBF1"
    property color borderSubtle: "#E1E7EE"
    property color borderStrong: "#D3DBE3"
    property color textPrimary: "#2B2F33"
    property color textSecondary: "#6E7A86"
    property color textMuted: "#9AA6B2"
    property color iconMuted: "#6E7A86"
    property color iconActive: "#2B2F33"
    property color accent: "#3B7DD8"
    property color accentHover: "#4A8AE0"
    property color accentPressed: "#3372C6"
    property color link: "#2F80ED"
    property color danger: "#E85D75"
    property color success: "#3CCF91"

    property color searchBg: "#F1F2F4"
    property color searchBorder: "#E1E4E8"
    property color inputBg: "#FFFFFF"
    property color inputBorder: "#D6DDE4"
    property color inputFocus: "#7FB3E6"

    property color dialogSelectedBg: "#E8F1FB"
    property color dialogSelectedFg: "#2B2F33"
    property color dialogHoverBg: "#F4F6F8"
    property color unreadBadgeBg: "#4A90E2"
    property color unreadBadgeFg: "#FFFFFF"

    property color bubbleInBg: "#FFFFFF"
    property color bubbleInFg: "#2B2F33"
    property color bubbleOutBg: "#D9F2BF"
    property color bubbleOutFg: "#1F2A1E"
    property color bubbleMetaInFg: "#9AA6B2"
    property color bubbleMetaOutFg: "#7A8C6A"

    property color messageBg: "#D4ECC3"
    property color messagePatternA: Qt.rgba(0.45, 0.61, 0.45, 0.18)
    property color messagePatternB: Qt.rgba(0.34, 0.49, 0.38, 0.12)

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
