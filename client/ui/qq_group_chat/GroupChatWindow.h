// Group chat window replica.
#pragma once

#include "../common/FramelessWindowBase.h"

class GroupChatWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit GroupChatWindow(QWidget *parent = nullptr);
};
