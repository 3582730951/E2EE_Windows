import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    property int leftWidth: Ui.Style.leftPaneWidthDefault
    property int rightWidth: Ui.Style.rightPaneWidth
    property var securityCoordinator: null
    readonly property bool shellReady: true
    readonly property bool hasActiveChat: Ui.ChatDisplayStore.currentChatId.length > 0
    readonly property bool canUseThreeColumn: root.windowWidth >= Ui.Style.threeColumnMinWidth
    readonly property bool rightPaneMounted: hasActiveChat && (canUseThreeColumn || Ui.ChatDisplayStore.rightPaneVisible)
    readonly property Window hostWindow: root.Window.window

    onHasActiveChatChanged: {
        if (!hasActiveChat) {
            Ui.ChatDisplayStore.closeRightPane()
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Ui.Style.tgCloudTop }
            GradientStop { position: 1.0; color: Ui.Style.tgCloudBottom }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            radius: 24
            color: Ui.Style.tgGlassSurface
            border.width: 1
            border.color: Ui.Style.tgCardBorder
            clip: true

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                width: parent.width * 0.44
                height: 120
                color: Ui.Style.tgCardHighlight
                radius: 60
                x: -24
                y: -42
            }

            SplitView {
                id: split
                anchors.fill: parent
                orientation: Qt.Horizontal
                handle: Rectangle {
                    implicitWidth: 1
                    color: Ui.Style.borderSubtle
                    z: 5
                }

                Shell.LeftPane {
                    id: leftPane
                    SplitView.preferredWidth: root.leftWidth
                    SplitView.minimumWidth: Ui.Style.leftPaneWidthMin
                    onWidthChanged: {
                        if (width > 80) {
                            root.leftWidth = width
                        }
                    }
                    onRequestNewChat: newChatDialog.open()
                    onRequestAddContact: addContactDialog.open()
                    onRequestCreateGroup: createGroupWizard.open()
                    onRequestNotifications: notificationDialog.open()
                    onRequestSettings: if (root.securityCoordinator) root.securityCoordinator.openSettings()
                    onRequestDeviceManager: if (root.securityCoordinator) root.securityCoordinator.openDeviceManager()
                }

                Shell.CenterPane {
                    id: centerPane
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: Ui.Style.centerPaneWidthMin
                }

                Loader {
                    id: rightPaneLoader
                    active: root.rightPaneMounted
                    visible: active
                    SplitView.preferredWidth: root.rightWidth
                    SplitView.minimumWidth: Ui.Style.rightPaneWidthMin
                    SplitView.maximumWidth: Ui.Style.rightPaneWidthMax
                    onWidthChanged: {
                        if (active && width > Ui.Style.rightPaneWidthMin) {
                            root.rightWidth = width
                        }
                    }
                    sourceComponent: Shell.RightPane {
                    }
                }
            }
        }
    }

    Dialogs.NewChatDialog {
        id: newChatDialog
        ownerWindow: root.hostWindow
    }
    Dialogs.AddContactDialog {
        id: addContactDialog
        ownerWindow: root.hostWindow
    }
    Dialogs.CreateGroupWizard {
        id: createGroupWizard
        ownerWindow: root.hostWindow
    }
    Dialogs.NotificationCenterDialog {
        id: notificationDialog
        ownerWindow: root.hostWindow
    }
    function focusSearch() {
        leftPane.focusSearch()
    }

    function showChatSearch() {
        centerPane.showSearch()
    }

    function openSecurityCenter() {
        if (root.securityCoordinator) {
            root.securityCoordinator.openSecurityCenter()
        }
    }

    function handleEscape() {
        if (root.securityCoordinator && root.securityCoordinator.handleEscape()) {
            return
        }
        if (createGroupWizard.visible) {
            createGroupWizard.close()
            return
        }
        if (addContactDialog.visible) {
            addContactDialog.close()
            return
        }
        if (newChatDialog.visible) {
            newChatDialog.close()
            return
        }
        if (notificationDialog.visible) {
            notificationDialog.close()
            return
        }
        if (centerPane.clearChatSearch()) {
            return
        }
        if (Ui.ChatDisplayStore.searchQuery.length > 0) {
            leftPane.clearSearch()
            return
        }
    }
}
