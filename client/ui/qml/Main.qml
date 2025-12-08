import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.3
import "Style.qml" as Style

ApplicationWindow {
    id: root
    width: 1100
    height: 700
    visible: true
    title: qsTr("MI E2EE Client")
    color: Style.bgDark
    property color accent: Style.accent
    font.family: Style.fontFamily

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0e141f" }
            GradientStop { position: 1.0; color: "#0a0e15" }
        }
    }

    ListModel { id: logModel }
    ListModel {
        id: groupModel
        ListElement { name: "全局公告"; status: "online" }
        ListElement { name: "安全群"; status: "online" }
        ListElement { name: "工作群"; status: "offline" }
    }
    ListModel {
        id: messageModel
        ListElement { sender: "sys"; text: "欢迎来到 MI E2EE Client"; time: "09:30:00" }
        ListElement { sender: "sys"; text: "请选择群组并尝试发送触发消息"; time: "09:30:05" }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("选择要上传的文件")
        nameFilters: ["All Files (*)"]
        onAccepted: {
            var g = groupList.currentIndex >=0 ? groupModel.get(groupList.currentIndex).name : qsTr("未命名群")
            if (clientBridge.dummySendFile(g, fileDialog.fileUrl)) {
                var t = Qt.formatTime(new Date(), "hh:mm:ss")
                logModel.append({ time: t, msg: qsTr("文件已投递: ") + g })
                messageModel.append({ sender: "sys", text: qsTr("文件上传 -> ") + g, time: t })
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 左侧侧边栏
        Rectangle {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            radius: 14
            color: "#0f1520"
            border.color: "#1b2434"
            layer.enabled: true
            layer.blurRadius: 12
            layer.color: "#080b12"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label { text: qsTr("账户"); color: "#a4c8ff"; font.pixelSize: 16; font.family: "Microsoft YaHei" }
                TextField {
                    id: username
                    placeholderText: qsTr("用户名")
                    color: "white"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#0f1520"; radius: 8; border.color: "#1f2b3d" }
                }
                TextField {
                    id: password
                    placeholderText: qsTr("密码")
                    echoMode: TextInput.Password
                    color: "white"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#0f1520"; radius: 8; border.color: "#1f2b3d" }
                }
                Button {
                    Layout.fillWidth: true
                    onClicked: {
                        if (clientBridge.loggedIn) {
                            clientBridge.logout()
                        } else {
                            clientBridge.login(username.text, password.text)
                        }
                    }
                    contentItem: Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Image { source: clientBridge.loggedIn ? "qrc:/qt/qml/mi/e2ee/ui/icons/logout.svg" : "qrc:/qt/qml/mi/e2ee/ui/icons/login.svg"; width: 20; height: 20 }
                        Text { text: clientBridge.loggedIn ? qsTr("退出") : qsTr("登录"); color: "white"; verticalAlignment: Text.AlignVCenter }
                    }
                    background: Rectangle { radius: 8; color: clientBridge.loggedIn ? "#d95c5c" : root.accent }
                }

                Rectangle { height: 1; color: "#1f2b3d"; Layout.fillWidth: true }

                Label { text: qsTr("群组"); color: "#a4c8ff"; font.pixelSize: 16; font.family: "Microsoft YaHei" }
                TextField {
                    id: groupId
                    placeholderText: qsTr("群组 ID")
                    color: "white"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#0f1520"; radius: 8; border.color: "#1f2b3d" }
                }
                Button {
                    Layout.fillWidth: true
                    onClicked: {
                        if (clientBridge.joinGroup(groupId.text)) {
                            var exists = false
                            for (var i = 0; i < groupModel.count; ++i) {
                                if (groupModel.get(i).name === groupId.text) { exists = true; break; }
                            }
                            if (!exists && groupId.text.length > 0) {
                                groupModel.append({ name: groupId.text, status: "online" })
                            }
                        } else {
                            var t = Qt.formatTime(new Date(), "hh:mm:ss")
                            logModel.append({ time: t, msg: qsTr("加入群失败: ") + groupId.text })
                        }
                    }
                    contentItem: Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/group.svg"; width: 20; height: 20 }
                        Text { text: qsTr("加入群"); color: "white"; verticalAlignment: Text.AlignVCenter }
                    }
                    background: Rectangle { radius: 8; color: root.accent }
                }

                Rectangle { height: 1; color: "#1f2b3d"; Layout.fillWidth: true }

                Label { text: qsTr("离线/文件"); color: "#a4c8ff"; font.pixelSize: 16; font.family: "Microsoft YaHei" }
                Button {
                    enabled: true
                    Layout.fillWidth: true
                    onClicked: fileDialog.open()
                    contentItem: Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/file-upload.svg"; width: 20; height: 20 }
                        Text { text: qsTr("文件上传（占位）"); color: "white"; verticalAlignment: Text.AlignVCenter }
                    }
                    background: Rectangle { radius: 8; color: root.accent }
                }
                Button {
                    enabled: true
                    Layout.fillWidth: true
                    onClicked: {
                        var g = groupList.currentIndex >=0 ? groupModel.get(groupList.currentIndex).name : qsTr("未命名群")
                        var msgs = clientBridge.pullOfflineDummy(g)
                        var t = Qt.formatTime(new Date(), "hh:mm:ss")
                        logModel.append({ time: t, msg: qsTr("离线拉取占位: ") + g + qsTr(" 条数=") + msgs.length })
                        for (var i = 0; i < msgs.length; ++i) {
                            messageModel.append({ sender: "peer", text: qsTr("离线: ") + msgs[i], time: t })
                        }
                    }
                    contentItem: Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/offline.svg"; width: 20; height: 20 }
                        Text { text: qsTr("拉取离线（占位）"); color: "white"; verticalAlignment: Text.AlignVCenter }
                    }
                    background: Rectangle { radius: 8; color: "#22324d" }
                }
            }
        }

        // 右侧区域：群列表 + 聊天/状态
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: "#101726"
            border.color: "#1b2434"
            layer.enabled: true
            layer.blurRadius: 16
            layer.color: "#080b12"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // 群列表
                Rectangle {
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    radius: 10
                    color: "#0f1520"
                    border.color: "#1f2b3d"

                    ListView {
                        id: groupList
                        anchors.fill: parent
                        model: groupModel
                        clip: true
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 52
                            color: ListView.isCurrentItem ? "#1b2a42" : "transparent"
                            border.color: "#1f2b3d"
                            Row {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10
                                Rectangle {
                                    width: 12; height: 12; radius: 6
                                    color: status === "online" ? "#6ddf89" : "#b0b7c6"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: name
                                    color: "#e9f2ff"
                                    font.pixelSize: 14
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: groupList.currentIndex = index
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Component.onCompleted: currentIndex = groupModel.count > 0 ? 0 : -1
                    }
                }

                // 聊天区
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    // 顶部状态栏
                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true
                        Rectangle {
                            radius: 8; color: "#0f1520"; border.color: "#1f2b3d"
                            Layout.fillWidth: true; Layout.preferredHeight: 40
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: clientBridge.loggedIn ? "#6ddf89" : "#d95c5c"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    text: qsTr("当前群: ") + (groupList.currentIndex >= 0 ? groupModel.get(groupList.currentIndex).name : qsTr("未选择"))
                                    color: "#a4c8ff"; elide: Text.ElideRight; Layout.fillWidth: true; font.family: "Microsoft YaHei"
                                }
                                Label { text: qsTr("Token: ") + clientBridge.token; color: "#a4c8ff"; elide: Text.ElideRight; Layout.fillWidth: true; font.family: "Microsoft YaHei" }
                                Label { id: statusLabel; text: ""; color: "#9edb8b"; font.family: "Microsoft YaHei" }
                            }
                        }
                        Rectangle {
                            radius: 8; color: "#0f1520"; border.color: "#1f2b3d"
                            Layout.preferredWidth: 260; Layout.preferredHeight: 40
                            RowLayout { anchors.fill: parent; anchors.margins: 10; spacing: 8
                                Label { text: qsTr("轮换阈值"); color: "#9fb6d8" }
                                SpinBox {
                                    id: threshold
                                    from: 0; to: 100000; value: 10000; editable: true
                                    Layout.fillWidth: true
                                    palette.text: "white"
                                }
                            }
                        }
                        Button {
                            text: qsTr("触发发送")
                            onClicked: {
                                clientBridge.sendGroupMessage(groupId.text, threshold.value)
                                var g = groupId.text.length > 0 ? groupId.text : (groupList.currentIndex >=0 ? groupModel.get(groupList.currentIndex).name : "未命名群")
                                messageModel.append({ sender: "me", text: qsTr("发送触发/阈值=") + threshold.value + " (" + g + ")", time: Qt.formatTime(new Date(), "hh:mm:ss") })
                            }
                            contentItem: Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/send.svg"; width: 18; height: 18 }
                                Text { text: parent.text; color: "white"; verticalAlignment: Text.AlignVCenter }
                            }
                            background: Rectangle { radius: 8; color: root.accent }
                        }
                        Button {
                            text: qsTr("清理日志")
                            onClicked: {
                                logModel.clear()
                            }
                            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 8; color: "#22324d" }
                        }
                        Button {
                            text: qsTr("模拟收到")
                            onClicked: {
                                var g = groupList.currentIndex >=0 ? groupModel.get(groupList.currentIndex).name : qsTr("未命名群")
                                var t = Qt.formatTime(new Date(), "hh:mm:ss")
                                var sample = qsTr("来自群「") + g + qsTr("」的演示消息")
                                messageModel.append({ sender: "peer", text: sample, time: t })
                                logModel.append({ time: t, msg: qsTr("模拟收到消息: ") + g })
                            }
                            contentItem: Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/offline.svg"; width: 18; height: 18 }
                                Text { text: parent.text; color: "white"; verticalAlignment: Text.AlignVCenter }
                            }
                            background: Rectangle { radius: 8; color: "#22324d" }
                        }
                        Button {
                            text: qsTr("关于")
                            onClicked: {
                                var t = Qt.formatTime(new Date(), "hh:mm:ss")
                                var info = clientBridge.serverInfo() + " / " + clientBridge.version()
                                logModel.append({ time: t, msg: qsTr("关于: ") + info })
                                messageModel.append({ sender: "sys", text: info, time: t })
                            }
                            contentItem: Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/group.svg"; width: 18; height: 18 }
                                Text { text: parent.text; color: "white"; verticalAlignment: Text.AlignVCenter }
                            }
                            background: Rectangle { radius: 8; color: "#22324d" }
                        }
                        Button {
                            text: qsTr("清空聊天")
                            onClicked: {
                                messageModel.clear()
                                clientBridge.clearMessages()
                            }
                            contentItem: Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Image { source: "qrc:/qt/qml/mi/e2ee/ui/icons/logout.svg"; width: 18; height: 18 }
                                Text { text: parent.text; color: "white"; verticalAlignment: Text.AlignVCenter }
                            }
                            background: Rectangle { radius: 8; color: "#22324d" }
                        }
                    }

                    // 输入区
                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                TextField {
                    id: messageInput
                    placeholderText: qsTr("输入消息（本地展示，后端仍走触发/轮换路径）")
                    color: "#e9f2ff"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#0f1520"; radius: 10; border.color: "#1f2b3d" }
                }
                        Button {
                            text: qsTr("发送消息")
                            onClicked: {
                                if (messageInput.text.length === 0) return
                                var g = groupList.currentIndex >=0 ? groupModel.get(groupList.currentIndex).name : qsTr("未命名群")
                                messageModel.append({ sender: "me", text: messageInput.text + " (" + g + ")", time: Qt.formatTime(new Date(), "hh:mm:ss") })
                                clientBridge.sendGroupMessage(g, threshold.value)
                                messageInput.text = ""
                            }
                            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 8; color: root.accent }
                        }
                    }

                    // 消息列表
                    Rectangle {
                        radius: 10
                        color: "#0f1520"
                        border.color: "#1f2b3d"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        layer.enabled: true
                        layer.blurRadius: 10
                        layer.color: "#0a0f17"

                        ListView {
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true
                            model: messageModel
                            id: messageList
                            onCountChanged: positionViewAtEnd()
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            delegate: Item {
                                width: ListView.view.width
                                height: bubbleRow.implicitHeight + 12
                                Row {
                                    id: bubbleRow
                                    anchors.left: sender === "me" ? undefined : parent.left
                                    anchors.right: sender === "me" ? parent.right : undefined
                                    anchors.margins: 6
                                    spacing: 8
                                    layoutDirection: sender === "me" ? Qt.RightToLeft : Qt.LeftToRight
                                    Rectangle {
                                        width: 32; height: 32; radius: 16
                                        color: sender === "me" ? "#5bd0ff" : "#7aa0c8"
                                        anchors.verticalCenter: bubble.verticalCenter
                                        opacity: 0.0
                                        Behavior on opacity { NumberAnimation { duration: 180 } }
                                        Component.onCompleted: opacity = 1.0
                                        Text { anchors.centerIn: parent; text: sender === "me" ? qsTr("Me") : qsTr("S"); color: "#0f1520"; font.bold: true }
                                    }
                                    Rectangle {
                                        id: bubble
                                        radius: 10
                                        color: sender === "me" ? "#162f4a" : "#22324d"
                                        border.color: "#1f2b3d"
                                        anchors.verticalCenter: parent.verticalCenter
                                        opacity: 0.0
                                        Behavior on opacity { NumberAnimation { duration: 220 } }
                                        Component.onCompleted: opacity = 1.0
                                        layer.enabled: true
                                        layer.blurRadius: 8
                                        layer.color: sender === "me" ? "#0d1a2a" : "#111a2a"
                                        Column {
                                            padding: 10
                                            spacing: 4
                                            Text { text: text; color: "#e9f2ff"; wrapMode: Text.WordWrap; width: 520 }
                                            Text { text: time; color: "#7aa0c8"; font.pixelSize: 11 }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: clientBridge
        function onStatus(message) {
            statusLabel.text = message
            var t = Qt.formatTime(new Date(), "hh:mm:ss")
            logModel.append({ time: t, msg: message })
            messageModel.append({ sender: "sys", text: message, time: t })
        }
    }
}
