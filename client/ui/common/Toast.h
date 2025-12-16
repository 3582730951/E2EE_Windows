// Lightweight non-blocking toast helper for Widgets UI.
#pragma once

#include <QString>

class QWidget;

class Toast {
public:
    enum class Level {
        Info = 0,
        Success = 1,
        Warning = 2,
        Error = 3,
    };

    static void Show(QWidget *parent,
                     const QString &text,
                     Level level = Level::Info,
                     int durationMs = 2400);
};

