pragma Singleton
import QtQuick 2.15

QtObject {
    property string fontFamily: "Segoe UI"

    property color windowBg: "#17212B"
    property color panelBg: "#17212B"
    property color panelBgAlt: "#1E2A36"
    property color hoverBg: "#232E3C"
    property color pressedBg: "#2B3948"
    property color borderSubtle: "#243140"
    property color textPrimary: "#F5F5F5"
    property color textSecondary: "#AAB6C3"
    property color textMuted: "#708499"
    property color accent: "#2F6EA5"
    property color accentHover: "#3476AB"
    property color accentPressed: "#2A6396"
    property color link: "#70BAF5"
    property color danger: "#E85D75"
    property color success: "#3CCF91"

    property color dialogSelectedBg: "#2B5278"
    property color dialogSelectedFg: "#E4ECF2"
    property color unreadBadgeBg: "#3D8AC7"
    property color unreadBadgeFg: "#FFFFFF"

    property color bubbleInBg: "#182533"
    property color bubbleInFg: "#E6EEF6"
    property color bubbleOutBg: "#2B5278"
    property color bubbleOutFg: "#E4ECF2"
    property color bubbleMetaFg: "#A8B5C3"

    property int radiusSmall: 8
    property int radiusMedium: 10
    property int radiusLarge: 14
    property int paddingXs: 6
    property int paddingS: 8
    property int paddingM: 12
    property int paddingL: 16
    property int avatarSizeDialogRow: 42
    property int avatarSizeTopBar: 32
    property int dialogRowHeight: 64
    property int leftPaneWidthMin: 280
    property int leftPaneWidthDefault: 320
    property int rightPaneWidth: 360

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
