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
    property string descriptionText: ""
    property int echoMode: TextInput.Password

    radius: Ui.Style.radiusLarge
    color: Ui.Style.authSurfaceStrong
    border.width: 1
    border.color: Ui.Style.authContextBorder
    clip: true

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.34 : 0.72
    }

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

        Components.UiText {
            Layout.fillWidth: true
            visible: root.descriptionText.length > 0
            text: root.descriptionText
            textRole: "supporting"
            roleColor: Ui.Style.textSecondary
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
