import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/auth" as Auth
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

ApplicationWindow {
    id: root
    property bool authMode: Ui.AppStore.currentPage === 0
    property int authWidth: 520
    property int authHeight: 620

    width: authMode ? authWidth : 1200
    height: authMode ? authHeight : 760
    minimumWidth: authMode ? authWidth : 900
    minimumHeight: authMode ? authHeight : 620
    maximumWidth: authMode ? authWidth : 16384
    maximumHeight: authMode ? authHeight : 16384
    flags: Qt.FramelessWindowHint | Qt.Window
    visible: true
    title: Ui.I18n.t("app.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function toggleMaximize() {
        if (root.visibility === Window.Maximized) {
            root.showNormal()
        } else {
            root.showMaximized()
        }
    }

    Rectangle {
        id: windowFrame
        anchors.fill: parent
        color: authMode ? "transparent" : Ui.Style.windowBg
        radius: 20
        border.color: authMode ? "transparent" : Ui.Style.borderSubtle
        border.width: authMode ? 0 : 1
        antialiasing: true
        clip: true
        layer.enabled: true
        layer.smooth: true

        StackLayout {
            id: rootStack
            anchors.fill: parent
            currentIndex: Ui.AppStore.currentPage === 0 ? 0 : 1

            Auth.AuthFlow {
                id: authFlow
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Item {
                id: appContainer
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        id: appTitleBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        Layout.minimumHeight: 26
                        Layout.maximumHeight: 26
                        color: Ui.Style.panelBg
                        property bool dragging: false
                        property point pressPos: Qt.point(0, 0)
                        property point windowPos: Qt.point(0, 0)

                        function hitButton(button, x, y) {
                            var p = button.mapToItem(appTitleBar, 0, 0)
                            return x >= p.x && x <= p.x + button.width &&
                                   y >= p.y && y <= p.y + button.height
                        }

                        DragHandler {
                            id: appDrag
                            target: null
                            acceptedButtons: Qt.LeftButton
                            grabPermissions: PointerHandler.CanTakeOverFromItems
                            property bool manualDrag: false
                            property point windowPos: Qt.point(0, 0)

                            onActiveChanged: {
                                if (!active) {
                                    manualDrag = false
                                    return
                                }
                                manualDrag = true
                                if (root.startSystemMove && root.startSystemMove()) {
                                    manualDrag = false
                                } else {
                                    windowPos = Qt.point(root.x, root.y)
                                }
                            }

                            onTranslationChanged: {
                                if (!manualDrag) {
                                    return
                                }
                                root.x = windowPos.x + translation.x
                                root.y = windowPos.y + translation.y
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            propagateComposedEvents: true
                            onDoubleClicked: root.toggleMaximize()
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 6
                            z: 1

                            Item { Layout.fillWidth: true }

                            ToolButton {
                                id: minButton
                                hoverEnabled: true
                                implicitWidth: 28
                                implicitHeight: 18
                                onClicked: root.showMinimized()
                                background: Rectangle {
                                    radius: 6
                                    color: minButton.down ? Ui.Style.pressedBg
                                                          : (minButton.hovered ? Ui.Style.hoverBg : "transparent")
                                }
                                contentItem: Item {
                                    width: 12
                                    height: 12
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 2
                                        radius: 1
                                        color: minButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                    }
                                }
                            }

                            ToolButton {
                                id: maxButton
                                hoverEnabled: true
                                implicitWidth: 28
                                implicitHeight: 18
                                onClicked: root.toggleMaximize()
                                background: Rectangle {
                                    radius: 6
                                    color: maxButton.down ? Ui.Style.pressedBg
                                                          : (maxButton.hovered ? Ui.Style.hoverBg : "transparent")
                                }
                                contentItem: Item {
                                    width: 12
                                    height: 12
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 8
                                        color: "transparent"
                                        border.width: 1
                                        border.color: maxButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                    }
                                }
                            }

                            ToolButton {
                                id: closeButton
                                hoverEnabled: true
                                implicitWidth: 28
                                implicitHeight: 18
                                onClicked: root.close()
                                background: Rectangle {
                                    radius: 6
                                    color: closeButton.down ? Ui.Style.pressedBg
                                                            : (closeButton.hovered ? Ui.Style.hoverBg : "transparent")
                                }
                                contentItem: Item {
                                    width: 12
                                    height: 12
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 2
                                        radius: 1
                                        rotation: 45
                                        color: closeButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 2
                                        radius: 1
                                        rotation: -45
                                        color: closeButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                    }
                                }
                            }
                        }
                    }

                    Shell.AppShell {
                        id: appShell
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        windowWidth: root.width
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: appShell.focusSearch()
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: appShell.showChatSearch()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: appShell.handleEscape()
    }

    Dialogs.TrustPromptDialog {
        id: trustDialog
        onAccepted: function(pinText) {
            if (!clientBridge) {
                return
            }
            if (mode === "peer") {
                clientBridge.trustPendingPeer(pinText)
            } else {
                clientBridge.trustPendingServer(pinText)
            }
        }
    }

    Connections {
        target: clientBridge
        function onServerTrustRequired(fingerprint, pin) {
            trustDialog.openWith("server", fingerprint, pin, "")
        }
        function onPeerTrustRequired(peer, fingerprint, pin) {
            trustDialog.openWith("peer", fingerprint, pin, peer)
        }
    }
}
