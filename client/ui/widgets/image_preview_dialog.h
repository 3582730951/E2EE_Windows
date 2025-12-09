#ifndef MI_E2EE_CLIENT_UI_WIDGETS_IMAGE_PREVIEW_DIALOG_H
#define MI_E2EE_CLIENT_UI_WIDGETS_IMAGE_PREVIEW_DIALOG_H

#include <QDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include "theme.h"

namespace mi::client::ui::widgets {

class ImagePreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImagePreviewDialog(const UiPalette& palette = DefaultPalette(),
                                QWidget* parent = nullptr);

    void setImage(const QPixmap& pixmap);

private:
    void applyTransform();

    UiPalette palette_;
    QGraphicsScene* scene_{nullptr};
    QGraphicsView* view_{nullptr};
    QGraphicsPixmapItem* item_{nullptr};
    int currentRotation_{0};
};

}  // namespace mi::client::ui::widgets

#endif  // MI_E2EE_CLIENT_UI_WIDGETS_IMAGE_PREVIEW_DIALOG_H
