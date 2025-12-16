#include "UiSettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QSettings>

#include <algorithm>

namespace UiSettings {

namespace {

Settings gSettings;

Language ParseLanguage(int v) {
    switch (v) {
        case static_cast<int>(Language::ZhCN):
            return Language::ZhCN;
        case static_cast<int>(Language::EnUS):
            return Language::EnUS;
        case static_cast<int>(Language::Auto):
        default:
            return Language::Auto;
    }
}

Theme::Scheme ParseScheme(int v) {
    switch (v) {
        case static_cast<int>(Theme::Scheme::Light):
            return Theme::Scheme::Light;
        case static_cast<int>(Theme::Scheme::HighContrast):
            return Theme::Scheme::HighContrast;
        case static_cast<int>(Theme::Scheme::Dark):
        default:
            return Theme::Scheme::Dark;
    }
}

int ClampInt(int value, int lo, int hi) { return std::max(lo, std::min(value, hi)); }

QString Key(const char *k) { return QString::fromLatin1(k); }

}  // namespace

void InitAppMeta() {
    if (QCoreApplication::organizationName().isEmpty()) {
        QCoreApplication::setOrganizationName(QStringLiteral("MI"));
    }
    if (QCoreApplication::applicationName().isEmpty()) {
        QCoreApplication::setApplicationName(QStringLiteral("MI_E2EE_Client_UI"));
    }
}

Settings Load() {
    InitAppMeta();
    QSettings s;
    Settings out;
    out.language = ParseLanguage(s.value(Key("ui/language"), 0).toInt());
    out.scheme = ParseScheme(s.value(Key("ui/scheme"), static_cast<int>(Theme::Scheme::Dark)).toInt());
    out.fontScalePercent = ClampInt(s.value(Key("ui/font_scale_percent"), 100).toInt(), 50, 200);
    out.trayNotifications = s.value(Key("ui/tray_notifications"), true).toBool();
    out.trayPreview = s.value(Key("ui/tray_preview"), false).toBool();
    gSettings = out;
    return out;
}

void Save(const Settings &settings) {
    InitAppMeta();
    QSettings s;
    s.setValue(Key("ui/language"), static_cast<int>(settings.language));
    s.setValue(Key("ui/scheme"), static_cast<int>(settings.scheme));
    s.setValue(Key("ui/font_scale_percent"), settings.fontScalePercent);
    s.setValue(Key("ui/tray_notifications"), settings.trayNotifications);
    s.setValue(Key("ui/tray_preview"), settings.trayPreview);
    s.sync();
}

const Settings &current() { return gSettings; }

void setCurrent(const Settings &settings) { gSettings = settings; }

Language resolvedLanguage() {
    if (gSettings.language == Language::ZhCN || gSettings.language == Language::EnUS) {
        return gSettings.language;
    }
    const QLocale loc = QLocale::system();
    if (loc.language() == QLocale::Chinese) {
        return Language::ZhCN;
    }
    return Language::EnUS;
}

QString Tr(const QString &zh, const QString &en) {
    return resolvedLanguage() == Language::EnUS ? en : zh;
}

void ApplyToApp(QApplication &app) {
    Theme::setScheme(gSettings.scheme);
    Theme::setFontScalePercent(gSettings.fontScalePercent);
    app.setFont(Theme::defaultFont(10));
    Theme::ApplyTo(app);
}

QString LanguageLabel(Language lang) {
    switch (lang) {
        case Language::ZhCN:
            return QStringLiteral("中文");
        case Language::EnUS:
            return QStringLiteral("English");
        case Language::Auto:
        default:
            return Tr(QStringLiteral("跟随系统"), QStringLiteral("Auto"));
    }
}

QString SchemeLabel(Theme::Scheme scheme) {
    switch (scheme) {
        case Theme::Scheme::Light:
            return Tr(QStringLiteral("浅色"), QStringLiteral("Light"));
        case Theme::Scheme::HighContrast:
            return Tr(QStringLiteral("高对比"), QStringLiteral("High Contrast"));
        case Theme::Scheme::Dark:
        default:
            return Tr(QStringLiteral("深色"), QStringLiteral("Dark"));
    }
}

}  // namespace UiSettings

