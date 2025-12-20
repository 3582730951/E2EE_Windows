#include "UiStyle.h"

#include <QMenu>

#include "Theme.h"

namespace UiStyle {

namespace {

QString MenuStyleSheet() {
    return QStringLiteral(
               "QMenu { background: %1; color: %2; border: 1px solid %3; border-radius: 10px; }"
               "QMenu::item { padding: 8px 20px; border-radius: 8px; margin: 2px 6px; }"
               "QMenu::item:selected { background: %4; }"
               "QMenu::separator { height: 1px; background: %3; margin: 6px 12px; }")
        .arg(Theme::uiMenuBg().name(),
             Theme::uiTextMain().name(),
             Theme::uiBorder().name(),
             Theme::uiHoverBg().name());
}

}  // namespace

void ApplyMenuStyle(QMenu &menu) { menu.setStyleSheet(MenuStyleSheet()); }

}  // namespace UiStyle
