import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    visible: false
    width: 480
    height: 520
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.notifications.title")
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
        spacing: Ui.Style.paddingS

        TabBar {
            id: notifyTabs
            Layout.fillWidth: true
            currentIndex: 0
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.panelBg
                border.color: Ui.Style.borderSubtle
            }
            TabButton {
                text: Ui.I18n.t("dialog.notifications.tab.requests") +
                      " (" + Ui.AppStore.friendRequestsModel.count + ")"
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: notifyTabs.currentIndex === 0 ? Ui.Style.dialogSelectedBg : "transparent"
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 11
                    color: notifyTabs.currentIndex === 0 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            TabButton {
                text: Ui.I18n.t("dialog.notifications.tab.invites") +
                      " (" + Ui.AppStore.groupInvitesModel.count + ")"
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: notifyTabs.currentIndex === 1 ? Ui.Style.dialogSelectedBg : "transparent"
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 11
                    color: notifyTabs.currentIndex === 1 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            TabButton {
                text: Ui.I18n.t("dialog.notifications.tab.notices") +
                      " (" + Ui.AppStore.noticesModel.count + ")"
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: notifyTabs.currentIndex === 2 ? Ui.Style.dialogSelectedBg : "transparent"
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 11
                    color: notifyTabs.currentIndex === 2 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: notifyTabs.currentIndex

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Ui.Style.radiusLarge
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle

                        ListView {
                            id: requestsList
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            model: Ui.AppStore.friendRequestsModel
                            clip: true
                            spacing: Ui.Style.paddingS
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                            visible: Ui.AppStore.friendRequestsModel.count > 0

                            delegate: Item {
                                width: requestsList.width
                                height: 60
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Ui.Style.radiusMedium
                                    color: mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: Ui.Style.paddingS

                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: Ui.Style.avatarColor(username)
                                        Text {
                                            anchors.centerIn: parent
                                            text: username.length > 0 ? username.charAt(0).toUpperCase() : "?"
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: username
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: remark
                                            visible: remark.length > 0
                                            color: Ui.Style.textMuted
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.notifications.reject")
                                        Layout.preferredWidth: 52
                                        height: 24
                                        onClicked: Ui.AppStore.respondFriendRequest(username, false)
                                    }
                                    Components.PrimaryButton {
                                        text: Ui.I18n.t("dialog.notifications.accept")
                                        Layout.preferredWidth: 52
                                        height: 24
                                        onClicked: Ui.AppStore.respondFriendRequest(username, true)
                                    }
                                }
                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: Ui.AppStore.friendRequestsModel.count === 0
                            text: Ui.I18n.t("dialog.notifications.emptyRequests")
                            color: Ui.Style.textMuted
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Ui.Style.radiusLarge
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle

                        ListView {
                            id: invitesList
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            model: Ui.AppStore.groupInvitesModel
                            clip: true
                            spacing: Ui.Style.paddingS
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                            visible: Ui.AppStore.groupInvitesModel.count > 0

                            delegate: Item {
                                width: invitesList.width
                                height: 72
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Ui.Style.radiusMedium
                                    color: inviteMouse.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: Ui.Style.paddingS

                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: Ui.Style.avatarColor(groupId)
                                        Text {
                                            anchors.centerIn: parent
                                            text: groupId.length > 0 ? groupId.charAt(0).toUpperCase() : "G"
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: Ui.AppStore.resolveTitle(groupId)
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: fromUser.length > 0
                                                  ? Ui.I18n.format("dialog.notifications.invitedBy", fromUser)
                                                  : Ui.I18n.t("dialog.notifications.invitedByUnknown")
                                            color: Ui.Style.textMuted
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.notifications.copyId")
                                        Layout.preferredWidth: 60
                                        height: 24
                                        onClicked: Ui.AppStore.copyGroupInviteId(groupId)
                                    }
                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.notifications.ignore")
                                        Layout.preferredWidth: 52
                                        height: 24
                                        onClicked: Ui.AppStore.ignoreGroupInvite(key)
                                    }
                                    Components.PrimaryButton {
                                        text: Ui.I18n.t("dialog.notifications.join")
                                        Layout.preferredWidth: 52
                                        height: 24
                                        onClicked: Ui.AppStore.joinGroupInvite(key)
                                    }
                                }
                                MouseArea {
                                    id: inviteMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: Ui.AppStore.groupInvitesModel.count === 0
                            text: Ui.I18n.t("dialog.notifications.emptyInvites")
                            color: Ui.Style.textMuted
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Ui.Style.radiusLarge
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle

                        ListView {
                            id: noticesList
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            model: Ui.AppStore.noticesModel
                            clip: true
                            spacing: Ui.Style.paddingS
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                            visible: Ui.AppStore.noticesModel.count > 0

                            delegate: Item {
                                width: noticesList.width
                                height: 72
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Ui.Style.radiusMedium
                                    color: noticeMouse.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: Ui.Style.paddingS

                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: Ui.Style.panelBgAlt
                                        border.color: Ui.Style.borderSubtle
                                        Image {
                                            anchors.centerIn: parent
                                            source: "qrc:/mi/e2ee/ui/icons/info.svg"
                                            width: 16
                                            height: 16
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: title
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: detail
                                            color: Ui.Style.textMuted
                                            font.pixelSize: 10
                                            wrapMode: Text.Wrap
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.notifications.dismiss")
                                        Layout.preferredWidth: 52
                                        height: 24
                                        onClicked: Ui.AppStore.dismissNotice(key)
                                    }
                                }
                                MouseArea {
                                    id: noticeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: Ui.AppStore.noticesModel.count === 0
                            text: Ui.I18n.t("dialog.notifications.emptyNotices")
                            color: Ui.Style.textMuted
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
