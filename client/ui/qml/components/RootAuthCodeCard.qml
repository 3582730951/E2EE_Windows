import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property alias field: inputField
    property alias text: inputField.text
    property string labelText: ""
    property string placeholderText: ""
    property int echoMode: TextInput.Password

    radius: Ui.Style.radiusMedium
    color: Ui.Style.authSurfaceStrong
    border.color: Ui.Style.authContextBorder

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        Text {
            Layout.fillWidth: true
            text: root.labelText
            color: Ui.Style.authLabelText
            font.family: Ui.Style.fontFamily
            font.pixelSize: Ui.Style.fontPixelSize("caption")
            font.weight: Ui.Style.fontWeight("caption")
            font.hintingPreference: Font.PreferFullHinting
            elide: Text.ElideRight
            renderType: Text.NativeRendering
            antialiasing: true
        }

        Components.SecureTextField {
            id: inputField
            Layout.fillWidth: true
            echoMode: root.echoMode
            placeholderText: root.placeholderText
            color: Ui.Style.textPrimary
            placeholderTextColor: Ui.Style.authPlaceholderText
            Accessible.name: root.labelText
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.authFieldBg
                border.color: inputField.activeFocus ? Ui.Style.authFieldFocus : Ui.Style.authFieldBorder
            }
        }
    }
}
