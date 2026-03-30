import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Text {
    id: control

    property string textRole: "supporting"
    property color roleColor: Ui.Style.textPrimary
    property bool monospace: textRole === "code_inline"
    readonly property int metaInsetBottom: Ui.Style.overflowMetaInsetBottom(textRole)

    color: roleColor
    font.family: monospace ? Ui.Style.monoFontFamily : Ui.Style.fontFamily
    font.pixelSize: Ui.Style.fontPixelSize(textRole)
    font.weight: Ui.Style.fontWeight(textRole)
    font.hintingPreference: Font.PreferFullHinting
    wrapMode: Ui.Style.overflowRole(textRole).wrapMode
    elide: Ui.Style.overflowRole(textRole).elide
    maximumLineCount: Ui.Style.overflowRole(textRole).maximumLineCount
    renderType: Text.NativeRendering
    antialiasing: true
}
