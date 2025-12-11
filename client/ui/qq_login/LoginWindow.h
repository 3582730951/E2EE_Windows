// QQ login window replica.
#pragma once

#include "../common/FramelessWindowBase.h"

class LoginWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
};
