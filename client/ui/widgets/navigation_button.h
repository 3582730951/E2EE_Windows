#ifndef MI_E2EE_CLIENT_UI_WIDGETS_NAVIGATION_BUTTON_H
#define MI_E2EE_CLIENT_UI_WIDGETS_NAVIGATION_BUTTON_H

#include <QLabel>
#include <QToolButton>

namespace mi::client::ui::widgets {

class NavigationButton : public QToolButton {
    Q_OBJECT

public:
    explicit NavigationButton(const QString& text, const QIcon& icon,
                              QWidget* parent = nullptr);
    void setUnreadCount(int count);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* badge_{nullptr};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_NAVIGATION_BUTTON_H
