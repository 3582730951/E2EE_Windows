// Empty chat window replica.
#pragma once

#include "../common/FramelessWindowBase.h"

class ChatEmptyWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit ChatEmptyWindow(QWidget *parent = nullptr);
};
