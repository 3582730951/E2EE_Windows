import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    property bool showingCallsSurface: false
    property bool showingSettingsSurface: false
    property bool showingSecuritySurface: false
    property int callDurationSec: 0
    readonly property int currentThemeOptionIndex: Math.max(0, Ui.SecurityDisplayStore.themeModeIndex(Ui.Style.themeMode))
    readonly property int currentLocaleOptionIndex: Math.max(0, Ui.SecurityDisplayStore.localeModeIndex(Ui.I18n.localeMode))
    readonly property var currentThemeOption: Ui.SecurityDisplayStore.themeOptions.length > 0
                                              ? Ui.SecurityDisplayStore.themeOptions[Math.min(currentThemeOptionIndex,
                                                                                              Ui.SecurityDisplayStore.themeOptions.length - 1)]
                                              : null
    readonly property var currentLocaleOption: Ui.SecurityDisplayStore.localeOptions.length > 0
                                               ? Ui.SecurityDisplayStore.localeOptions[Math.min(currentLocaleOptionIndex,
                                                                                               Ui.SecurityDisplayStore.localeOptions.length - 1)]
                                               : null

    function utility_friendly_theme_detail() {
        return currentThemeOption && currentThemeOption.label
                ? currentThemeOption.label
                : Ui.I18n.t("settings.theme.system")
    }

    function utility_friendly_locale_detail() {
        return currentLocaleOption && currentLocaleOption.label
                ? currentLocaleOption.label
                : Ui.I18n.t("settings.language")
    }

    function utility_friendly_security_detail() {
        if (Ui.SecurityDisplayStore.transportHealthy) {
            return Ui.I18n.usesCjkLocale ? "已加密并保持连接" : "Encrypted and connected"
        }
        return Ui.I18n.usesCjkLocale ? "需要重新检查会话" : "Session needs review"
    }

    function utility_friendly_trust_detail() {
        var stateText = (Ui.SecurityDisplayStore.gatewayDisplayState || "").toLowerCase()
        if (stateText.indexOf("pin") !== -1 || stateText.indexOf("固定") !== -1 ||
                stateText.indexOf("local") !== -1 || stateText.indexOf("本地") !== -1) {
            return Ui.I18n.usesCjkLocale ? "已验证" : "Verified"
        }
        return Ui.I18n.usesCjkLocale ? "待确认" : "Pending"
    }

    function utility_friendly_gateway_detail() {
        if (Ui.SecurityDisplayStore.gatewayDisplayDetail.length > 0 ||
                Ui.SecurityDisplayStore.gatewayDisplayState.length > 0) {
            return Ui.I18n.usesCjkLocale ? "已连接受信网络" : "Connected over trusted transport"
        }
        return Ui.I18n.usesCjkLocale ? "等待连接" : "Waiting for connection"
    }

    function utility_friendly_device_name(displayId, index, isCurrent) {
        if (isCurrent === true) {
            return Ui.I18n.usesCjkLocale ? "这台设备" : "This device"
        }
        var lowered = (displayId || "").toLowerCase()
        if (lowered.indexOf("pad") !== -1 || lowered.indexOf("tab") !== -1) {
            return Ui.I18n.usesCjkLocale ? "平板" : "Tablet"
        }
        if (lowered.indexOf("desk") !== -1 || lowered.indexOf("lap") !== -1 || lowered.indexOf("pc") !== -1) {
            return Ui.I18n.usesCjkLocale ? "桌面端" : "Desktop"
        }
        return Ui.I18n.usesCjkLocale
                ? ("已连接设备 " + (index + 1))
                : ("Linked device " + (index + 1))
    }

    function utility_call_target_id() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatId
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).chatId || ""
        }
        return ""
    }

    function utility_call_target_title() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatTitle
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).title || ""
        }
        return ""
    }

    function utility_call_target_avatar_seed() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatId
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            var dialogEntry = Ui.ChatDisplayStore.filteredDialogsModel.get(0)
            return dialogEntry.avatarKey || dialogEntry.title || ""
        }
        return ""
    }

    function utility_call_target_avatar_mode() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatType === "group" ? "group" : "person"
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).avatarMode || ""
        }
        return ""
    }

    component UtilityNavRow: Item {
        id: utilityNavRow
        property string iconSource: ""
        property color iconBg: Ui.Style.railAccentBg
        property color iconBorder: Ui.Style.railAccentBorder
        property string titleText: ""
        property string detailText: ""
        property string trailingText: ""
        signal clicked()

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: titleText
        Keys.onReturnPressed: utilityNavRow.clicked()
        Keys.onEnterPressed: utilityNavRow.clicked()
        Keys.onSpacePressed: utilityNavRow.clicked()
        implicitHeight: trailingText.length > 0 || detailText.length === 0 ? 48 : 54

        Rectangle {
            anchors.fill: parent
            radius: Ui.Style.radiusListRow
            color: utilityNavRow.activeFocus || utilityNavMouse.containsMouse
                   ? Ui.Style.sidebarListHoverBg
                   : "transparent"
            border.width: utilityNavRow.activeFocus ? 1 : 0
            border.color: utilityNavRow.activeFocus ? Ui.Style.inputFocus : "transparent"
        }

        Rectangle {
            id: utilityNavIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilityNavRow.iconBg
            border.width: 1
            border.color: utilityNavRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilityNavRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Image {
            id: utilityNavChevron
            width: 12
            height: 12
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "qrc:/mi/e2ee/ui/icons/chevron-right.svg"
            smooth: true
            antialiasing: true
        }

        Text {
            id: utilityNavTrailing
            visible: utilityNavRow.trailingText.length > 0
            width: visible ? 78 : 0
            anchors.right: utilityNavChevron.left
            anchors.rightMargin: visible ? Ui.Style.paddingS : 0
            anchors.verticalCenter: parent.verticalCenter
            text: utilityNavRow.trailingText
            maximumLineCount: 1
            elide: Text.ElideRight
            color: Ui.Style.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
        }

        Column {
            anchors.left: utilityNavIcon.right
            anchors.right: utilityNavTrailing.visible ? utilityNavTrailing.left : utilityNavChevron.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingS
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityNavRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: utilityNavRow.detailText.length > 0 && !utilityNavTrailing.visible
                width: parent.width
                text: utilityNavRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: utilityNavMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                utilityNavRow.forceActiveFocus()
                utilityNavRow.clicked()
            }
        }

    }

    component UtilityToggleRow: Item {
        id: utilityToggleRow
        property string iconSource: ""
        property color iconBg: Ui.Style.topBarPillBg
        property color iconBorder: Ui.Style.topBarPillBorder
        property string titleText: ""
        property string detailText: ""
        property bool checked: false
        signal toggled(bool checked)

        implicitHeight: 72

        Rectangle {
            id: utilityToggleIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilityToggleRow.iconBg
            border.width: 1
            border.color: utilityToggleRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilityToggleRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Components.InlineSwitch {
            id: utilityToggleSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: utilityToggleRow.checked
            onToggled: utilityToggleRow.toggled(checked)
        }

        Column {
            anchors.left: utilityToggleIcon.right
            anchors.right: utilityToggleSwitch.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityToggleRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: utilityToggleRow.detailText.length > 0
                width: parent.width
                text: utilityToggleRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

    }

    component UtilitySummaryRow: Item {
        id: utilitySummaryRow
        property string iconSource: ""
        property color iconBg: Ui.Style.railAccentBg
        property color iconBorder: Ui.Style.railAccentBorder
        property string labelText: ""
        property string valueText: ""

        implicitHeight: Ui.Style.settingsRowMinHeight

        Rectangle {
            id: utilitySummaryIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilitySummaryRow.iconBg
            border.width: 1
            border.color: utilitySummaryRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilitySummaryRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Text {
            id: utilitySummaryValue
            width: 168
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: utilitySummaryRow.valueText
            color: Ui.Style.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        Text {
            anchors.left: utilitySummaryIcon.right
            anchors.right: utilitySummaryValue.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            text: utilitySummaryRow.labelText
            color: Ui.Style.textPrimary
            font.pixelSize: 13
            font.weight: Font.DemiBold
            maximumLineCount: 1
            elide: Text.ElideRight
        }

    }

    component UtilityDeviceRow: Item {
        id: utilityDeviceRow
        property string titleText: ""
        property string detailText: ""

        implicitHeight: 40

        Rectangle {
            id: utilityDeviceIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: Ui.Style.railAccentBg
            border.width: 1
            border.color: Ui.Style.railAccentBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: "qrc:/mi/e2ee/ui/icons/device.svg"
            }
        }

        Column {
            anchors.left: utilityDeviceIcon.right
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityDeviceRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: utilityDeviceRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

    }

    component UtilityCallRow: Item {
        id: utilityCallRow
        property string titleText: ""
        property string subtitleText: ""
        property string timeText: ""
        property string avatarTitle: ""
        property string avatarSeed: ""
        property string avatarMode: ""
        signal clicked()
        signal actionClicked()

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: titleText
        Keys.onReturnPressed: utilityCallRow.clicked()
        Keys.onEnterPressed: utilityCallRow.clicked()
        Keys.onSpacePressed: utilityCallRow.clicked()
        readonly property string detailLine: {
            if (utilityCallRow.subtitleText.length > 0 && utilityCallRow.timeText.length > 0) {
                return utilityCallRow.subtitleText + " · " + utilityCallRow.timeText
            }
            if (utilityCallRow.subtitleText.length > 0) {
                return utilityCallRow.subtitleText
            }
            return utilityCallRow.timeText
        }

        implicitHeight: detailLine.length > 0 ? 54 : 48

        Rectangle {
            anchors.fill: parent
            radius: Ui.Style.radiusListRow
            color: utilityCallRow.activeFocus || utilityCallMouse.containsMouse
                   ? Ui.Style.sidebarListHoverBg
                   : "transparent"
            border.width: utilityCallRow.activeFocus ? 1 : 0
            border.color: utilityCallRow.activeFocus ? Ui.Style.inputFocus : "transparent"
        }

        Components.IdentityAvatar {
            id: utilityCallAvatar
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            size: 38
            titleText: utilityCallRow.avatarTitle
            seedText: utilityCallRow.avatarSeed
            mode: utilityCallRow.avatarMode
            presenceState: "idle"
        }

        Components.IconButton {
            id: utilityCallAction
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            accessibleName: Ui.I18n.t("chat.call")
            icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
            buttonSize: 34
            iconSize: 16
            onClicked: utilityCallRow.actionClicked()
        }

        Column {
            anchors.left: utilityCallAvatar.right
            anchors.right: utilityCallAction.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityCallRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 13
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: detailLine.length > 0
                width: parent.width
                text: detailLine
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: utilityCallMouse
            anchors.fill: parent
            anchors.rightMargin: 42
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                utilityCallRow.forceActiveFocus()
                utilityCallRow.clicked()
            }
        }

    }

    function cycle_theme_option() {
        var options = Ui.SecurityDisplayStore.themeOptions || []
        if (options.length === 0) {
            return
        }
        var nextIndex = (Ui.SecurityDisplayStore.themeModeIndex(Ui.Style.themeMode) + 1 + options.length) % options.length
        Ui.SecurityDisplayStore.setThemeMode(options[nextIndex].mode)
    }

    function cycle_locale_option() {
        var options = Ui.SecurityDisplayStore.localeOptions || []
        if (options.length === 0) {
            return
        }
        var nextIndex = (Ui.SecurityDisplayStore.localeModeIndex(Ui.I18n.localeMode) + 1 + options.length) % options.length
        Ui.SecurityDisplayStore.setLocaleMode(options[nextIndex].code)
    }

    Item {
        id: utilitySurface
        anchors.fill: parent
        visible: root.showingCallsSurface || root.showingSettingsSurface || root.showingSecuritySurface

        ScrollView {
            id: utilityScroll
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingL
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                readonly property int utilityCardWidth: Math.min(width, Ui.Style.utilitySurfaceMaxWidth)
                width: Math.max(0, utilityScroll.availableWidth)
                spacing: Ui.Style.paddingL

                Item {
                    id: utilityPageHeader
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredHeight: 92
                    readonly property bool hideBackAction: utilityScroll.availableWidth >= 700

                    Rectangle {
                        id: utilityBackButton
                        width: 28
                        height: 28
                        radius: 14
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: Ui.I18n.t("dialog.createGroup.back")
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !utilityPageHeader.hideBackAction
                        color: utilityBackButton.activeFocus || utilityBackMouse.containsMouse
                               ? Ui.Style.sidebarListHoverBg
                               : Ui.Style.topBarPillBg
                        border.width: 1
                        border.color: utilityBackButton.activeFocus
                                      ? Ui.Style.inputFocus
                                      : Ui.Style.topBarPillBorder
                        Keys.onReturnPressed: activate()
                        Keys.onEnterPressed: activate()
                        Keys.onSpacePressed: activate()

                        function activate() {
                            if (root.showingSecuritySurface) {
                                Ui.AppStore.setShellSurface("settings")
                            } else {
                                Ui.AppStore.setShellSurface("chat")
                            }
                        }

                        Image {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            source: "qrc:/mi/e2ee/ui/icons/chevron-right.svg"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                            rotation: 180
                        }

                        MouseArea {
                            id: utilityBackMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                utilityBackButton.forceActiveFocus()
                                utilityBackButton.activate()
                            }
                        }
                    }

                    Column {
                        anchors.left: utilityBackButton.visible ? utilityBackButton.right : parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: utilityBackButton.visible ? Ui.Style.paddingM : 0
                        anchors.rightMargin: 0
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Text {
                            id: utilityPageTitle
                            width: parent.width
                            maximumLineCount: 1
                            elide: Text.ElideRight
                            text: root.showingCallsSurface
                                  ? Ui.I18n.t("chat.call")
                                  : (root.showingSecuritySurface
                                     ? Ui.I18n.t("dialog.securityCenter.title")
                                     : Ui.I18n.t("settings.title"))
                            color: Ui.Style.textPrimary
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                            horizontalAlignment: utilityBackButton.visible ? Text.AlignLeft : Text.AlignHCenter
                            renderType: Text.NativeRendering
                            antialiasing: true
                        }

                        Text {
                            width: parent.width
                            maximumLineCount: 1
                            elide: Text.ElideRight
                            text: root.showingCallsSurface
                                  ? (Ui.I18n.usesCjkLocale ? "最近记录与快速发起" : "Recent history and quick launch")
                                  : (root.showingSecuritySurface
                                     ? (Ui.I18n.usesCjkLocale ? "信任、设备与传输状态" : "Trust, devices, and transport status")
                                     : (Ui.I18n.usesCjkLocale ? "外观、语言与隐私偏好" : "Appearance, language, and privacy"))
                            color: Ui.Style.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: utilityBackButton.visible ? Text.AlignLeft : Text.AlignHCenter
                            renderType: Text.NativeRendering
                            antialiasing: true
                        }
                    }
                }

                Rectangle {
                    id: settingsPrimaryList
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.showingSettingsSurface
                    radius: Ui.Style.radiusLarge
                    color: Ui.Style.panelBg
                    border.width: 1
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: settingsPrimaryColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: settingsPrimaryColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingS
                        spacing: 0

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: settingsAccountRow.implicitHeight

                        UtilityNavRow {
                            id: settingsAccountRow
                            anchors.fill: parent
                            iconSource: "qrc:/mi/e2ee/ui/icons/device.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("auth.brand")
                            trailingText: Ui.I18n.usesCjkLocale ? "账号" : "Account"
                        }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: themeSettingsRow.implicitHeight

                            UtilityNavRow {
                            id: themeSettingsRow
                            anchors.fill: parent
                            iconSource: "qrc:/mi/e2ee/ui/icons/palette.svg"
                            iconBg: Ui.Style.railAccentBg
                            iconBorder: Ui.Style.railAccentBorder
                            titleText: Ui.I18n.t("settings.theme")
                                detailText: root.utility_friendly_theme_detail()
                                onClicked: root.cycle_theme_option()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: localeSettingsRow.implicitHeight

                            UtilityNavRow {
                            id: localeSettingsRow
                            anchors.fill: parent
                            iconSource: "qrc:/mi/e2ee/ui/icons/language.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("settings.language")
                                detailText: root.utility_friendly_locale_detail()
                                onClicked: root.cycle_locale_option()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: securitySettingsRow.implicitHeight

                            UtilityNavRow {
                            id: securitySettingsRow
                            anchors.fill: parent
                            iconSource: "qrc:/mi/e2ee/ui/icons/check.svg"
                            iconBg: Ui.Style.railAccentBg
                            iconBorder: Ui.Style.railAccentBorder
                            titleText: Ui.I18n.t("settings.securityCenter.title")
                                detailText: root.utility_friendly_security_detail()
                                onClicked: Ui.AppStore.setShellSurface("security")
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: privacySettingsRow.implicitHeight

                            UtilityNavRow {
                                id: privacySettingsRow
                                anchors.fill: parent
                                iconSource: "qrc:/mi/e2ee/ui/icons/file.svg"
                                iconBg: Ui.Style.topBarPillBg
                                iconBorder: Ui.Style.topBarPillBorder
                                titleText: Ui.I18n.t("settings.section.privacy")
                                detailText: ""
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: notificationsSettingsRow.implicitHeight

                            UtilityNavRow {
                                id: notificationsSettingsRow
                                anchors.fill: parent
                                iconSource: "qrc:/mi/e2ee/ui/icons/bell.svg"
                                iconBg: Ui.Style.railAccentBg
                                iconBorder: Ui.Style.railAccentBorder
                                titleText: Ui.I18n.t("settings.section.notifications")
                                detailText: ""
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: devicesSettingsRow.implicitHeight

                            UtilityNavRow {
                                id: devicesSettingsRow
                                anchors.fill: parent
                                iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                                iconBg: Ui.Style.topBarPillBg
                                iconBorder: Ui.Style.topBarPillBorder
                                titleText: Ui.I18n.t("dialog.deviceManager.linkedDevices")
                                trailingText: Ui.SecurityDisplayStore.devicesModel.count > 0
                                              ? ("" + Ui.SecurityDisplayStore.devicesModel.count)
                                              : "0"
                                onClicked: Ui.AppStore.setShellSurface("security")
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: densitySettingsRow.implicitHeight

                            UtilityNavRow {
                                id: densitySettingsRow
                                anchors.fill: parent
                                iconSource: "qrc:/mi/e2ee/ui/icons/chat.svg"
                                iconBg: Ui.Style.railAccentBg
                                iconBorder: Ui.Style.railAccentBorder
                                titleText: Ui.I18n.t("settings.messageDensity")
                                detailText: Ui.I18n.t("settings.density.normal")
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: fontSizeSettingsRow.implicitHeight

                            UtilityNavRow {
                                id: fontSizeSettingsRow
                                anchors.fill: parent
                                iconSource: "qrc:/mi/e2ee/ui/icons/language.svg"
                                iconBg: Ui.Style.topBarPillBg
                                iconBorder: Ui.Style.topBarPillBorder
                                titleText: Ui.I18n.t("settings.fontSize")
                                detailText: "16 px"
                            }
                        }

                    }
                }

                Rectangle {
                    id: securitySummaryList
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.showingSecuritySurface
                    radius: Ui.Style.radiusLarge
                    color: Ui.Style.panelBg
                    border.width: 1
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: securitySummaryColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: securitySummaryColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingS
                        spacing: 0

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/check.svg"
                            iconBg: Ui.Style.railAccentBg
                            iconBorder: Ui.Style.railAccentBorder
                            titleText: Ui.I18n.t("dialog.securityCenter.transportTitle")
                            trailingText: root.utility_friendly_security_detail()
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                            trailingText: Ui.SecurityDisplayStore.devicesModel.count > 0
                                          ? (Ui.I18n.usesCjkLocale
                                             ? ("" + Ui.SecurityDisplayStore.devicesModel.count)
                                             : ("" + Ui.SecurityDisplayStore.devicesModel.count))
                                          : "0"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/info.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("dialog.securityCenter.trustTitle")
                            trailingText: root.utility_friendly_trust_detail()
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/clock.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("dialog.securityCenter.serverTitle")
                            trailingText: root.utility_friendly_gateway_detail()
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/device.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.t("dialog.deviceManager.currentDevice")
                            trailingText: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                                          ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                                          : (Ui.I18n.usesCjkLocale ? "本机" : "This device")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/info.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.usesCjkLocale ? "版本" : "Version"
                            trailingText: Ui.SecurityDisplayStore.versionText.length > 0
                                          ? Ui.SecurityDisplayStore.versionText
                                          : (Ui.I18n.usesCjkLocale ? "桌面版" : "Desktop")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/file.svg"
                            iconBg: Ui.Style.topBarPillBg
                            iconBorder: Ui.Style.topBarPillBorder
                            titleText: Ui.I18n.usesCjkLocale ? "剪贴板隔离" : "Clipboard isolation"
                            trailingText: Ui.SecurityDisplayStore.clipboardIsolationEnabled
                                          ? (Ui.I18n.usesCjkLocale ? "已开启" : "On")
                                          : (Ui.I18n.usesCjkLocale ? "关闭" : "Off")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        UtilityNavRow {
                            Layout.fillWidth: true
                            iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                            iconBg: Ui.Style.railAccentBg
                            iconBorder: Ui.Style.railAccentBorder
                            titleText: Ui.I18n.t("dialog.securityCenter.manageDevices")
                            trailingText: ""
                        }
                    }
                }

                Rectangle {
                    id: securityDevicesCard
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.showingSecuritySurface &&
                             Ui.SecurityDisplayStore.devicesModel.count > 0
                    radius: Ui.Style.radiusLarge
                    color: Ui.Style.panelBg
                    border.width: 1
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: devicesColumn.implicitHeight + Ui.Style.paddingL * 2

                    ColumnLayout {
                        id: devicesColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingL
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                            color: Ui.Style.textPrimary
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        Repeater {
                            model: Ui.SecurityDisplayStore.devicesModel

                            delegate: UtilityDeviceRow {
                                Layout.fillWidth: true
                                titleText: root.utility_friendly_device_name(maskedDeviceDisplayId, index, false)
                                detailText: lastSeenDisplay
                            }
                        }

                        Text {
                            visible: Ui.SecurityDisplayStore.devicesModel.count === 0
                            Layout.fillWidth: true
                            text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                            color: Ui.Style.textSecondary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                    }
                }

                Rectangle {
                    id: callsCurrentCard
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.showingCallsSurface &&
                             (Ui.CallDisplayStore.activeCallId.length > 0 ||
                              Ui.CallDisplayStore.incomingCallActive)
                    radius: Ui.Style.radiusLarge
                    color: Ui.Style.panelBg
                    border.width: 1
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: callsCurrentColumn.implicitHeight + Ui.Style.paddingL * 2

                    ColumnLayout {
                        id: callsCurrentColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingM

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingM

                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: Ui.Style.railAccentBg
                                border.width: 1
                                border.color: Ui.Style.railAccentBorder

                                Image {
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16
                                    fillMode: Image.PreserveAspectFit
                                    source: (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.incomingCallActive)
                                            ? (Ui.CallDisplayStore.activeCallVideo || Ui.CallDisplayStore.incomingCallVideo
                                               ? "qrc:/mi/e2ee/ui/icons/video.svg"
                                               : "qrc:/mi/e2ee/ui/icons/phone.svg")
                                            : "qrc:/mi/e2ee/ui/icons/phone.svg"
                                    smooth: true
                                    antialiasing: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                    text: Ui.CallDisplayStore.activeCallId.length > 0
                                          ? (Ui.CallDisplayStore.activeCallVideo
                                             ? Ui.I18n.t("chat.callActiveVideo")
                                             : Ui.I18n.t("chat.callActiveVoice"))
                                          : (Ui.CallDisplayStore.incomingCallActive
                                             ? (Ui.CallDisplayStore.incomingCallVideo
                                                ? Ui.I18n.t("chat.callIncomingVideo")
                                                : Ui.I18n.t("chat.callIncomingVoice"))
                                             : Ui.I18n.t("calls.ready"))
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.incomingCallActive)
                                          ? Ui.ChatDisplayStore.resolveTitle(Ui.CallDisplayStore.activeCallPeer.length > 0
                                                                            ? Ui.CallDisplayStore.activeCallPeer
                                                                            : Ui.CallDisplayStore.incomingCallPeer)
                                          : (Ui.ChatDisplayStore.currentChatTitle.length > 0
                                             ? Ui.ChatDisplayStore.currentChatTitle
                                             : Ui.I18n.t("calls.pickContact"))
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 12
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                visible: Ui.CallDisplayStore.activeCallId.length > 0
                                text: Ui.I18n.t("chat.callDuration").arg(Ui.UiUtil.format_call_duration(callDurationSec))
                                color: Ui.Style.textMuted
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Components.GhostButton {
                                visible: Ui.CallDisplayStore.incomingCallActive &&
                                         Ui.CallDisplayStore.activeCallId.length === 0
                                text: Ui.I18n.t("chat.callDecline")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                onClicked: Ui.CallDisplayStore.declineIncomingCall()
                            }

                            Components.PrimaryButton {
                                visible: Ui.CallDisplayStore.incomingCallActive &&
                                         Ui.CallDisplayStore.activeCallId.length === 0
                                text: Ui.I18n.t("chat.callAccept")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                onClicked: Ui.CallDisplayStore.acceptIncomingCall()
                            }

                            Components.GhostButton {
                                visible: Ui.CallDisplayStore.activeCallId.length > 0
                                text: Ui.I18n.t("chat.callHangup")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                onClicked: Ui.CallDisplayStore.endCall()
                            }

                            Components.PrimaryButton {
                                visible: !Ui.CallDisplayStore.incomingCallActive &&
                                         Ui.CallDisplayStore.activeCallId.length === 0
                                text: Ui.I18n.t("chat.call")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                                onClicked: Ui.ChatDisplayStore.handleCallAction(false)
                            }

                            Components.GhostButton {
                                visible: !Ui.CallDisplayStore.incomingCallActive &&
                                         Ui.CallDisplayStore.activeCallId.length === 0
                                text: Ui.I18n.t("chat.video")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                                onClicked: Ui.ChatDisplayStore.handleCallAction(true)
                            }
                        }
                    }
                }

                Rectangle {
                    id: callsRecentCard
                    Layout.preferredWidth: parent.utilityCardWidth
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.showingCallsSurface
                    radius: Ui.Style.radiusLarge
                    color: Ui.Style.panelBg
                    border.width: 1
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: callsRecentColumn.implicitHeight + Ui.Style.paddingL * 2

                    ColumnLayout {
                        id: callsRecentColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            visible: Ui.CallDisplayStore.activeCallId.length === 0 &&
                                     !Ui.CallDisplayStore.incomingCallActive &&
                                     root.utility_call_target_id().length > 0
                            spacing: 10

                            Components.IdentityAvatar {
                                size: 42
                                titleText: root.utility_call_target_title()
                                seedText: root.utility_call_target_avatar_seed()
                                mode: root.utility_call_target_avatar_mode()
                                presenceState: "online"
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: root.utility_call_target_title()
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: Ui.I18n.usesCjkLocale ? "快速发起通话" : "Quick call"
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 12
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }

                            Components.IconButton {
                                accessibleName: Ui.I18n.t("chat.call")
                                icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                                buttonSize: 36
                                iconSize: 16
                                onClicked: {
                                    Ui.ChatDisplayStore.setCurrentChat(root.utility_call_target_id())
                                    Ui.ChatDisplayStore.handleCallAction(false)
                                }
                            }

                            Components.IconButton {
                                accessibleName: Ui.I18n.t("chat.video")
                                icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                                buttonSize: 36
                                iconSize: 16
                                onClicked: {
                                    Ui.ChatDisplayStore.setCurrentChat(root.utility_call_target_id())
                                    Ui.ChatDisplayStore.handleCallAction(true)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            visible: Ui.CallDisplayStore.activeCallId.length === 0 &&
                                     !Ui.CallDisplayStore.incomingCallActive &&
                                     root.utility_call_target_id().length > 0
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Text {
                            Layout.fillWidth: true
                            text: Ui.I18n.t("calls.recent")
                            color: Ui.Style.textPrimary
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 52
                                Layout.preferredHeight: 26
                                radius: 13
                                color: Ui.Style.railAccentBg
                                border.width: 1
                                border.color: Ui.Style.railAccentBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: Ui.I18n.usesCjkLocale ? "全部" : "All"
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 60
                                Layout.preferredHeight: 26
                                radius: 13
                                color: Ui.Style.topBarPillBg
                                border.width: 1
                                border.color: Ui.Style.topBarPillBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: Ui.I18n.usesCjkLocale ? "未接" : "Missed"
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Repeater {
                            model: root.showingCallsSurface
                                   ? Math.min(10, Ui.ChatDisplayStore.filteredDialogsModel.count)
                                   : 0

                            delegate: Item {
                                property var dialogEntry: Ui.ChatDisplayStore.filteredDialogsModel.get(index)
                                Layout.fillWidth: true
                                implicitHeight: recentCallRow.implicitHeight

                                UtilityCallRow {
                                    id: recentCallRow
                                    anchors.fill: parent
                                    avatarTitle: dialogEntry.title || ""
                                    avatarSeed: dialogEntry.avatarKey || dialogEntry.title || ""
                                    avatarMode: dialogEntry.avatarMode || ""
                                    titleText: dialogEntry.title || ""
                                    subtitleText: (Ui.CallDisplayStore.activeCallPeer === (dialogEntry.chatId || "") ||
                                                   Ui.CallDisplayStore.incomingCallPeer === (dialogEntry.chatId || ""))
                                                  ? (Ui.CallDisplayStore.activeCallVideo || Ui.CallDisplayStore.incomingCallVideo
                                                     ? Ui.I18n.t("chat.callActiveVideo")
                                                     : Ui.I18n.t("chat.callActiveVoice"))
                                                  : Ui.I18n.t("chat.call")
                                    timeText: dialogEntry.timeText || ""
                                    onClicked: Ui.ChatDisplayStore.setCurrentChat(dialogEntry.chatId || "")
                                    onActionClicked: {
                                        Ui.ChatDisplayStore.setCurrentChat(dialogEntry.chatId || "")
                                        Ui.ChatDisplayStore.handleCallAction(false)
                                    }
                                }

                            }
                        }

                        Rectangle {
                            visible: Ui.ChatDisplayStore.filteredDialogsModel.count === 0
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.panelBgAlt
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: noRecentCallsLabel.implicitHeight + Ui.Style.paddingM * 2

                            Text {
                                id: noRecentCallsLabel
                                anchors.centerIn: parent
                                text: Ui.I18n.t("calls.noRecent")
                                color: Ui.Style.textSecondary
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Ui.Style.paddingL
                }
            }
        }
    }

}
