#include "chat_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>
#include <QToolButton>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ChatWindow::ChatWindow(const UiPalette& palette, QWidget* parent)
    : QWidget(parent), palette_(palette) {
    setObjectName(QStringLiteral("Panel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    buildHeader(root);
    buildMessageArea(root);
    buildInputArea(root);

    addMessage({QStringLiteral("S"), QStringLiteral("欢迎进入安全群"), QStringLiteral("10:00"),
                false});
    addMessage({QStringLiteral("我"), QStringLiteral("消息示例，静态展示"), QStringLiteral("10:01"),
                true});
}

void ChatWindow::buildHeader(QVBoxLayout* parentLayout) {
    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 6, 8, 6);
    headerLayout->setSpacing(10);

    auto* thresholdLabel = new QLabel(tr("轮换阈值"), header);
    thresholdLabel->setStyleSheet(QStringLiteral("color:%1;").arg(palette_.textPrimary.name()));
    headerLayout->addWidget(thresholdLabel);

    threshold_ = new QComboBox(header);
    threshold_->addItems({QStringLiteral("1000"), QStringLiteral("5000"), QStringLiteral("10000")});
    threshold_->setFixedWidth(120);
    headerLayout->addWidget(threshold_);

    auto* roundBtn = new QToolButton(header);
    roundBtn->setFixedSize(24, 24);
    roundBtn->setStyleSheet(QStringLiteral("background:%1; border-radius:12px; border:none;")
                                .arg(palette_.buttonDark.name()));
    headerLayout->addWidget(roundBtn);

    auto* clearBtn = new QPushButton(tr("清理日志"), header);
    clearBtn->setMinimumHeight(32);
    clearBtn->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:6px;")
                                .arg(palette_.accent.name(), palette_.textPrimary.name()));
    headerLayout->addWidget(clearBtn);

    auto addDot = [&](const QString& color) {
        auto* dot = new QToolButton(header);
        dot->setFixedSize(20, 20);
        dot->setStyleSheet(QStringLiteral("background:%1; border-radius:10px; border:none;")
                               .arg(color));
        headerLayout->addWidget(dot);
    };
    addDot(QStringLiteral("#2c2c36"));
    addDot(QStringLiteral("#373744"));
    addDot(QStringLiteral("#444454"));

    headerLayout->addStretch(1);

    parentLayout->addWidget(header);
}

void ChatWindow::buildMessageArea(QVBoxLayout* parentLayout) {
    messageContainer_ = new QWidget(this);
    messageContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(8, 8, 8, 8);
    messageLayout_->setSpacing(12);
    messageLayout_->setAlignment(Qt::AlignTop);

    messageScroll_ = new QScrollArea(this);
    messageScroll_->setWidgetResizable(true);
    messageScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageScroll_->setFrameShape(QFrame::NoFrame);
    messageScroll_->setWidget(messageContainer_);

    parentLayout->addWidget(messageScroll_, 1);
}

void ChatWindow::buildInputArea(QVBoxLayout* parentLayout) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("输入消息（本地展示，后续仍走触发/轮换路径）"));
    row->addWidget(input_, 1);

    auto* sendButton = new QPushButton(tr("发送消息"), this);
    sendButton->setMinimumWidth(120);
    sendButton->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:6px;")
                                  .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
    sendButton->setCursor(Qt::PointingHandCursor);
    connect(sendButton, &QPushButton::clicked, this, [this]() {
        const QString text = input_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        const QString now = QTime::currentTime().toString(QStringLiteral("hh:mm"));
        ChatMessage msg{QStringLiteral("S"), text, now, true};
        addMessage(msg);
        emit messageSent(msg);
        input_->clear();
    });
    row->addWidget(sendButton, 0, Qt::AlignVCenter);

    auto* inputPanel = new QWidget(this);
    inputPanel->setLayout(row);
    parentLayout->addWidget(inputPanel);
}

void ChatWindow::addMessage(const ChatMessage& message) {
    if (!messageLayout_) {
        return;
    }
    auto* wrapper = new QWidget(messageContainer_);
    auto* row = new QHBoxLayout(wrapper);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    if (message.fromSelf) {
        row->addStretch(1);
    }

    row->addWidget(buildBubble(message, wrapper),
                   0, message.fromSelf ? Qt::AlignRight : Qt::AlignLeft);

    if (!message.fromSelf) {
        row->addStretch(1);
    }

    messageLayout_->addWidget(wrapper);
    scrollToBottom();
}

QWidget* ChatWindow::buildBubble(const ChatMessage& message, QWidget* parent) {
    auto* bubble = new QFrame(parent);
    bubble->setObjectName(QStringLiteral("Bubble"));
    const QString bg = message.fromSelf ? QStringLiteral("#1a3a80") : QStringLiteral("#121222");
    bubble->setStyleSheet(QStringLiteral("QFrame#Bubble { background:%1; border-radius:10px; }")
                              .arg(bg));

    auto* layout = new QHBoxLayout(bubble);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    auto* avatar = new QLabel(bubble);
    const QString avatarText = message.sender.isEmpty() ? QStringLiteral("S") : message.sender;
    avatar->setPixmap(BuildAvatar(avatarText, palette_.accent, 32));
    avatar->setFixedSize(32, 32);
    avatar->setScaledContents(true);
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto* column = new QVBoxLayout();
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(2);

    auto* nameLabel = new QLabel(message.sender, bubble);
    nameLabel->setStyleSheet(
        QStringLiteral("color:%1; font-weight:600;").arg(palette_.textPrimary.name()));
    column->addWidget(nameLabel, 0, Qt::AlignLeft);

    auto* textLabel =
        new QLabel(message.text.isEmpty() ? tr("示例消息") : message.text, bubble);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:13px;").arg(palette_.textPrimary.name()));
    column->addWidget(textLabel);

    auto* timeLabel = new QLabel(message.time, bubble);
    timeLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;").arg(palette_.textSecondary.name()));
    column->addWidget(timeLabel, 0, Qt::AlignLeft);

    layout->addLayout(column, 1);

    return bubble;
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
