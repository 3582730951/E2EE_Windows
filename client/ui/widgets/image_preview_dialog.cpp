#include "image_preview_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ImagePreviewDialog::ImagePreviewDialog(const UiPalette& palette, QWidget* parent)
    : QDialog(parent), palette_(palette) {
    setWindowTitle(tr("图片预览"));
    resize(720, 520);
    setModal(true);
    setStyleSheet(QStringLiteral("QDialog { background:%1; }").arg(palette_.background.name()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    scene_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(scene_, this);
    view_->setFrameShape(QFrame::NoFrame);
    view_->setStyleSheet(QStringLiteral(
        "QGraphicsView { background:%1; border-radius:6px; border:none; }")
                             .arg(palette_.panel.name()));
    view_->setAlignment(Qt::AlignCenter);
    layout->addWidget(view_, 1);

    item_ = new QGraphicsPixmapItem();
    scene_->addItem(item_);

    auto* controls = new QHBoxLayout();
    controls->setSpacing(10);

    auto* rotate = new QPushButton(tr("向左旋转"), this);
    rotate->setMinimumWidth(120);
    connect(rotate, &QPushButton::clicked, this, [this]() {
        currentRotation_ -= 90;
        applyTransform();
    });
    controls->addWidget(rotate, 0, Qt::AlignLeft);
    controls->addStretch(1);

    layout->addLayout(controls);
}

void ImagePreviewDialog::setImage(const QPixmap& pixmap) {
    if (!item_ || pixmap.isNull()) {
        return;
    }
    item_->setPixmap(pixmap);
    const QRectF rect = pixmap.rect();
    item_->setOffset(-rect.width() / 2.0, -rect.height() / 2.0);
    item_->setTransformOriginPoint(rect.center());
    item_->setPos(0, 0);
    scene_->setSceneRect(rect.adjusted(-50, -50, 50, 50));
    currentRotation_ = 0;
    applyTransform();
}

void ImagePreviewDialog::applyTransform() {
    if (!item_) {
        return;
    }
    QTransform transform;
    transform.rotate(static_cast<double>(currentRotation_));
    item_->setTransform(transform);
}

}  // namespace mi::client::ui::widgets
