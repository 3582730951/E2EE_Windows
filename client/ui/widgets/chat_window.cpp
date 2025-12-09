#include "chat_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ChatWindow::ChatWindow(const UiPalette& palette, QWidget* parent)
    : QWidget(parent), palette_(palette) {
    setObjectName(QStringLiteral("Panel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    buildHeader(root);
    buildMessageArea(root);
    buildInputArea(root);

    addMessage({QStringLiteral("System"), QStringLiteral("欢迎进入安全群"), QStringLiteral("10:00"),
                false});
    addMessage({QStringLiteral("Me"), QStringLiteral("消息示例，静态展示"), QStringLiteral("10:01"),
                true});
}

void ChatWindow::buildHeader(QVBoxLayout* parentLayout) {
    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("Panel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(10);

    auto* thresholdLabel = new QLabel(tr("轮换阈值"), header);
    thresholdLabel->setStyleSheet(QStringLiteral("color:%1;").arg(palette_.textPrimary.name()));
    headerLayout->addWidget(thresholdLabel);

    threshold_ = new QComboBox(header);
    threshold_->addItems({QStringLiteral("1000"), QStringLiteral("5000"), QStringLiteral("10000")});
    threshold_->setFixedWidth(100);
    headerLayout->addWidget(threshold_);

    auto* roundBtn = new QToolButton(header);
    roundBtn->setFixedSize(24, 24);
    roundBtn->setStyleSheet(QStringLiteral("background:#000; border-radius:12px;"));
    headerLayout->addWidget(roundBtn);

    auto* clearBtn = new QPushButton(tr("清理日志"), header);
    clearBtn->setMinimumHeight(30);
    headerLayout->addWidget(clearBtn);

    auto addDot = [&](const QString& color) {
        auto* dot = new QToolButton(header);
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QStringLiteral("background:%1; border-radius:6px; border:none;")
                               .arg(color));
        headerLayout->addWidget(dot);
    };
    addDot(QStringLiteral("#444"));
    addDot(QStringLiteral("#666"));
    addDot(QStringLiteral("#888"));

    headerLayout->addStretch(1);

    parentLayout->addWidget(header);
}

void ChatWindow::buildMessageArea(QVBoxLayout* parentLayout) {
    messageContainer_ = new QWidget(this);
    messageContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(4, 4, 4, 4);
    messageLayout_->setSpacing(6);
    messageLayout_->setAlignment(Qt::AlignTop);

    messageScroll_ = new QScrollArea(this);
    messageScroll_->setWidgetResizable(true);
    messageScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageScroll_->setFrameShape(QFrame::NoFrame);
    messageScroll_->setWidget(messageContainer_);

    parentLayout->addWidget(messageScroll_, 1);
}

void ChatWindow::buildInputArea(QVBoxLayout* parentLayout) {
    auto* inputPanel = new QFrame(this);
    inputPanel->setObjectName(QStringLiteral("Panel"));
    auto* row = new QHBoxLayout(inputPanel);
    row->setContentsMargins(12, 8, 12, 8);
    row->setSpacing(10);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("输入消息（本地展示，后续仍走触发/轮换路径）"));
    row->addWidget(input_, 1);

    auto* sendButton = new QPushButton(tr("发送消息"), this);
    sendButton->setMinimumWidth(120);
    sendButton->setCursor(Qt::PointingHandCursor);
    connect(sendButton, &QPushButton::clicked, this, [this]() {
        const QString text = input_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        const QString now = QTime::currentTime().toString(QStringLiteral("hh:mm"));
        ChatMessage msg{QStringLiteral("Me"), text, now, true};
        addMessage(msg);
        emit messageSent(msg);
        input_->clear();
    });
    row->addWidget(sendButton, 0, Qt::AlignVCenter);

    parentLayout->addWidget(inputPanel);
}

void ChatWindow::addMessage(const ChatMessage& message) {
    if (!messageLayout_) {
        return;
    }
    auto* wrapper = new QWidget(messageContainer_);
    auto* row = new QHBoxLayout(wrapper);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    if (message.fromSelf) {
        row->addStretch(1);
    }

    auto* bubble = new MessageBubble(message, palette_, wrapper);
    row->addWidget(bubble, 0, message.fromSelf ? Qt::AlignRight : Qt::AlignLeft);

    if (!message.fromSelf) {
        row->addStretch(1);
    }

    messageLayout_->addWidget(wrapper);
    scrollToBottom();
}

void ChatWindow::scrollToBottom() {
    if (!messageScroll_) {
        return;
    }
    if (auto* bar = messageScroll_->verticalScrollBar()) {
        bar->setValue(bar->maximum());
    }
}

void ChatWindow::setGroupName(const QString& name) {
    Q_UNUSED(name);
}

}  // namespace mi::client::ui::widgets
