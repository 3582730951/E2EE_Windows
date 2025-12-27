import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Dialog {
    id: root
    modal: true
    width: 420
    height: 520
    title: "New Chat"
    standardButtons: Dialog.Close

    property string filterText: ""
    property var filtered: []

    onOpened: rebuild()
    onVisibleChanged: {
        if (!visible) {
            searchField.text = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Search contacts"
            onTextChanged: {
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
                height: 54
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    spacing: Ui.Style.paddingM
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
                            font.pixelSize: 13
                            color: Ui.Style.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            text: usernameOrPhone
                            font.pixelSize: 11
                            color: Ui.Style.textMuted
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
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
