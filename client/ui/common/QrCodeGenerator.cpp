#include "QrCodeGenerator.h"

#include <algorithm>

#include <QPainter>

#include "qrcodegen.hpp"

namespace mi::ui {

QImage BuildQrImage(const QString& text, int size, int border) {
  if (size <= 0) {
    return {};
  }
  const std::string payload = text.toUtf8().toStdString();
  if (payload.empty()) {
    return {};
  }
  const qrcodegen::QrCode qr =
      qrcodegen::QrCode::encodeText(payload.c_str(),
                                    qrcodegen::QrCode::Ecc::MEDIUM);
  const int qr_size = qr.getSize();
  const int safe_border = std::max(0, border);
  const int cells = qr_size + safe_border * 2;
  const int scale = std::max(1, size / cells);
  const int img_size = cells * scale;

  QImage img(img_size, img_size, QImage::Format_ARGB32);
  img.fill(Qt::white);
  QPainter painter(&img);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  for (int y = 0; y < qr_size; ++y) {
    for (int x = 0; x < qr_size; ++x) {
      if (qr.getModule(x, y)) {
        const int px = (x + safe_border) * scale;
        const int py = (y + safe_border) * scale;
        painter.drawRect(px, py, scale, scale);
      }
    }
  }
  painter.end();

  if (img_size != size) {
    return img.scaled(size, size, Qt::IgnoreAspectRatio,
                      Qt::FastTransformation);
  }
  return img;
}

}  // namespace mi::ui
