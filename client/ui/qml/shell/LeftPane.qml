import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    clip: true

    property bool compactShell: false
    readonly property string shellSurface: Ui.AppStore.currentShellSurface || "chat"
    readonly property bool showConversationList: shellSurface === "chat"
    readonly property bool showContactsList: shellSurface === "contacts"
    readonly property bool showUtilityList: shellSurface === "settings" || shellSurface === "security" || shellSurface === "calls"
    readonly property bool showCallsList: shellSurface === "calls" && !showUtilityCompactRail
    readonly property bool showUtilityCompactRail: showUtilityList
    readonly property bool condensedConversationRows: width <= 300
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property string smokeRailBadgeText: {
        return Ui.I18n.usesCjkLocale ? "最近与常用" : "Recent and pinned"
    }
    readonly property string chatsLabel: Ui.I18n.usesCjkLocale ? "聊天" : "Chats"
    readonly property string contactsLabel: Ui.I18n.t("left.contacts")
    readonly property string callsLabel: Ui.I18n.t("chat.call")
    readonly property string settingsLabel: Ui.I18n.t("settings.title")
    readonly property string settingsDetailLabel: Ui.I18n.usesCjkLocale
                                                  ? "外观、隐私、设备"
                                                  : "Appearance, privacy, devices"
    readonly property string securityDetailLabel: Ui.I18n.usesCjkLocale
                                                  ? "信任与设备"
                                                  : "Trust and devices"
    readonly property string callsDetailLabel: Ui.I18n.usesCjkLocale
                                               ? "最近通话"
                                               : "Recent calls"
    readonly property string utilitySearchPlaceholder: Ui.I18n.usesCjkLocale
                                                       ? "搜索设置或安全项"
                                                       : "Search settings or security"
    readonly property string callsSearchPlaceholder: Ui.I18n.usesCjkLocale
                                                     ? "搜索通话联系人"
                                                     : "Search calls"
    readonly property string shellIdentityTitle: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                                                 ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                                                 : Ui.I18n.t("dialog.deviceManager.currentDevice")
    readonly property string shellIdentityStatusLabel: Ui.SecurityDisplayStore.transportHealthy
                                                       ? Ui.I18n.t("dialog.securityCenter.transportHealthy")
                                                       : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention")
    readonly property int visibleUtilityCount: {
        var visibleCount = 0
        for (var i = 0; i < utilitySectionsModel.length; ++i) {
            if (utilityItemVisible(utilitySectionsModel[i])) {
                visibleCount += 1
            }
        }
        return visibleCount
    }
    readonly property int chromeInset: root.compactShell ? 12 : Ui.Style.sidebarHeaderSideInset
    readonly property int listInset: root.compactShell ? 6 : 8
    readonly property int headerHeight: root.compactShell ? 126 : 140

    readonly property var utilitySectionsModel: [
        {
            title: Ui.I18n.t("settings.title"),
            detail: root.settingsDetailLabel,
            surface: "settings"
        },
        {
            title: Ui.I18n.t("dialog.securityCenter.title"),
            detail: root.securityDetailLabel,
            surface: "security"
        },
        {
            title: Ui.I18n.t("left.deviceManager"),
            detail: Ui.I18n.usesCjkLocale ? "管理已绑定设备" : "Manage linked devices",
            surface: "security"
        }
    ]
    readonly property var compactRailModel: [
        {
            label: root.chatsLabel,
            surface: "chat",
            iconSource: root.surfaceIconSource("chat")
        },
        {
            label: root.contactsLabel,
            surface: "contacts",
            iconSource: root.surfaceIconSource("contacts")
        },
        {
            label: root.callsLabel,
            surface: "calls",
            iconSource: root.surfaceIconSource("calls")
        },
        {
            label: root.settingsLabel,
            surface: "settings",
            iconSource: root.surfaceIconSource("settings")
        },
        {
            label: Ui.I18n.t("dialog.securityCenter.title"),
            surface: "security",
            iconSource: "qrc:/mi/e2ee/ui/icons/check.svg"
        }
    ]

    signal requestNewChat()
    signal requestAddContact()
    signal requestCreateGroup()
    signal requestNotifications()
    signal requestSettings()
    signal requestDeviceManager()

    function focusSearch() {
        searchField.focusInput()
    }

    function clearSearch() {
        searchField.text = ""
        Ui.ChatDisplayStore.setSearchQuery("")
    }

    function handleEscape() {
        if (searchField.inputActiveFocus || searchField.text.length > 0) {
            clearSearch()
            return true
        }
        return false
    }

    function utilityItemVisible(entry) {
        var query = (Ui.ChatDisplayStore.searchQuery || "").toLowerCase()
        if (query.length === 0) {
            return true
        }
        var title = ((entry && entry.title) || "").toLowerCase()
        var detail = ((entry && entry.detail) || "").toLowerCase()
        return title.indexOf(query) !== -1 || detail.indexOf(query) !== -1
    }

    function utilityIconSource(entry) {
        if (!entry) {
            return "qrc:/mi/e2ee/ui/icons/info.svg"
        }
        if (entry.title === Ui.I18n.t("left.deviceManager")) {
            return "qrc:/mi/e2ee/ui/icons/device.svg"
        }
        if (entry.surface === "settings") {
            return "qrc:/mi/e2ee/ui/icons/settings.svg"
        }
        return "qrc:/mi/e2ee/ui/icons/info.svg"
    }

    function surfaceIconSource(surface) {
        switch (surface) {
        case "chat":
            return "qrc:/mi/e2ee/ui/icons/chat.svg"
        case "contacts":
            return "qrc:/mi/e2ee/ui/icons/group.svg"
        case "calls":
            return "qrc:/mi/e2ee/ui/icons/phone.svg"
        case "settings":
        case "security":
            return "qrc:/mi/e2ee/ui/icons/settings.svg"
        default:
            return "qrc:/mi/e2ee/ui/icons/info.svg"
        }
    }

    function utilityBadgeText(entry) {
        if (!entry) {
            return ""
        }
        if (entry.title === Ui.I18n.t("left.deviceManager")) {
            return Ui.SecurityDisplayStore.linkedDeviceCount > 0
                    ? "" + Ui.SecurityDisplayStore.linkedDeviceCount
                    : ""
        }
        if (entry.surface === "security") {
            return Ui.SecurityDisplayStore.transportHealthy
                    ? (Ui.I18n.usesCjkLocale ? "正常" : "OK")
                    : (Ui.I18n.usesCjkLocale ? "注意" : "Alert")
        }
        if (entry.surface === "settings") {
            return ""
        }
        return ""
    }

    function utilityItemActive(entry) {
        if (!entry) {
            return false
        }
        if (entry.title === Ui.I18n.t("left.deviceManager")) {
            return false
        }
        return root.shellSurface === entry.surface
    }

    function avatarModeFor(chatType, titleText, explicitMode) {
        if (explicitMode && explicitMode.length > 0) {
            return explicitMode
        }
        if (chatType === "group") {
            return "group"
        }
        var lowered = (titleText || "").toLowerCase()
        if (lowered.indexOf("gateway") !== -1) {
            return "system"
        }
        return "person"
    }

    function previewKindFor(previewText, explicitKind) {
        if (explicitKind && explicitKind.length > 0) {
            return explicitKind
        }
        var value = (previewText || "").toLowerCase()
        if (value.indexOf(".png") !== -1 ||
                value.indexOf(".jpg") !== -1 ||
                value.indexOf(".jpeg") !== -1 ||
                value.indexOf("screenshot") !== -1 ||
                value.indexOf("image") !== -1) {
            return "photo"
        }
        if (value.indexOf(".zip") !== -1 ||
                value.indexOf(".pdf") !== -1 ||
                value.indexOf(".doc") !== -1 ||
                value.indexOf("[文件]") === 0 ||
                value.indexOf("file") !== -1) {
            return "file"
        }
        if (value.indexOf("voice") !== -1 || value.indexOf("call") !== -1) {
            return "voice"
        }
        if (value.indexOf("http://") !== -1 ||
                value.indexOf("https://") !== -1 ||
                value.indexOf("maps.") !== -1 ||
                value.indexOf("example.") !== -1) {
            return "link"
        }
        return ""
    }

    function previewIconFor(kind) {
        switch (kind) {
        case "photo":
            return "qrc:/mi/e2ee/ui/icons/image.svg"
        case "file":
            return "qrc:/mi/e2ee/ui/icons/file.svg"
        case "voice":
            return "qrc:/mi/e2ee/ui/icons/mic.svg"
        case "link":
            return "qrc:/mi/e2ee/ui/icons/info.svg"
        default:
            return ""
        }
    }

    function previewTintFor(kind) {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.18 : 0.10)
        case "file":
            return Qt.rgba(100 / 255, 116 / 255, 139 / 255, Ui.Style.isDark ? 0.20 : 0.10)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.20 : 0.10)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.20 : 0.10)
        default:
            return "transparent"
        }
    }

    function presenceStateFor(chatType, unreadCount, mutedState, previewText, explicitState) {
        if (explicitState && explicitState.length > 0) {
            return explicitState
        }
        if (mutedState) {
            return "muted"
        }
        if (chatType === "group") {
            return "secure"
        }
        var previewKind = previewKindFor(previewText, "")
        if (previewKind === "voice") {
            return "busy"
        }
        return unreadCount > 0 ? "online" : "secure"
    }

    function headerStatusText() {
        if (showConversationList) {
            return Ui.I18n.usesCjkLocale ? "端到端已启用" : "End-to-end active"
        }
        if (showContactsList) {
            return Ui.I18n.usesCjkLocale ? "联系人" : "Contacts"
        }
        if (showCallsList) {
            return Ui.I18n.usesCjkLocale ? "通话" : "Calls"
        }
        return shellIdentityStatusLabel
    }

    function conversationSectionLabel(modelIndex, pinnedState) {
        if (modelIndex !== 0) {
            return ""
        }
        if (pinnedState) {
            return Ui.I18n.usesCjkLocale ? "置顶" : "Pinned"
        }
        return Ui.I18n.usesCjkLocale ? "最近" : "Recent"
    }

    function searchPlaceholder() {
        if (showCallsList) {
            return callsSearchPlaceholder
        }
        if (showContactsList) {
            return Ui.I18n.t("contacts.searchPlaceholder")
        }
        if (showUtilityList) {
            return utilitySearchPlaceholder
        }
        return Ui.I18n.usesCjkLocale ? "搜索会话" : "Search chats"
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.sidebarSurface
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Ui.Style.sidebarBackdropTop }
            GradientStop { position: 1.0; color: Ui.Style.sidebarBackdropBottom }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.sidebarSurfaceOverlay
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Ui.Style.sidebarBorder
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Ui.Style.sidebarHairline
        opacity: 0.36
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.38)
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Ui.Style.sidebarVibrancyTop }
            GradientStop { position: 1.0; color: Ui.Style.sidebarVibrancyBottom }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Ui.Style.sidebarHairline
        opacity: 0.46
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Ui.Style.sidebarBorder
        opacity: 0.92
        z: 20
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.chromeInset
        anchors.rightMargin: root.chromeInset
        anchors.topMargin: 0
        anchors.bottomMargin: 10
        spacing: 0
        visible: !root.showUtilityCompactRail

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerHeight
            Layout.minimumHeight: root.headerHeight
            Layout.maximumHeight: root.headerHeight

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: root.compactShell ? 18 : Ui.Style.sidebarHeaderTopInset
                anchors.bottomMargin: Ui.Style.sidebarHeaderBottomInset
                spacing: root.compactShell ? 10 : 12

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Ui.Style.sidebarHeaderStatusHeight
                    spacing: 8

                    Rectangle {
                        id: shellIdentityCard
                        Layout.fillWidth: true
                        Layout.preferredHeight: Ui.Style.sidebarHeaderStatusHeight
                        radius: Ui.Style.radiusPill
                        color: Ui.Style.sidebarStatusChipBg
                        border.width: 1
                        border.color: Ui.Style.sidebarStatusChipBorder

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: Ui.Style.badgeSurfaceStrong
                                border.width: 1
                                border.color: Ui.Style.badgeBorder

                                Image {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 10
                                    source: root.surfaceIconSource(root.shellSurface)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.headerStatusText()
                                color: Ui.Style.sidebarStatusText
                                font.pixelSize: Ui.Style.sidebarStatusSize
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                verticalAlignment: Text.AlignVCenter
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }
                        }
                    }

                    Components.IconButton {
                        id: quickComposeButton
                        Accessible.name: Ui.I18n.t("chat.more")
                        visible: root.showConversationList
                        icon.source: "qrc:/mi/e2ee/ui/icons/plus.svg"
                        buttonSize: Ui.Style.sidebarHeaderStatusHeight
                        iconSize: 13
                        bgColor: Ui.Style.sidebarComposeBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: composePopup.popup(quickComposeButton, 0, quickComposeButton.height + 6)
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.more")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Ui.Style.sidebarSearchHeight
                    spacing: 8

                    Components.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        Layout.preferredHeight: Ui.Style.sidebarSearchHeight
                        placeholderText: root.searchPlaceholder()
                        text: Ui.ChatDisplayStore.searchQuery
                        onTextEdited: Ui.ChatDisplayStore.setSearchQuery(text)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Ui.Style.sidebarNavHeight + 8
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 6

                        Repeater {
                            model: [
                                { label: root.chatsLabel, surface: "chat" },
                                { label: root.contactsLabel, surface: "contacts" },
                                { label: root.callsLabel, surface: "calls" },
                                { label: root.settingsLabel, surface: "settings" }
                            ]

                            delegate: Rectangle {
                                readonly property bool active: root.shellSurface === modelData.surface ||
                                                               (modelData.surface === "settings" &&
                                                                root.shellSurface === "security")
                                Layout.fillWidth: true
                                Layout.preferredHeight: Ui.Style.sidebarNavHeight
                                radius: Ui.Style.radiusPill
                                color: active
                                       ? Ui.Style.sidebarNavActiveBg
                                       : (navMouse.containsMouse ? Ui.Style.sidebarNavBg : "transparent")
                                border.width: active || navMouse.containsMouse ? 1 : 0
                                border.color: active
                                              ? Ui.Style.sidebarNavActiveBorder
                                              : Ui.Style.sidebarNavBorder

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 7

                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: active ? Ui.Style.badgeSurfaceStrong : Ui.Style.sidebarMetaChipBg
                                        border.width: 1
                                        border.color: active ? Ui.Style.badgeBorder : Ui.Style.sidebarMetaChipBorder

                                        Image {
                                            anchors.centerIn: parent
                                            width: 10
                                            height: 10
                                            source: root.surfaceIconSource(modelData.surface)
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            antialiasing: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        color: active ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                        font.pixelSize: Ui.Style.sidebarNavLabelSize
                                        font.weight: active ? Font.DemiBold : Font.Medium
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }
                                }

                                ToolTip.visible: navMouse.containsMouse
                                ToolTip.text: modelData.label

                                MouseArea {
                                    id: navMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Ui.AppStore.setShellSurface(modelData.surface)
                                }
                            }
                        }
                    }
                }
            }
        }

        Menu {
            id: menuPopup
            property int compactWidth: 180
            property int compactFontSize: 13
            property int compactPadding: 3
            property int compactSpacing: 3
            property int compactItemHeight: Math.round(compactFontSize + compactPadding * 2 + 4)
            padding: 3
            implicitWidth: compactWidth
            width: compactWidth

            MenuItem {
                id: menuNewGroup
                text: Ui.I18n.t("left.newGroup")
                implicitHeight: menuPopup.compactItemHeight
                height: menuPopup.compactItemHeight
                padding: menuPopup.compactPadding
                spacing: menuPopup.compactSpacing
                onTriggered: root.requestCreateGroup()

                contentItem: Text {
                    anchors.fill: parent
                    text: menuNewGroup.text
                    color: menuNewGroup.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                    font.pixelSize: menuPopup.compactFontSize
                    font.family: Ui.Style.fontFamily
                    renderType: Text.NativeRendering
                    antialiasing: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            MenuItem {
                id: menuAddContact
                text: Ui.I18n.t("left.addContact")
                implicitHeight: menuPopup.compactItemHeight
                height: menuPopup.compactItemHeight
                padding: menuPopup.compactPadding
                spacing: menuPopup.compactSpacing
                onTriggered: root.requestAddContact()

                contentItem: Text {
                    anchors.fill: parent
                    text: menuAddContact.text
                    color: menuAddContact.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                    font.pixelSize: menuPopup.compactFontSize
                    font.family: Ui.Style.fontFamily
                    renderType: Text.NativeRendering
                    antialiasing: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            MenuSeparator { }

            MenuItem {
                id: menuDeviceManager
                text: Ui.I18n.t("left.deviceManager")
                implicitHeight: menuPopup.compactItemHeight
                height: menuPopup.compactItemHeight
                padding: menuPopup.compactPadding
                spacing: menuPopup.compactSpacing
                onTriggered: root.requestDeviceManager()

                contentItem: Text {
                    anchors.fill: parent
                    text: menuDeviceManager.text
                    color: menuDeviceManager.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                    font.pixelSize: menuPopup.compactFontSize
                    font.family: Ui.Style.fontFamily
                    renderType: Text.NativeRendering
                    antialiasing: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            MenuItem {
                id: menuSettings
                text: Ui.I18n.t("left.settings")
                implicitHeight: menuPopup.compactItemHeight
                height: menuPopup.compactItemHeight
                padding: menuPopup.compactPadding
                spacing: menuPopup.compactSpacing
                onTriggered: root.requestSettings()

                contentItem: Text {
                    anchors.fill: parent
                    text: menuSettings.text
                    color: menuSettings.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                    font.pixelSize: menuPopup.compactFontSize
                    font.family: Ui.Style.fontFamily
                    renderType: Text.NativeRendering
                    antialiasing: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }

        Menu {
            id: composePopup
            width: 170

            MenuItem {
                text: Ui.I18n.t("left.newChat")
                onTriggered: root.requestNewChat()
            }
            MenuItem {
                text: Ui.I18n.t("left.newGroup")
                onTriggered: root.requestCreateGroup()
            }
            MenuItem {
                text: Ui.I18n.t("left.addContact")
                onTriggered: root.requestAddContact()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Ui.Style.sidebarSectionDivider
            opacity: 0.85
        }

        ListView {
            id: dialogsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            topMargin: 8
            visible: root.showConversationList
            model: Ui.ChatDisplayStore.filteredDialogsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            spacing: 4
            delegate: dialogDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 5 }
        }

        ListView {
            id: contactsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            topMargin: 8
            visible: root.showContactsList
            model: Ui.ChatDisplayStore.filteredContactsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            spacing: 4
            delegate: contactDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 5 }
        }

        ListView {
            id: callsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            topMargin: 8
            visible: root.showCallsList
            model: Ui.ChatDisplayStore.filteredDialogsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            spacing: 4
            delegate: callDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 5 }
        }

        ListView {
            id: utilityList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            topMargin: 8
            visible: false
            model: root.utilitySectionsModel
            boundsBehavior: Flickable.StopAtBounds
            spacing: 6
            delegate: utilityDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 5 }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.showConversationList && Ui.ChatDisplayStore.filteredDialogsModel.count === 0

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 248)
                spacing: 10

                Components.EmptyStateIllustration {
                    Layout.alignment: Qt.AlignHCenter
                    kind: "chat"
                    size: 64
                }

                Text {
                    Layout.fillWidth: true
                    text: Ui.I18n.usesCjkLocale ? "最近会话" : "Recent chats"
                    color: Ui.Style.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: Ui.I18n.t("chat.empty")
                    color: Ui.Style.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Components.PrimaryButton {
                    text: Ui.I18n.t("left.newChat")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    onClicked: root.requestNewChat()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.showContactsList && Ui.ChatDisplayStore.filteredContactsModel.count === 0

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 248)
                spacing: 10

                Components.EmptyStateIllustration {
                    Layout.alignment: Qt.AlignHCenter
                    kind: "chat"
                    size: 64
                }

                Text {
                    Layout.fillWidth: true
                    text: Ui.I18n.t("contacts.emptyTitle")
                    color: Ui.Style.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: Ui.I18n.t("contacts.emptyHint")
                    color: Ui.Style.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Components.PrimaryButton {
                    text: Ui.I18n.t("left.addContact")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    onClicked: root.requestAddContact()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.showCallsList && Ui.ChatDisplayStore.filteredDialogsModel.count === 0

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 248)
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: root.callsLabel
                    color: Ui.Style.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: Ui.I18n.usesCjkLocale ? "从会话发起或回看通话" : "Start or revisit calls"
                    color: Ui.Style.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Components.PrimaryButton {
                    text: Ui.I18n.t("left.newChat")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    onClicked: {
                        Ui.AppStore.setShellSurface("chat")
                        root.requestNewChat()
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: false

            Text {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 248)
                text: Ui.I18n.usesCjkLocale ? "未找到结果" : "No results"
                color: Ui.Style.textSecondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        spacing: 10
        visible: root.showUtilityCompactRail

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 24
            height: 4
            radius: 2
            color: Ui.Style.sidebarHairline
            opacity: 0.9
        }

        Repeater {
            model: root.compactRailModel

            delegate: Rectangle {
                readonly property bool active: root.shellSurface === modelData.surface
                Layout.alignment: Qt.AlignHCenter
                width: Ui.Style.sidebarCompactRailItemSize
                height: Ui.Style.sidebarCompactRailItemSize
                radius: Ui.Style.radiusContinuous
                color: active ? Ui.Style.sidebarNavActiveBg : Ui.Style.sidebarHeaderSurface
                border.width: 1
                border.color: active ? Ui.Style.sidebarNavActiveBorder : Ui.Style.sidebarHeaderBorder

                Rectangle {
                    anchors.centerIn: parent
                    width: 34
                    height: 34
                    radius: 17
                    color: active ? Ui.Style.badgeSurfaceStrong : Ui.Style.sidebarMetaChipBg
                    border.width: 1
                    border.color: active ? Ui.Style.badgeBorder : Ui.Style.sidebarMetaChipBorder

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: modelData.iconSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }
                }

                ToolTip.visible: compactRailMouse.containsMouse
                ToolTip.text: modelData.label

                MouseArea {
                    id: compactRailMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Ui.AppStore.setShellSurface(modelData.surface)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Component {
        id: contactDelegate

        Item {
            width: ListView.view.width
            height: 64

            Rectangle {
                id: contactCard
                x: root.listInset
                y: 0
                width: parent.width - root.listInset * 2
                height: parent.height
                radius: Ui.Style.radiusListRow
                color: contactMouse.containsMouse ? Ui.Style.sidebarListHoverBg : "transparent"
                border.width: contactMouse.containsMouse ? 1 : 0
                border.color: Ui.Style.sidebarNavBorder
            }

            RowLayout {
                anchors.fill: contactCard
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Components.IdentityAvatar {
                    size: Ui.Style.avatarSizeDialogRow
                    titleText: displayName
                    seedText: avatarKey || displayName
                    mode: "person"
                    presenceState: "online"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: displayName
                        color: Ui.Style.textPrimary
                        font.pixelSize: Ui.Style.sidebarRowTitleSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: usernameOrPhone
                        color: Ui.Style.sidebarSubtitleText
                        font.pixelSize: Ui.Style.sidebarRowPreviewSize
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }
                }

                Components.GhostButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: Ui.I18n.t("contacts.startChat")
                    Layout.preferredHeight: 30
                    onClicked: Ui.ChatDisplayStore.openChatFromContact(contactId)
                }
            }

            MouseArea {
                id: contactMouse
                anchors.fill: contactCard
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Ui.ChatDisplayStore.openChatFromContact(contactId)
            }
        }
    }

    Component {
        id: utilityDelegate

        Item {
            width: ListView.view.width
            height: root.utilityItemVisible(modelData) ? 60 : 0
            visible: root.utilityItemVisible(modelData)

            Rectangle {
                id: utilityCard
                x: root.listInset
                y: 0
                width: parent.width - root.listInset * 2
                height: parent.height
                radius: Ui.Style.radiusListRow
                color: root.utilityItemActive(modelData)
                       ? Ui.Style.sidebarListSelectedBg
                       : (utilityMouse.containsMouse ? Ui.Style.sidebarListHoverBg : "transparent")
                border.width: root.utilityItemActive(modelData) || utilityMouse.containsMouse ? 1 : 0
                border.color: root.utilityItemActive(modelData)
                              ? Ui.Style.sidebarListSelectedBorder
                              : Ui.Style.sidebarNavBorder
            }

            RowLayout {
                anchors.fill: utilityCard
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 17
                    color: Ui.Style.railAccentBg
                    border.width: 1
                    border.color: Ui.Style.railAccentBorder

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        fillMode: Image.PreserveAspectFit
                        source: root.utilityIconSource(modelData)
                        smooth: true
                        antialiasing: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: modelData.title
                        color: Ui.Style.textPrimary
                        font.pixelSize: Ui.Style.sidebarRowTitleSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.detail || ""
                        color: Ui.Style.sidebarSubtitleText
                        font.pixelSize: Ui.Style.sidebarRowPreviewSize
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.utilityBadgeText(modelData).length > 0
                    radius: Ui.Style.radiusPill
                    color: Ui.Style.sidebarMetaChipBg
                    border.width: 1
                    border.color: Ui.Style.sidebarMetaChipBorder
                    implicitWidth: utilityBadgeLabel.implicitWidth + 14
                    implicitHeight: 22

                    Text {
                        id: utilityBadgeLabel
                        anchors.centerIn: parent
                        text: root.utilityBadgeText(modelData)
                        color: Ui.Style.badgeTextPrimary
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }
                }
            }

            MouseArea {
                id: utilityMouse
                anchors.fill: utilityCard
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (modelData.title === Ui.I18n.t("left.deviceManager")) {
                        root.requestDeviceManager()
                        return
                    }
                    Ui.AppStore.setShellSurface(modelData.surface)
                }
            }
        }
    }

    Component {
        id: callDelegate

        Item {
            width: ListView.view.width
            height: Ui.Style.dialogRowHeight

            readonly property bool selected: chatId === Ui.ChatDisplayStore.currentChatId
            readonly property bool activeCall: Ui.CallDisplayStore.activeCallPeer === chatId ||
                                              Ui.CallDisplayStore.incomingCallPeer === chatId
            readonly property string resolvedAvatarMode: root.avatarModeFor(type, title, avatarMode || "")
            readonly property string resolvedPresenceState: activeCall
                                                            ? "busy"
                                                            : root.presenceStateFor(type,
                                                                                    unread,
                                                                                    muted,
                                                                                    preview,
                                                                                    presenceState || "")
            readonly property string callStateText: {
                if (Ui.CallDisplayStore.activeCallId.length > 0 && activeCall) {
                    return Ui.CallDisplayStore.activeCallVideo
                           ? Ui.I18n.t("chat.callActiveVideo")
                           : Ui.I18n.t("chat.callActiveVoice")
                }
                if (Ui.CallDisplayStore.incomingCallActive && activeCall) {
                    return Ui.CallDisplayStore.incomingCallVideo
                           ? Ui.I18n.t("chat.callIncomingVideo")
                           : Ui.I18n.t("chat.callIncomingVoice")
                }
                return preview
            }

            Rectangle {
                id: callCard
                x: root.listInset
                y: 0
                width: parent.width - root.listInset * 2
                height: parent.height
                radius: Ui.Style.radiusListRow
                color: selected
                       ? Ui.Style.sidebarListSelectedBg
                       : (callMouse.containsMouse ? Ui.Style.sidebarListHoverBg : "transparent")
                border.width: selected || callMouse.containsMouse ? 1 : 0
                border.color: selected ? Ui.Style.sidebarListSelectedBorder : Ui.Style.sidebarNavBorder
            }

            RowLayout {
                anchors.fill: callCard
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Components.IdentityAvatar {
                    size: Ui.Style.avatarSizeDialogRow
                    titleText: title
                    seedText: avatarKey || title
                    mode: resolvedAvatarMode
                    presenceState: resolvedPresenceState
                    selected: selected
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: title
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textPrimary
                        font.pixelSize: Ui.Style.sidebarRowTitleSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: callStateText
                        color: activeCall
                               ? Ui.Style.accentSoft
                               : (selected ? Ui.Style.dialogSelectedFg : Ui.Style.sidebarSubtitleText)
                        font.pixelSize: Ui.Style.sidebarRowPreviewSize
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }
                }

                Text {
                    text: timeText
                    color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.sidebarTimestamp
                    font.pixelSize: Ui.Style.sidebarTimestampSize
                    font.weight: Font.Medium
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    renderType: Text.NativeRendering
                    antialiasing: true
                }
            }

            MouseArea {
                id: callMouse
                anchors.fill: callCard
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    Ui.AppStore.setShellSurface("chat")
                    Ui.ChatDisplayStore.setCurrentChat(chatId)
                }
            }
        }
    }

    Component {
        id: dialogDelegate

        Item {
            width: ListView.view.width
            height: Ui.Style.dialogRowHeight + (sectionVisible ? 28 : 0)

            property bool selected: chatId === Ui.ChatDisplayStore.currentChatId
            readonly property string resolvedAvatarMode: root.avatarModeFor(type, title, avatarMode || "")
            readonly property string resolvedPreviewKind: root.previewKindFor(preview, previewKind || "")
            readonly property string resolvedPresenceState: root.presenceStateFor(type,
                                                                                  unread,
                                                                                  muted,
                                                                                  preview,
                                                                                  presenceState || "")
            readonly property bool typingActive: Ui.AppStore.typingByChatId
                                                 && Ui.AppStore.typingByChatId[chatId] === true
            readonly property string sectionLabel: root.conversationSectionLabel(index, pinned)
            readonly property bool sectionVisible: sectionLabel.length > 0
            readonly property bool showSenderAvatar: !root.condensedConversationRows &&
                                                     !typingActive &&
                                                     type === "group" &&
                                                     (lastSenderName || "").length > 0
            readonly property string previewDisplayText: {
                if (typingActive) {
                    return Ui.I18n.usesCjkLocale ? "正在输入…" : "Typing..."
                }
                var basePreview = preview || ""
                if (!root.condensedConversationRows &&
                        type === "group" &&
                        (lastSenderName || "").length > 0 &&
                        basePreview.length > 0) {
                    return lastSenderName + ": " + basePreview
                }
                return basePreview
            }

            function handlePressed(mouse) {
                if (mouse.button === Qt.RightButton) {
                    contextMenu.popup()
                }
            }

            Item {
                id: sectionHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: sectionVisible ? 28 : 0
                visible: sectionVisible

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.listInset + 4
                    anchors.rightMargin: root.listInset + 4
                    spacing: 8

                    Text {
                        text: sectionLabel
                        color: Ui.Style.sidebarSectionText
                        font.pixelSize: Ui.Style.sidebarSectionLabelSize
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Ui.Style.sidebarSectionDivider
                        opacity: 0.55
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            Rectangle {
                id: rowCard
                x: root.listInset
                y: sectionVisible ? 28 : 0
                width: parent.width - root.listInset * 2
                height: Ui.Style.dialogRowHeight
                radius: Ui.Style.radiusListRow
                color: selected
                       ? Ui.Style.sidebarListSelectedBg
                       : (mouseArea.containsMouse ? Ui.Style.sidebarListHoverBg : "transparent")
                border.width: selected || mouseArea.containsMouse ? 1 : 0
                border.color: selected ? Ui.Style.sidebarListSelectedBorder : Ui.Style.sidebarNavBorder

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    color: "transparent"
                    border.width: selected ? 1 : 0
                    border.color: selected ? Ui.Style.alpha(Ui.Style.sidebarHairline, 0.28) : "transparent"
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 34
                    radius: 2
                    visible: selected
                    color: Ui.Style.sidebarSelectionStripe
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    Components.IdentityAvatar {
                        size: Ui.Style.avatarSizeDialogRow
                        titleText: title
                        seedText: avatarKey || title
                        mode: resolvedAvatarMode
                        presenceState: resolvedPresenceState
                        selected: selected
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            Layout.fillWidth: true
                            text: title
                            font.pixelSize: Ui.Style.sidebarRowTitleSize
                            font.weight: Font.DemiBold
                            color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textPrimary
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            renderType: Text.NativeRendering
                            antialiasing: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: showSenderAvatar || resolvedPreviewKind.length > 0 ? 6 : 0

                            Components.IdentityAvatar {
                                visible: showSenderAvatar
                                size: 16
                                titleText: lastSenderName || ""
                                seedText: lastSenderAvatarKey || lastSenderName
                                mode: "person"
                                presenceState: "online"
                            }

                            Rectangle {
                                visible: resolvedPreviewKind.length > 0 && !root.condensedConversationRows
                                Layout.alignment: Qt.AlignVCenter
                                width: 16
                                height: 16
                                radius: 8
                                color: root.previewTintFor(resolvedPreviewKind)
                                border.width: 1
                                border.color: Qt.rgba(1, 1, 1, Ui.Style.isDark ? 0.10 : 0.06)

                                Image {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 10
                                    source: root.previewIconFor(resolvedPreviewKind)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: previewDisplayText
                                font.pixelSize: Ui.Style.sidebarRowPreviewSize
                                font.weight: typingActive ? Font.DemiBold : Font.Normal
                                color: typingActive
                                       ? Ui.Style.accentSoft
                                       : (selected ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary)
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }
                        }
                    }

                    Item {
                        id: metaColumn
                        Layout.preferredWidth: root.condensedConversationRows ? 20 : 30
                        Layout.minimumWidth: root.condensedConversationRows ? 20 : 30
                        Layout.maximumWidth: root.condensedConversationRows ? 20 : 30
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                visible: !root.condensedConversationRows
                                text: timeText
                                font.pixelSize: Ui.Style.sidebarTimestampSize
                                font.weight: unread > 0 ? Font.DemiBold : Font.Medium
                                color: unread > 0
                                       ? Ui.Style.sidebarTimestampUnread
                                       : (selected ? Ui.Style.dialogSelectedFg : Ui.Style.sidebarTimestamp)
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Image {
                                visible: pinned && unread === 0 && !root.condensedConversationRows
                                source: "qrc:/mi/e2ee/ui/icons/star.svg"
                                width: 12
                                height: 12
                                opacity: 0.72
                                fillMode: Image.PreserveAspectFit
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                            }

                            Rectangle {
                                visible: muted && unread === 0 && !root.condensedConversationRows && !pinned
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                width: 6
                                height: 6
                                radius: 3
                                color: Ui.Style.sidebarPinnedTint
                            }

                            Item { Layout.fillHeight: true }

                            Rectangle {
                                visible: unread > 0
                                radius: 9
                                color: muted ? Ui.Style.sidebarUnreadBadgeMutedBg : Ui.Style.sidebarUnreadBadgeBg
                                implicitWidth: Math.max(20, unreadText.paintedWidth + 10)
                                implicitHeight: 18
                                Layout.alignment: Qt.AlignRight | Qt.AlignBottom

                                Text {
                                    id: unreadText
                                    anchors.centerIn: parent
                                    text: unread > 99 ? "99+" : unread
                                    color: muted ? Ui.Style.unreadBadgeMutedFg : Ui.Style.sidebarUnreadBadgeFg
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: rowCard
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Ui.ChatDisplayStore.setCurrentChat(chatId)
                onPressed: handlePressed
            }

            Menu {
                id: contextMenu

                MenuItem {
                    text: pinned ? Ui.I18n.t("left.context.unpin") : Ui.I18n.t("left.context.pin")
                    onTriggered: Ui.ChatDisplayStore.togglePin(chatId)
                }
                MenuItem {
                    text: Ui.I18n.t("left.context.markRead")
                    onTriggered: Ui.ChatDisplayStore.markDialogRead(chatId)
                }
                MenuItem { text: Ui.I18n.t("left.context.mute") }
                MenuItem {
                    text: Ui.I18n.t("left.context.delete")
                    onTriggered: Ui.ChatDisplayStore.removeChat(chatId)
                }
            }
        }
    }
}
