import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Dialog {
    id: root
    modal: true
    width: 520
    height: 600
    title: "New Group"
    standardButtons: Dialog.NoButton

    property int stepIndex: 0
    property var selectedIds: []

    onOpened: {
        stepIndex = 0
        selectedIds = []
        groupNameField.text = ""
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
            Text {
                text: stepIndex === 0 ? "1/2" : "2/2"
                color: Ui.Style.textMuted
                font.pixelSize: 12
            }
            Components.IconButton {
                icon.source: "qrc:/mi/e2ee/ui/icons/close-x.svg"
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

        StackLayout {
            id: steps
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: stepIndex

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    Text {
                        text: "Select members"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }

                    ListView {
                        id: memberList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ui.AppStore.contactsModel
                        delegate: Item {
                            width: ListView.view.width
                            height: 54
                            Rectangle {
                                anchors.fill: parent
                                radius: Ui.Style.radiusMedium
                                color: mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingS
                                spacing: Ui.Style.paddingM
                                CheckBox {
                                    checked: selectedIds.indexOf(contactId) !== -1
                                    onClicked: toggleSelection(contactId, checked)
                                }
                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 16
                                    color: Ui.Style.avatarColor(avatarKey)
                                    Text {
                                        anchors.centerIn: parent
                                        text: displayName.length > 0 ? displayName.charAt(0).toUpperCase() : "?"
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 12
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: displayName
                                        font.pixelSize: 12
                                        color: Ui.Style.textPrimary
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: usernameOrPhone
                                        font.pixelSize: 10
                                        color: Ui.Style.textMuted
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingM

                    Text {
                        text: "Group details"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }

                    TextField {
                        id: groupNameField
                        Layout.fillWidth: true
                        placeholderText: "Group name"
                        font.pixelSize: 13
                        color: Ui.Style.textPrimary
                        placeholderTextColor: Ui.Style.textMuted
                        background: Rectangle {
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.inputBg
                            border.color: groupNameField.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
                        }
                    }
                    Rectangle {
                        width: 96
                        height: 96
                        radius: 48
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle
                        Text {
                            anchors.centerIn: parent
                            text: "Avatar"
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Components.GhostButton {
                text: stepIndex === 0 ? "Cancel" : "Back"
                Layout.fillWidth: true
                onClicked: {
                    if (stepIndex === 0) {
                        root.close()
                    } else {
                        stepIndex = 0
                    }
                }
            }
            Components.PrimaryButton {
                text: stepIndex === 0 ? "Next" : "Create"
                Layout.fillWidth: true
                enabled: stepIndex === 0 ? selectedIds.length > 0 : groupNameField.text.trim().length > 0
                onClicked: {
                    if (stepIndex === 0) {
                        stepIndex = 1
                    } else {
                        Ui.AppStore.createGroup(groupNameField.text, selectedIds)
                        root.close()
                    }
                }
            }
        }
    }

    function toggleSelection(contactId, checked) {
        var idx = selectedIds.indexOf(contactId)
        if (checked) {
            if (idx === -1) {
                selectedIds.push(contactId)
            }
        } else if (idx !== -1) {
            selectedIds.splice(idx, 1)
        }
    }
}
