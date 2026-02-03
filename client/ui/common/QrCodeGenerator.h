#pragma once

#include <QImage>
#include <QString>

namespace mi::ui {

QImage BuildQrImage(const QString& text, int size, int border = 2);

}  // namespace mi::ui
