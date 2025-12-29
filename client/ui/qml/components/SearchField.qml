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
    implicitHeight: 34

    SecureTextField {
        id: field
        anchors.fill: parent
        leftPadding: Ui.Style.paddingM + 14
        rightPadding: Ui.Style.paddingM
        font.pixelSize: 12
        color: Ui.Style.textPrimary
        placeholderTextColor: Ui.Style.textMuted
        background: Rectangle {
            radius: 16
            color: Ui.Style.searchBg
            border.color: Ui.Style.inputFocus
            border.width: field.activeFocus ? 1 : 0
        }
        onTextEdited: root.textEdited(text)
    }

    Image {
        source: "qrc:/mi/e2ee/ui/icons/search.svg"
        width: 14
        height: 14
        anchors.left: parent.left
        anchors.leftMargin: Ui.Style.paddingM
        anchors.verticalCenter: parent.verticalCenter
        opacity: 0.8
        fillMode: Image.PreserveAspectFit
    }
}
