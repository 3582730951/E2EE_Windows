import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    anchors.fill: parent

    readonly property bool groupChat: Ui.ChatDisplayStore.currentChatType === "group"
    readonly property string chatTitle: Ui.ChatDisplayStore.currentChatTitle.length > 0
                                        ? Ui.ChatDisplayStore.currentChatTitle
                                        : Ui.I18n.t("right.noChatSelected")
    readonly property string subtitleText: Ui.ChatDisplayStore.currentChatSubtitle
    readonly property string avatarMode: {
        if (groupChat) {
            return "group"
        }
        if ((Ui.ChatDisplayStore.currentChatTitle || "").toLowerCase().indexOf("gateway") !== -1) {
            return "system"
        }
        return "person"
    }
    readonly property string transportTone: Ui.SecurityDisplayStore.transportHealthy ? "healthy" : "blocked"
    readonly property string trustTone: {
        var stateText = (Ui.SecurityDisplayStore.gatewayDisplayState || "").toLowerCase()
        if (stateText.indexOf("pin") !== -1 || stateText.indexOf("固定") !== -1) {
            return "healthy"
        }
        return stateText.length > 0 ? "review" : "checking"
    }
    readonly property string deviceTone: Ui.SecurityDisplayStore.linkedDeviceCount > 0 ? "healthy" : "checking"
    readonly property bool compactMode: root.width <= 208
    readonly property int surfacePadding: root.compactMode ? Ui.Style.paddingS : Ui.Style.paddingM
    readonly property int contentMaxWidth: root.compactMode
                                           ? Math.max(0, root.width - surfacePadding * 2)
                                           : 304
    readonly property string summaryEyebrow: root.groupChat
                                             ? (Ui.I18n.usesCjkLocale ? "群组详情" : "Group details")
                                             : (Ui.I18n.usesCjkLocale ? "聊天详情" : "Conversation details")
    readonly property string summarySubtitle: root.subtitleText.length > 0
                                             ? root.subtitleText
                                             : (root.groupChat
                                                ? (Ui.I18n.usesCjkLocale
                                                   ? "成员、共享文件与安全状态集中在这里。"
                                                   : "Members, shared items, and trust state live here.")
                                                : (Ui.I18n.usesCjkLocale
                                                   ? "共享媒体、位置和文件会在这里归档。"
                                                   : "Shared media, places, and files are collected here."))
    readonly property var runtimeMediaModel: Ui.ChatDisplayStore.sharedMediaModel
    readonly property var runtimeFilesModel: Ui.ChatDisplayStore.sharedFilesModel
    readonly property var runtimeLinksModel: Ui.ChatDisplayStore.sharedLinksModel
    readonly property var overviewCardsModel: {
        if (root.runtimeMediaModel.count > 0) {
            return root.runtimeMediaModel
        }
        if (root.runtimeLinksModel.count > 0) {
            return root.runtimeLinksModel
        }
        if (root.runtimeFilesModel.count > 0) {
            return root.runtimeFilesModel
        }
        return root.runtimeMediaModel
    }
    readonly property var mediaCardsModel: root.runtimeMediaModel
    readonly property var fileCardsModel: root.runtimeFilesModel
    readonly property var linkCardsModel: root.runtimeLinksModel
    readonly property string overviewTabLabel: Ui.I18n.usesCjkLocale ? "概览" : "Overview"
    readonly property string linksTabLabel: Ui.I18n.usesCjkLocale ? "链接" : "Links"
    readonly property var detailTabModel: root.groupChat
                                          ? [
                                                { label: Ui.I18n.t("right.members"), icon: "group.svg" },
                                                { label: Ui.I18n.t("right.media"), icon: "image.svg" },
                                                { label: Ui.I18n.t("right.files"), icon: "file.svg" },
                                                { label: root.linksTabLabel, icon: "info.svg" }
                                            ]
                                          : [
                                                { label: root.overviewTabLabel, icon: "chat.svg" },
                                                { label: Ui.I18n.t("right.media"), icon: "image.svg" },
                                                { label: Ui.I18n.t("right.files"), icon: "file.svg" },
                                                { label: root.linksTabLabel, icon: "info.svg" }
                                            ]

    function detailEntry(modelSource, modelIndex) {
        if (!modelSource) {
            return { entryKind: "file", entryTitle: "", entryDetail: "" }
        }
        if (modelSource.get) {
            return modelSource.get(modelIndex)
        }
        return modelSource[modelIndex]
    }

    function modelCount(modelSource) {
        if (!modelSource) {
            return 0
        }
        if (modelSource.count !== undefined) {
            return modelSource.count
        }
        return modelSource.length !== undefined ? modelSource.length : 0
    }

    function compactTrustSnapshotLabel() {
        var stateText = (Ui.SecurityDisplayStore.gatewayDisplayState || "").toLowerCase()
        if (stateText.indexOf("pin") !== -1 || stateText.indexOf("固定") !== -1) {
            return Ui.I18n.usesCjkLocale ? "固定" : "Pin"
        }
        if (stateText.indexOf("local") !== -1 || stateText.indexOf("本地") !== -1) {
            return Ui.I18n.usesCjkLocale ? "本地" : "Local"
        }
        return Ui.I18n.usesCjkLocale ? "审查" : "Review"
    }

    function compactDeviceSnapshotLabel() {
        return "" + Ui.SecurityDisplayStore.linkedDeviceCount
    }

    function toneAccentColor(tone) {
        switch (tone) {
        case "healthy":
            return Ui.Style.success
        case "blocked":
            return Ui.Style.danger
        case "review":
            return Ui.Style.warning
        default:
            return Ui.Style.accent
        }
    }

    function toneSurfaceColor(tone) {
        switch (tone) {
        case "healthy":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.18 : 0.10)
        case "blocked":
            return Qt.rgba(220 / 255, 38 / 255, 38 / 255, Ui.Style.isDark ? 0.20 : 0.10)
        case "review":
            return Qt.rgba(245 / 255, 158 / 255, 11 / 255, Ui.Style.isDark ? 0.18 : 0.10)
        default:
            return Qt.rgba(51 / 255, 144 / 255, 236 / 255, Ui.Style.isDark ? 0.18 : 0.10)
        }
    }

    function toneBorderColor(tone) {
        switch (tone) {
        case "healthy":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.26 : 0.16)
        case "blocked":
            return Qt.rgba(220 / 255, 38 / 255, 38 / 255, Ui.Style.isDark ? 0.26 : 0.16)
        case "review":
            return Qt.rgba(245 / 255, 158 / 255, 11 / 255, Ui.Style.isDark ? 0.26 : 0.16)
        default:
            return Qt.rgba(51 / 255, 144 / 255, 236 / 255, Ui.Style.isDark ? 0.26 : 0.16)
        }
    }

    component SharedEmptyPanel: ColumnLayout {
        property string iconKind: "chat"
        property string titleText: ""
        property string detailText: ""
        Layout.fillWidth: true
        spacing: 8

        Components.EmptyStateIllustration {
            Layout.alignment: Qt.AlignHCenter
            kind: iconKind
            size: 58
        }

        Text {
            Layout.fillWidth: true
            text: titleText
            color: Ui.Style.textPrimary
            font.pixelSize: 12
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: detailText
            color: Ui.Style.textMuted
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
        }
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
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.round(parent.height * 0.24)
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Ui.Style.sidebarVibrancyTop }
            GradientStop { position: 1.0; color: Ui.Style.sidebarVibrancyBottom }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 1
        color: Ui.Style.sidebarBorder
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: contentColumn
            readonly property int compactPadding: root.surfacePadding
            x: Math.max(compactPadding, Math.round((root.width - width) / 2))
            y: compactPadding
            width: Math.min(root.width - compactPadding * 2, root.contentMaxWidth)
            spacing: root.compactMode ? Ui.Style.paddingS : 14

            Rectangle {
                id: summaryCard
                Layout.fillWidth: true
                radius: Ui.Style.radiusContinuous
                color: Ui.Style.sidebarHeaderSurface
                border.width: 1
                border.color: Ui.Style.sidebarHeaderBorder
                implicitHeight: summaryColumn.implicitHeight + Ui.Style.paddingM * 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 52
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarVibrancyTop
                    opacity: Ui.Style.isDark ? 0.38 : 0.52
                }

                ColumnLayout {
                    id: summaryColumn
                    anchors.fill: parent
                    anchors.margins: root.compactMode ? Ui.Style.paddingS : Ui.Style.paddingM
                    spacing: root.compactMode ? 8 : 8

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.summaryEyebrow
                        color: Ui.Style.sidebarSectionText
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Components.IdentityAvatar {
                        Layout.alignment: Qt.AlignHCenter
                        size: root.compactMode ? 44 : 52
                        titleText: root.chatTitle
                        seedText: Ui.ChatDisplayStore.currentChatId || root.chatTitle
                        mode: root.avatarMode
                        presenceState: root.groupChat ? "secure" : "online"
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        text: root.chatTitle
                        font.pixelSize: root.compactMode ? 13 : 15
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        renderType: Text.NativeRendering
                        antialiasing: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        text: root.summarySubtitle
                        visible: root.summarySubtitle.length > 0
                        font.pixelSize: 11
                        color: Ui.Style.sidebarSubtitleText
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        visible: true
                        spacing: 6

                        Repeater {
                            model: [
                                {
                                    icon: root.groupChat ? "group.svg" : "chat.svg",
                                    value: root.groupChat ? "" + Ui.ChatDisplayStore.currentChatMembers : "1:1",
                                    accent: true
                                },
                                {
                                    icon: "image.svg",
                                    value: "" + root.modelCount(root.mediaCardsModel),
                                    accent: false
                                },
                                {
                                    icon: "file.svg",
                                    value: "" + root.modelCount(root.fileCardsModel),
                                    accent: false
                                },
                                {
                                    icon: "info.svg",
                                    value: "" + root.modelCount(root.linkCardsModel),
                                    accent: false
                                }
                            ]

                            delegate: Rectangle {
                                radius: 10
                                color: modelData.accent ? Ui.Style.railAccentBg : Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: modelData.accent ? Ui.Style.railAccentBorder : Ui.Style.sidebarMetaChipBorder
                                implicitWidth: summaryMetricRow.implicitWidth + 14
                                implicitHeight: 22

                                RowLayout {
                                    id: summaryMetricRow
                                    anchors.centerIn: parent
                                    spacing: 5

                                    Image {
                                        width: 10
                                        height: 10
                                        source: "qrc:/mi/e2ee/ui/icons/" + modelData.icon
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }

                                    Text {
                                        text: modelData.value
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: Ui.Style.textSecondary
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        visible: !root.compactMode
                        spacing: 6

                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/bell.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 14
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("right.mute")
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 14
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("right.search")
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/more.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 14
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.more")
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: !root.compactMode
                radius: Ui.Style.radiusContinuous
                color: Ui.Style.sidebarHeaderSurface
                border.width: 1
                border.color: Ui.Style.sidebarHeaderBorder
                implicitHeight: trustSnapshotRow.implicitHeight + Ui.Style.paddingM * 2

                RowLayout {
                    id: trustSnapshotRow
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusMedium
                        color: root.toneSurfaceColor(root.transportTone)
                        border.width: 1
                        border.color: root.toneBorderColor(root.transportTone)
                        implicitHeight: transportSnapshot.implicitHeight + 12

                        RowLayout {
                            id: transportSnapshot
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Image {
                                width: 12
                                height: 12
                                source: Ui.SecurityDisplayStore.transportHealthy
                                        ? "qrc:/mi/e2ee/ui/icons/check.svg"
                                        : "qrc:/mi/e2ee/ui/icons/info.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: Ui.SecurityDisplayStore.transportHealthy
                                      ? (Ui.I18n.usesCjkLocale ? "正常" : "Ready")
                                      : (Ui.I18n.usesCjkLocale ? "审查" : "Review")
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: root.toneAccentColor(root.transportTone)
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusMedium
                        color: root.toneSurfaceColor(root.trustTone)
                        border.width: 1
                        border.color: root.toneBorderColor(root.trustTone)
                        implicitHeight: trustSnapshot.implicitHeight + 12

                        RowLayout {
                            id: trustSnapshot
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Image {
                                width: 12
                                height: 12
                                source: "qrc:/mi/e2ee/ui/icons/info.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.compactTrustSnapshotLabel()
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: root.toneAccentColor(root.trustTone)
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusMedium
                        color: root.toneSurfaceColor(root.deviceTone)
                        border.width: 1
                        border.color: root.toneBorderColor(root.deviceTone)
                        implicitHeight: deviceSnapshot.implicitHeight + 12

                        RowLayout {
                            id: deviceSnapshot
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Image {
                                width: 12
                                height: 12
                                source: "qrc:/mi/e2ee/ui/icons/device.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.compactDeviceSnapshotLabel()
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: root.toneAccentColor(root.deviceTone)
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: compactDetailCard
                Layout.fillWidth: true
                visible: root.compactMode
                radius: Ui.Style.radiusContinuous
                color: Ui.Style.sidebarHeaderSurface
                border.width: 1
                border.color: Ui.Style.sidebarHeaderBorder
                implicitHeight: compactDetailColumn.implicitHeight + Ui.Style.paddingM * 2

                ColumnLayout {
                    id: compactDetailColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    Repeater {
                        model: root.groupChat ? Math.min(2, Ui.ChatDisplayStore.membersModel.count)
                                              : Math.min(2, root.modelCount(root.overviewCardsModel))

                        delegate: Item {
                            Layout.fillWidth: true
                            implicitHeight: root.groupChat ? 54 : compactMediaCard.implicitHeight

                            RowLayout {
                                anchors.fill: parent
                                spacing: Ui.Style.paddingS
                                visible: root.groupChat

                                Components.IdentityAvatar {
                                    size: 34
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
                                        font.pixelSize: 12
                                        color: Ui.Style.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: role
                                        font.pixelSize: 10
                                        color: Ui.Style.textMuted
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Components.MediaPreviewCard {
                                id: compactMediaCard
                                visible: !root.groupChat
                                anchors.fill: parent
                                compact: true
                                property var cardEntry: root.detailEntry(root.overviewCardsModel, index)
                                kind: cardEntry.entryKind || "file"
                                titleText: cardEntry.entryTitle || ""
                                detailText: cardEntry.entryDetail || ""
                            }
                        }
                    }

                    SharedEmptyPanel {
                        visible: !root.groupChat && root.modelCount(root.overviewCardsModel) === 0
                        iconKind: "chat"
                        titleText: Ui.I18n.t("right.emptySharedTitle")
                        detailText: Ui.I18n.t("right.emptySharedDetail")
                    }
                }
            }

            TabBar {
                id: detailTabsBar
                Layout.fillWidth: true
                visible: !root.compactMode
                currentIndex: 0
                spacing: 4
                background: Rectangle {
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder
                }

                Repeater {
                    model: root.detailTabModel

                    delegate: TabButton {
                        property string tabTooltip: modelData.label
                        hoverEnabled: true
                        focusPolicy: Qt.TabFocus
                        implicitHeight: 34
                        implicitWidth: 38
                        Accessible.name: tabTooltip
                        Keys.onLeftPressed: detailTabsBar.currentIndex = Math.max(0, detailTabsBar.currentIndex - 1)
                        Keys.onRightPressed: detailTabsBar.currentIndex = Math.min(root.detailTabModel.length - 1, detailTabsBar.currentIndex + 1)
                        Keys.onReturnPressed: detailTabsBar.currentIndex = index
                        Keys.onEnterPressed: detailTabsBar.currentIndex = index
                        ToolTip.visible: hovered
                        ToolTip.text: tabTooltip
                        background: Rectangle {
                            radius: Ui.Style.radiusPill
                            color: detailTabsBar.currentIndex === index ? Ui.Style.sidebarNavActiveBg : "transparent"
                            border.width: detailTabsBar.currentIndex === index ? 1 : 0
                            border.color: Ui.Style.sidebarNavActiveBorder
                        }
                        contentItem: Item {
                            implicitWidth: 24
                            implicitHeight: 24
                            Image {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: "qrc:/mi/e2ee/ui/icons/" + modelData.icon
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                                opacity: hovered || detailTabsBar.currentIndex === index ? 1.0 : 0.82
                            }
                        }
                    }
                }
            }

            StackLayout {
                id: detailPanels
                Layout.fillWidth: true
                visible: !root.compactMode
                currentIndex: detailTabsBar.currentIndex

                Rectangle {
                    Layout.fillWidth: true
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder
                    implicitHeight: overviewColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: overviewColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: root.groupChat
                                      ? Ui.I18n.t("right.members")
                                      : root.overviewTabLabel
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Rectangle {
                                radius: 9
                                color: Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: Ui.Style.sidebarMetaChipBorder
                                implicitWidth: overviewCountLabel.implicitWidth + 12
                                implicitHeight: 18

                                Text {
                                    id: overviewCountLabel
                                    anchors.centerIn: parent
                                    text: root.groupChat
                                          ? ("" + Ui.ChatDisplayStore.membersModel.count)
                                          : ("" + root.modelCount(root.overviewCardsModel))
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textSecondary
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        RowLayout {
                            visible: root.groupChat
                            Layout.fillWidth: true
                            spacing: 6

                            Components.IconButton {
                                icon.source: "qrc:/mi/e2ee/ui/icons/plus.svg"
                                accessibleName: Ui.I18n.t("right.add")
                                buttonSize: 30
                                iconSize: 14
                                bgColor: Ui.Style.topBarPillBg
                                hoverBg: Ui.Style.hoverBg
                                pressedBg: Ui.Style.pressedBg
                                ToolTip.visible: hovered
                                ToolTip.text: accessibleName
                            }

                            Components.IconButton {
                                icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                                accessibleName: Ui.I18n.t("right.search")
                                buttonSize: 30
                                iconSize: 14
                                bgColor: Ui.Style.topBarPillBg
                                hoverBg: Ui.Style.hoverBg
                                pressedBg: Ui.Style.pressedBg
                                ToolTip.visible: hovered
                                ToolTip.text: accessibleName
                            }
                        }

                        Repeater {
                            model: root.groupChat ? 0 : root.overviewCardsModel

                            delegate: Components.MediaPreviewCard {
                                property var cardEntry: root.detailEntry(root.overviewCardsModel, index)
                                Layout.fillWidth: true
                                compact: true
                                kind: cardEntry.entryKind || "file"
                                titleText: cardEntry.entryTitle || ""
                                detailText: cardEntry.entryDetail || ""
                            }
                        }

                        SharedEmptyPanel {
                            visible: !root.groupChat && root.modelCount(root.overviewCardsModel) === 0
                            iconKind: "chat"
                            titleText: Ui.I18n.t("right.emptySharedTitle")
                            detailText: Ui.I18n.t("right.emptySharedDetail")
                        }

                        ListView {
                            visible: root.groupChat
                            clip: true
                            model: Ui.ChatDisplayStore.membersModel
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            delegate: Item {
                                width: ListView.view.width
                                height: 56

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Ui.Style.radiusMedium
                                    color: memberMouse.containsMouse ? Ui.Style.sidebarListHoverBg : Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: memberMouse.containsMouse ? Ui.Style.sidebarNavBorder : Ui.Style.sidebarMetaChipBorder
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: Ui.Style.paddingM

                                    Components.IdentityAvatar {
                                        size: 36
                                        titleText: displayName
                                        seedText: avatarKey || displayName
                                        mode: "person"
                                        presenceState: "online"
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
                                            text: role
                                            font.pixelSize: 10
                                            color: Ui.Style.textMuted
                                        }
                                    }
                                }

                                MouseArea {
                                    id: memberMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                        }
                    }
                }

                Rectangle {
                    id: sharedMediaCard
                    Layout.fillWidth: true
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder
                    implicitHeight: sharedMediaColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: sharedMediaColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: Ui.I18n.t("right.media")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Rectangle {
                                radius: 9
                                color: Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: Ui.Style.sidebarMetaChipBorder
                                implicitWidth: sharedMediaCountLabel.implicitWidth + 12
                                implicitHeight: 18

                                Text {
                                    id: sharedMediaCountLabel
                                    anchors.centerIn: parent
                                    text: "" + root.modelCount(root.mediaCardsModel)
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textSecondary
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        Repeater {
                            model: root.mediaCardsModel

                            delegate: Components.MediaPreviewCard {
                                property var cardEntry: root.detailEntry(root.mediaCardsModel, index)
                                Layout.fillWidth: true
                                compact: true
                                kind: cardEntry.entryKind || "file"
                                titleText: cardEntry.entryTitle || ""
                                detailText: cardEntry.entryDetail || ""
                            }
                        }

                        SharedEmptyPanel {
                            visible: root.modelCount(root.mediaCardsModel) === 0
                            iconKind: "chat"
                            titleText: Ui.I18n.t("right.emptyMediaTitle")
                            detailText: Ui.I18n.t("right.emptyMediaDetail")
                        }
                    }
                }

                Rectangle {
                    id: sharedFilesCard
                    Layout.fillWidth: true
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder
                    implicitHeight: sharedFilesColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: sharedFilesColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: Ui.I18n.t("right.files")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Rectangle {
                                radius: 9
                                color: Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: Ui.Style.sidebarMetaChipBorder
                                implicitWidth: sharedFilesCountLabel.implicitWidth + 12
                                implicitHeight: 18

                                Text {
                                    id: sharedFilesCountLabel
                                    anchors.centerIn: parent
                                    text: "" + root.modelCount(root.fileCardsModel)
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textSecondary
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        Repeater {
                            model: root.fileCardsModel

                            delegate: Components.MediaPreviewCard {
                                property var cardEntry: root.detailEntry(root.fileCardsModel, index)
                                Layout.fillWidth: true
                                compact: true
                                kind: cardEntry.entryKind || "file"
                                titleText: cardEntry.entryTitle || ""
                                detailText: cardEntry.entryDetail || ""
                            }
                        }

                        SharedEmptyPanel {
                            visible: root.modelCount(root.fileCardsModel) === 0
                            iconKind: "settings"
                            titleText: Ui.I18n.t("right.emptyFilesTitle")
                            detailText: Ui.I18n.t("right.emptyFilesDetail")
                        }
                    }
                }

                Rectangle {
                    id: sharedLinksCard
                    Layout.fillWidth: true
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.sidebarHeaderSurface
                    border.width: 1
                    border.color: Ui.Style.sidebarHeaderBorder
                    implicitHeight: sharedLinksColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: sharedLinksColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: root.linksTabLabel
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Rectangle {
                                radius: 9
                                color: Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: Ui.Style.sidebarMetaChipBorder
                                implicitWidth: sharedLinksCountLabel.implicitWidth + 12
                                implicitHeight: 18

                                Text {
                                    id: sharedLinksCountLabel
                                    anchors.centerIn: parent
                                    text: "" + root.modelCount(root.linkCardsModel)
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textSecondary
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        Repeater {
                            model: root.linkCardsModel

                            delegate: Components.MediaPreviewCard {
                                property var cardEntry: root.detailEntry(root.linkCardsModel, index)
                                Layout.fillWidth: true
                                compact: true
                                kind: cardEntry.entryKind || "file"
                                titleText: cardEntry.entryTitle || ""
                                detailText: cardEntry.entryDetail || ""
                            }
                        }

                        SharedEmptyPanel {
                            visible: root.modelCount(root.linkCardsModel) === 0
                            iconKind: "security"
                            titleText: Ui.I18n.t("right.emptyLinksTitle")
                            detailText: Ui.I18n.t("right.emptyLinksDetail")
                        }
                    }
                }
            }
        }
    }
}
