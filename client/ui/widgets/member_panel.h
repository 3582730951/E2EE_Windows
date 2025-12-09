#ifndef MI_E2EE_CLIENT_UI_WIDGETS_MEMBER_PANEL_H
#define MI_E2EE_CLIENT_UI_WIDGETS_MEMBER_PANEL_H

#include <QListWidget>
#include <QWidget>

#include "theme.h"

namespace mi::client::ui::widgets {

class MemberPanel : public QWidget {
    Q_OBJECT

public:
    explicit MemberPanel(const UiPalette& palette = DefaultPalette(),
                         QWidget* parent = nullptr);

    void addMember(const QString& name, bool isAdmin);

private:
    UiPalette palette_;
    QListWidget* list_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_MEMBER_PANEL_H
