#pragma once

#include <QString>

#include "Theme.h"

class QApplication;

namespace UiSettings {

enum class Language : int {
    Auto = 0,
    ZhCN = 1,
    EnUS = 2,
};

struct Settings {
    Language language{Language::Auto};
    Theme::Scheme scheme{Theme::Scheme::Auto};
    int fontScalePercent{100};  // 50~200
    bool trayNotifications{true};
    bool trayPreview{false};  // privacy: default off
};

void InitAppMeta();

Settings Load();
void Save(const Settings &settings);

const Settings &current();
void setCurrent(const Settings &settings);

Language resolvedLanguage();
QString Tr(const QString &zh, const QString &en);

void ApplyToApp(QApplication &app);

QString LanguageLabel(Language lang);
QString SchemeLabel(Theme::Scheme scheme);

}  // namespace UiSettings
