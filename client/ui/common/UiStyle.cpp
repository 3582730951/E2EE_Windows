#include "UiStyle.h"

#include <QMenu>

#include "Theme.h"

namespace UiStyle {

namespace {

QString MenuStyleSheet() {
    return QStringLiteral(
               "QMenu { background: %1; color: %2; border: 1px solid %3; }"
               "QMenu::item { padding: 6px 18px; }"
               "QMenu::item:selected { background: %4; }"
               "QMenu::separator { height: 1px; background: %3; margin: 4px 10px; }")
        .arg(Theme::uiMenuBg().name(),
             Theme::uiTextMain().name(),
             Theme::uiBorder().name(),
             Theme::uiHoverBg().name());
}

}  // namespace

void ApplyMenuStyle(QMenu &menu) { menu.setStyleSheet(MenuStyleSheet()); }

}  // namespace UiStyle

