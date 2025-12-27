pragma Singleton
import QtQuick 2.15

QtObject {
    property string fontFamily: "Segoe UI"

    property color windowBg: "#0E1621"
    property color panelBg: "#17212B"
    property color panelBgAlt: "#1C2735"
    property color panelBgRaised: "#1F2D3B"
    property color hoverBg: "#223243"
    property color pressedBg: "#1A2634"
    property color borderSubtle: "#243448"
    property color borderStrong: "#2B3D52"
    property color textPrimary: "#E7EDF4"
    property color textSecondary: "#B5C2D0"
    property color textMuted: "#7D8FA5"
    property color iconMuted: "#9BB0C7"
    property color iconActive: "#E7EDF4"
    property color accent: "#2F6EA5"
    property color accentHover: "#367BB0"
    property color accentPressed: "#2A6396"
    property color link: "#6FB2E6"
    property color danger: "#E85D75"
    property color success: "#3CCF91"

    property color searchBg: "#1B2836"
    property color searchBorder: "#2A3A4D"
    property color inputBg: "#1B2836"
    property color inputBorder: "#2A3A4D"
    property color inputFocus: "#3C7FB5"

    property color dialogSelectedBg: "#23384F"
    property color dialogSelectedFg: "#E4ECF2"
    property color dialogHoverBg: "#203244"
    property color unreadBadgeBg: "#3D8AC7"
    property color unreadBadgeFg: "#FFFFFF"

    property color bubbleInBg: "#1A2736"
    property color bubbleInFg: "#E8F0F7"
    property color bubbleOutBg: "#2B5278"
    property color bubbleOutFg: "#E6EFF8"
    property color bubbleMetaInFg: "#97A9BC"
    property color bubbleMetaOutFg: "#C0D0E0"

    property color messageBg: "#0E1621"
    property color messagePatternA: Qt.rgba(0.21, 0.28, 0.38, 0.12)
    property color messagePatternB: Qt.rgba(0.18, 0.24, 0.32, 0.08)

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
    property int dialogRowHeight: 68
    property int topBarHeight: 56
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 330
    property int rightPaneWidth: 360
    property int iconButtonSize: 32
    property int iconButtonSmall: 28

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
