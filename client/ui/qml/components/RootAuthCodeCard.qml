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

        Components.UiText {
            Layout.fillWidth: true
            text: root.labelText
            textRole: "caption"
            foreground: Ui.Style.authLabelText
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
