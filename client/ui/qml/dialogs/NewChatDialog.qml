import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    visible: false
    width: 420
    height: 520
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.newChat.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function open() {
        visible = true
        raise()
        requestActivate()
    }

    property string filterText: ""
    property var filtered: []

    onVisibleChanged: {
        if (visible) {
            rebuild()
        } else {
            searchField.text = ""
        }
    }

    background: Rectangle {
        radius: Ui.Style.radiusLarge
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
    }

    header: Rectangle {
        height: Ui.Style.topBarHeight
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active && root.startSystemMove) {
                    root.startSystemMove()
                }
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingM
            Text {
                text: root.title
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            Components.IconButton {
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/close-x.svg"
                             : "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
                buttonSize: Ui.Style.iconButtonSmall
                iconSize: 14
                onClicked: root.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        Components.SearchField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: Ui.I18n.t("dialog.newChat.search")
            onTextEdited: {
                filterText = text
                rebuild()
            }
        }

        ListView {
            id: contactList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ListModel {
                id: filteredModel
            }
            delegate: Item {
                width: ListView.view.width
                height: 56
                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusMedium
                    color: mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    spacing: 10
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: Ui.Style.avatarColor(avatarKey)
                        Text {
                            anchors.centerIn: parent
                            text: displayName.length > 0 ? displayName.charAt(0).toUpperCase() : "?"
                            color: Ui.Style.textPrimary
                            font.pixelSize: 13
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: displayName
                            Layout.fillWidth: true
                            font.pixelSize: 13
                            color: Ui.Style.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            text: usernameOrPhone
                            Layout.fillWidth: true
                            font.pixelSize: 11
                            color: Ui.Style.textMuted
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        Ui.AppStore.openChatFromContact(contactId)
                        root.close()
                    }
                }
            }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        }
    }

    function rebuild() {
        filteredModel.clear()
        var query = (filterText || "").toLowerCase()
        for (var i = 0; i < Ui.AppStore.contactsModel.count; ++i) {
            var item = Ui.AppStore.contactsModel.get(i)
            if (query.length === 0 ||
                item.displayName.toLowerCase().indexOf(query) !== -1 ||
                item.usernameOrPhone.toLowerCase().indexOf(query) !== -1) {
                filteredModel.append(item)
            }
        }
    }
}
