import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: qsTr("MI E2EE Client")
    color: "#0b1018"
    font.family: "Microsoft YaHei UI"

    property string activeConvId: ""
    property bool activeIsGroup: false
    property string statusText: ""

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0b1018" }
            GradientStop { position: 1.0; color: "#0e1624" }
        }
    }

    ListModel { id: friendModel }
    ListModel { id: groupModel }
    ListModel { id: requestModel }
    ListModel { id: messageModel }
    ListModel { id: stickerModel }

    FileDialog {
        id: fileDialog
        title: qsTr("选择要发送的文件")
        nameFilters: ["All Files (*)"]
        onAccepted: {
            if (activeConvId.length > 0) {
                clientBridge.sendFile(activeConvId, fileDialog.selectedFile, activeIsGroup)
            }
        }
    }

    Popup {
        id: stickerPopup
        width: 360
        height: 280
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#101b2a"; radius: 12; border.color: "#1d2a3f" }
        onOpened: refreshStickerModel()

        GridView {
            anchors.fill: parent
            anchors.margins: 12
            cellWidth: 72
            cellHeight: 72
            cacheBuffer: 120
            reuseItems: true
            model: stickerModel
            delegate: Rectangle {
                width: 64; height: 64; radius: 10
                color: "#132034"
                border.color: "#1f2e47"
                property bool inView: {
                    var view = GridView.view
                    if (!view) return true
                    var buffer = 60
                    return (y + height) >= view.contentY - buffer && y <= view.contentY + view.height + buffer
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (activeConvId.length > 0) {
                            clientBridge.sendSticker(activeConvId, model.id, activeIsGroup)
                            stickerPopup.close()
                        }
                    }
                }
                AnimatedImage {
                    anchors.centerIn: parent
                    width: 48; height: 48
                    source: model.path
                    playing: model.animated && inView
                    visible: model.animated
                }
                Image {
                    anchors.centerIn: parent
                    width: 48; height: 48
                    source: model.path
                    visible: !model.animated
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Left panel
        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            radius: 16
            color: "#0f1623"
            border.color: "#1c2738"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label { text: qsTr("MI E2EE"); font.pixelSize: 20; color: "#cfe2ff" }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1f2b3d" }

                TextField {
                    id: usernameField
                    placeholderText: qsTr("用户名")
                    color: "#e6efff"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#111a28"; radius: 8; border.color: "#1f2b3d" }
                }
                TextField {
                    id: passwordField
                    placeholderText: qsTr("密码")
                    echoMode: TextInput.Password
                    color: "#e6efff"
                    Layout.fillWidth: true
                    background: Rectangle { color: "#111a28"; radius: 8; border.color: "#1f2b3d" }
                }
                Button {
                    Layout.fillWidth: true
                    onClicked: {
                        if (clientBridge.loggedIn) {
                            clientBridge.logout()
                        } else {
                            clientBridge.login(usernameField.text, passwordField.text)
                        }
                    }
                    contentItem: Text {
                        text: clientBridge.loggedIn ? qsTr("退出") : qsTr("登录")
                        color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: clientBridge.loggedIn ? "#cf5a5a" : "#2f6bff"
                    }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1f2b3d" }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: friendAddField
                        placeholderText: qsTr("添加好友")
                        color: "#e6efff"
                        Layout.fillWidth: true
                        background: Rectangle { color: "#111a28"; radius: 8; border.color: "#1f2b3d" }
                    }
                    Button {
                        text: qsTr("发送")
                        onClicked: clientBridge.sendFriendRequest(friendAddField.text, "")
                        background: Rectangle { radius: 8; color: "#2f6bff" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                }

                TabBar {
                    id: listTabs
                    Layout.fillWidth: true
                    TabButton { text: qsTr("好友") }
                    TabButton { text: qsTr("群聊") }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: listTabs.currentIndex

                    // Friends list
                    ListView {
                        id: friendList
                        clip: true
                        cacheBuffer: 120
                        reuseItems: true
                        model: friendModel
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 46
                            radius: 8
                            color: ListView.isCurrentItem ? "#1a2a44" : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10
                                Rectangle { width: 8; height: 8; radius: 4; color: "#65d6a6" }
                                Text { text: model.name; color: "#e6efff"; Layout.fillWidth: true }
                                Rectangle {
                                    visible: model.unread > 0
                                    radius: 8
                                    color: "#ff6b6b"
                                    implicitWidth: 24
                                    implicitHeight: 18
                                    Text { anchors.centerIn: parent; text: model.unread; color: "white"; font.pixelSize: 10 }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    friendList.currentIndex = index
                                    activeConvId = model.id
                                    activeIsGroup = false
                                    loadConversation()
                                }
                            }
                        }
                    }

                    // Group list
                    ListView {
                        id: groupList
                        clip: true
                        cacheBuffer: 120
                        reuseItems: true
                        model: groupModel
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 46
                            radius: 8
                            color: ListView.isCurrentItem ? "#1a2a44" : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10
                                Rectangle { width: 8; height: 8; radius: 4; color: "#6aa9ff" }
                                Text { text: model.name; color: "#e6efff"; Layout.fillWidth: true }
                                Rectangle {
                                    visible: model.unread > 0
                                    radius: 8
                                    color: "#ff6b6b"
                                    implicitWidth: 24
                                    implicitHeight: 18
                                    Text { anchors.centerIn: parent; text: model.unread; color: "white"; font.pixelSize: 10 }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    groupList.currentIndex = index
                                    activeConvId = model.id
                                    activeIsGroup = true
                                    loadConversation()
                                }
                            }
                        }
                    }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1f2b3d" }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: groupJoinField
                        placeholderText: qsTr("群组 ID")
                        color: "#e6efff"
                        Layout.fillWidth: true
                        background: Rectangle { color: "#111a28"; radius: 8; border.color: "#1f2b3d" }
                    }
                    Button {
                        text: qsTr("加入")
                        onClicked: clientBridge.joinGroup(groupJoinField.text)
                        background: Rectangle { radius: 8; color: "#2f6bff" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("创建群聊")
                    onClicked: {
                        var id = clientBridge.createGroup()
                        if (id.length > 0) {
                            groupJoinField.text = id
                        }
                    }
                    background: Rectangle { radius: 8; color: "#20406b" }
                    contentItem: Text { text: parent.text; color: "white" }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1f2b3d" }

                Label { text: qsTr("好友请求"); color: "#9fb6d8" }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 110
                    model: requestModel
                    delegate: RowLayout {
                        width: ListView.view.width
                        spacing: 8
                        Text { text: model.username; color: "#e6efff"; Layout.fillWidth: true }
                        Button {
                            text: qsTr("同意")
                            onClicked: clientBridge.respondFriendRequest(model.username, true)
                            background: Rectangle { radius: 6; color: "#2f6bff" }
                            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12 }
                        }
                        Button {
                            text: qsTr("拒绝")
                            onClicked: clientBridge.respondFriendRequest(model.username, false)
                            background: Rectangle { radius: 6; color: "#2c3646" }
                            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }

        // Right panel
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 16
            color: "#0f1623"
            border.color: "#1c2738"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // Top bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#121c2b"
                        border.color: "#1f2b3d"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            Rectangle { width: 10; height: 10; radius: 5; color: clientBridge.loggedIn ? "#6ddf89" : "#d95c5c" }
                            Text {
                                text: activeConvId.length > 0 ? (activeIsGroup ? qsTr("群聊: ") : qsTr("好友: ")) + activeConvId : qsTr("未选择会话")
                                color: "#cfe2ff"; Layout.fillWidth: true
                            }
                            Text { text: statusText; color: "#9edb8b" }
                        }
                    }
                    Button {
                        text: qsTr("语音")
                        enabled: activeConvId.length > 0 && !activeIsGroup
                        onClicked: clientBridge.startVoiceCall(activeConvId)
                        background: Rectangle { radius: 8; color: "#2f6bff" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                    Button {
                        text: qsTr("视频")
                        enabled: activeConvId.length > 0 && !activeIsGroup
                        onClicked: clientBridge.startVideoCall(activeConvId)
                        background: Rectangle { radius: 8; color: "#2f6bff" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                    Button {
                        text: qsTr("挂断")
                        enabled: clientBridge.activeCallId.length > 0
                        onClicked: clientBridge.endCall()
                        background: Rectangle { radius: 8; color: "#cf5a5a" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                }

                Rectangle {
                    id: videoPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: clientBridge.activeCallVideo ? 220 : 0
                    Layout.maximumHeight: clientBridge.activeCallVideo ? 240 : 0
                    Layout.minimumHeight: clientBridge.activeCallVideo ? 180 : 0
                    visible: clientBridge.activeCallVideo
                    radius: 12
                    color: "#0b1320"
                    border.color: "#1f2b3d"
                    clip: true

                    VideoOutput {
                        id: remoteVideoOutput
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectCrop
                        Component.onCompleted: clientBridge.bindRemoteVideoSink(videoSink)
                    }

                    Rectangle {
                        width: 160
                        height: 90
                        radius: 8
                        color: "#101a28"
                        border.color: "#2b3a50"
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 10
                        clip: true

                        VideoOutput {
                            id: localVideoOutput
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectFit
                            Component.onCompleted: clientBridge.bindLocalVideoSink(videoSink)
                        }
                    }
                }

                // Message list
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#101a28"
                    border.color: "#1f2b3d"

                    ListView {
                        id: messageList
                        anchors.fill: parent
                        anchors.margins: 12
                        model: messageModel
                        clip: true
                        cacheBuffer: 320
                        reuseItems: true
                        spacing: 8
                        onCountChanged: positionViewAtEnd()
                        delegate: Item {
                            width: ListView.view.width
                            height: bubble.implicitHeight + 8
                            property bool inView: {
                                var view = ListView.view
                                if (!view) return true
                                var buffer = 240
                                return (y + height) >= view.contentY - buffer && y <= view.contentY + view.height + buffer
                            }
                            visible: inView
                            Row {
                                anchors.left: outgoing ? undefined : parent.left
                                anchors.right: outgoing ? parent.right : undefined
                                spacing: 8
                                layoutDirection: outgoing ? Qt.RightToLeft : Qt.LeftToRight
                                Rectangle {
                                    id: bubble
                                    radius: 10
                                    color: outgoing ? "#1c3658" : "#1f2b3d"
                                    border.color: "#2b3a50"
                                    layer.enabled: inView && !messageList.moving
                                    Column {
                                        padding: 10
                                        spacing: 6
                                        Text { text: sender; color: "#7aa0c8"; visible: isGroup && !outgoing }
                                        Text {
                                            text: (kind === "text" || kind === "notice" || kind === "system") ? text : ""
                                            color: "#e9f2ff"
                                            wrapMode: Text.WordWrap
                                            visible: kind === "text" || kind === "notice" || kind === "system"
                                            width: 520
                                        }
                                        Row {
                                            spacing: 8
                                            visible: kind === "file"
                                            Text { text: qsTr("文件: ") + fileName; color: "#e9f2ff" }
                                            Text { text: Math.round(fileSize / 1024) + " KB"; color: "#7aa0c8" }
                                        }
                                        Item {
                                            visible: kind === "sticker"
                                            width: 96; height: 96
                                            AnimatedImage {
                                                anchors.centerIn: parent
                                                width: 92; height: 92
                                                source: stickerUrl
                                                playing: stickerAnimated && inView && !messageList.moving
                                                visible: stickerAnimated
                                            }
                                            Image {
                                                anchors.centerIn: parent
                                                width: 92; height: 92
                                                source: stickerUrl
                                                visible: !stickerAnimated
                                            }
                                        }
                                        Row {
                                            spacing: 8
                                            visible: kind === "group_invite"
                                            Text { text: qsTr("群聊邀请"); color: "#e9f2ff" }
                                            Button {
                                                text: qsTr("加入")
                                                onClicked: clientBridge.joinGroup(convId)
                                                background: Rectangle { radius: 6; color: "#2f6bff" }
                                                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12 }
                                            }
                                        }
                                        Row {
                                            spacing: 8
                                            visible: kind === "call_invite"
                                            Text { text: video ? qsTr("视频通话邀请") : qsTr("语音通话邀请"); color: "#e9f2ff" }
                                            Button {
                                                text: qsTr("加入")
                                                visible: !outgoing
                                                onClicked: clientBridge.joinCall(sender, callId, video)
                                                background: Rectangle { radius: 6; color: "#2f6bff" }
                                                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12 }
                                            }
                                        }
                                        Text { text: time; color: "#7aa0c8"; font.pixelSize: 10 }
                                    }
                                }
                            }
                        }
                    }
                }

                // Input
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: messageInput
                        placeholderText: qsTr("输入消息")
                        color: "#e6efff"
                        Layout.fillWidth: true
                        background: Rectangle { color: "#111a28"; radius: 10; border.color: "#1f2b3d" }
                    }
                    Button {
                        text: qsTr("😀")
                        onClicked: stickerPopup.open()
                        background: Rectangle { radius: 8; color: "#22324d" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                    Button {
                        text: qsTr("附件")
                        onClicked: fileDialog.open()
                        background: Rectangle { radius: 8; color: "#22324d" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                    Button {
                        text: qsTr("发送")
                        onClicked: {
                            if (activeConvId.length === 0) return
                            clientBridge.sendText(activeConvId, messageInput.text, activeIsGroup)
                            messageInput.text = ""
                        }
                        background: Rectangle { radius: 8; color: "#2f6bff" }
                        contentItem: Text { text: parent.text; color: "white" }
                    }
                }
            }
        }
    }

    function refreshStickerModel() {
        stickerModel.clear()
        var items = clientBridge.stickerItems()
        for (var i = 0; i < items.length; ++i) {
            stickerModel.append(items[i])
        }
    }

    function refreshFriendModel() {
        friendModel.clear()
        var items = clientBridge.friends
        for (var i = 0; i < items.length; ++i) {
            friendModel.append({ id: items[i].username, name: items[i].remark.length > 0 ? items[i].remark : items[i].username, unread: 0 })
        }
    }

    function refreshGroupModel() {
        groupModel.clear()
        var items = clientBridge.groups
        for (var i = 0; i < items.length; ++i) {
            groupModel.append({ id: items[i].id, name: items[i].name, unread: items[i].unread })
        }
    }

    function refreshRequestModel() {
        requestModel.clear()
        var items = clientBridge.friendRequests
        for (var i = 0; i < items.length; ++i) {
            requestModel.append({ username: items[i].username, remark: items[i].remark })
        }
    }

    function loadConversation() {
        messageModel.clear()
        if (activeConvId.length === 0) {
            return
        }
        var history = clientBridge.loadHistory(activeConvId, activeIsGroup)
        for (var i = 0; i < history.length; ++i) {
            messageModel.append(history[i])
        }
        clearUnread(activeConvId, activeIsGroup)
    }

    function clearUnread(convId, isGroup) {
        var model = isGroup ? groupModel : friendModel
        for (var i = 0; i < model.count; ++i) {
            if (model.get(i).id === convId) {
                model.setProperty(i, "unread", 0)
                break
            }
        }
    }

    function incrementUnread(convId, isGroup) {
        var model = isGroup ? groupModel : friendModel
        for (var i = 0; i < model.count; ++i) {
            if (model.get(i).id === convId) {
                model.setProperty(i, "unread", model.get(i).unread + 1)
                return
            }
        }
        if (isGroup) {
            groupModel.append({ id: convId, name: convId, unread: 1 })
        }
    }

    Component.onCompleted: {
        clientBridge.init("")
        refreshFriendModel()
        refreshGroupModel()
        refreshRequestModel()
    }

    Connections {
        target: clientBridge
        function onStatus(message) { statusText = message }
        function onFriendsChanged() { refreshFriendModel() }
        function onGroupsChanged() { refreshGroupModel() }
        function onFriendRequestsChanged() { refreshRequestModel() }
        function onMessageEvent(message) {
            if (message.convId === activeConvId && message.isGroup === activeIsGroup) {
                messageModel.append(message)
            } else {
                incrementUnread(message.convId, message.isGroup)
            }
        }
    }
}
