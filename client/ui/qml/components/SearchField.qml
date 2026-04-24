import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias inputMethodHints: field.inputMethodHints
    property alias inputActiveFocus: field.activeFocus
    signal textEdited(string text)

    function focusInput() {
        field.forceActiveFocus()
    }

    implicitWidth: 200
    implicitHeight: 38

    SecureTextField {
        id: field
        anchors.fill: parent
        leftPadding: Ui.Style.paddingM + 16
        rightPadding: Ui.Style.paddingM
        font.pixelSize: 13
        color: Ui.Style.textPrimary
        placeholderTextColor: Ui.Style.textMuted
        background: Rectangle {
            radius: Ui.Style.radiusLarge
            color: Ui.Style.searchBg
            border.color: field.activeFocus ? Ui.Style.inputFocus : Ui.Style.searchBorder
            border.width: 1
        }
        onTextEdited: root.textEdited(text)
    }

    Image {
        source: "qrc:/mi/e2ee/ui/icons/search.svg"
        width: 15
        height: 15
        anchors.left: parent.left
        anchors.leftMargin: Ui.Style.paddingM
        anchors.verticalCenter: parent.verticalCenter
        opacity: 0.8
        fillMode: Image.PreserveAspectFit
    }
}
