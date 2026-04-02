pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: store

    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property string sceneName: typeof uiSmokeScene !== "undefined"
                                        ? ((uiSmokeScene || "").toLowerCase())
                                        : ""
    readonly property string localeOverride: typeof uiSmokeLocale !== "undefined"
                                             ? (uiSmokeLocale || "")
                                             : ""
    readonly property string themeOverride: typeof uiSmokeTheme !== "undefined"
                                            ? ((uiSmokeTheme || "").toLowerCase())
                                            : ""
    readonly property double scaleOverride: typeof uiSmokeScale !== "undefined"
                                            ? Number(uiSmokeScale || 1.0)
                                            : 1.0
    readonly property bool validScene: sceneName === "" ||
                                       sceneName === "auth_login" ||
                                       sceneName === "login" ||
                                       sceneName === "chat_list" ||
                                       sceneName === "chat_list_light" ||
                                       sceneName === "chat_detail" ||
                                       sceneName === "settings_home" ||
                                       sceneName === "calls_home" ||
                                       sceneName === "security_center" ||
                                       sceneName === "post_login_light" ||
                                       sceneName === "post_login"
    readonly property string normalizedScene: {
        if (sceneName === "auth_login") {
            return "login"
        }
        if (sceneName === "chat_list" ||
                sceneName === "chat_list_light" ||
                sceneName === "chat_detail" ||
                sceneName === "settings_home" ||
                sceneName === "calls_home" ||
                sceneName === "login" ||
                sceneName === "security_center" ||
                sceneName === "post_login_light" ||
                sceneName === "post_login") {
            return sceneName
        }
        return validScene ? "post_login" : "invalid"
    }
    readonly property bool loginScene: normalizedScene === "login"
    readonly property bool securityCenterScene: normalizedScene === "security_center"
    readonly property bool postLoginLightScene: normalizedScene === "post_login_light" ||
                                                normalizedScene === "chat_list_light"
    readonly property bool postLoginScene: normalizedScene !== "login" &&
                                           normalizedScene !== "invalid"
    readonly property bool forceLightTheme: postLoginLightScene || themeOverride === "light"
    readonly property bool forceDarkTheme: ((postLoginScene && !postLoginLightScene) ||
                                           normalizedScene === "security_center") &&
                                           themeOverride !== "light" ||
                                           themeOverride === "dark"

    function activatePreview() {
        if (!smokeMode || !postLoginScene) {
            return false
        }
        return Ui.AppStore.enterSmokeShellPreview(sceneName || normalizedScene)
    }

    function viewportWidth(includeAuthFallback) {
        if (loginScene || includeAuthFallback === true) {
            return 840
        }
        return 900
    }

    function viewportHeight() {
        return 620
    }

    function captureName() {
        if (sceneName === "auth_login" || normalizedScene === "login") {
            return "login"
        }
        if (sceneName === "chat_list") {
            return "chat-list"
        }
        if (sceneName === "chat_detail") {
            return "chat-detail"
        }
        if (sceneName === "settings_home") {
            return "settings-home"
        }
        if (sceneName === "calls_home") {
            return "calls-home"
        }
        if (normalizedScene === "security_center") {
            return "security-center"
        }
        if (sceneName === "chat_list_light") {
            return "chat-list-light"
        }
        if (normalizedScene === "post_login_light") {
            return "post-login-light"
        }
        if (normalizedScene === "invalid") {
            return "invalid-scene"
        }
        return "post-login"
    }
}
