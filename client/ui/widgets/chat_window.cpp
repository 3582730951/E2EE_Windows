#include "chat_window.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>
#include <functional>

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

    previewDialog_ = new ImagePreviewDialog(palette_, this);

    addMessage({QStringLiteral("Alice"), QStringLiteral("@Bob 欢迎加入群组"), QStringLiteral("09:30"),
                false});
    addMessage({QStringLiteral("Me"), QStringLiteral("已收到，继续测试\n换行渲染"), QStringLiteral("09:31"),
                true});
    addMessage({QStringLiteral("Charlie"), QStringLiteral("表情占位 -> [图片/表情]"), QStringLiteral("09:32"),
                false});
}

void ChatWindow::buildHeader(QVBoxLayout* parentLayout) {
    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("Panel"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(12);

    titleLabel_ = new QLabel(tr("安全群聊"), header);
    titleLabel_->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700; color:%1;")
                                   .arg(palette_.textPrimary.name()));
    headerLayout->addWidget(titleLabel_, 1);

    auto makeIconButton = [this](const QString& tooltip, QStyle::StandardPixmap icon) {
        auto* btn = new QToolButton(this);
        btn->setIcon(style()->standardIcon(icon));
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setAutoRaise(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border-radius:10px; padding:8px; }"
            "QToolButton:hover { background:%2; }")
                               .arg(palette_.panelMuted.name(), palette_.accent.name()));
        return btn;
    };

    headerLayout->addWidget(makeIconButton(tr("电话"), QStyle::SP_DialogYesButton));
    headerLayout->addWidget(makeIconButton(tr("视频"), QStyle::SP_DesktopIcon));
    headerLayout->addWidget(makeIconButton(tr("更多"), QStyle::SP_BrowserReload));

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
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(10);

    auto* toolRow = new QHBoxLayout();
    toolRow->setSpacing(6);

    auto makeTool = [this](const QString& text, QStyle::StandardPixmap icon,
                           const std::function<void()>& onClick) {
        auto* btn = new QToolButton(this);
        btn->setText(text);
        btn->setIcon(style()->standardIcon(icon));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setAutoRaise(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border-radius:8px; padding:6px; }"
            "QToolButton:hover { background:%2; }")
                               .arg(palette_.panelMuted.name(), palette_.accent.name()));
        connect(btn, &QToolButton::clicked, this, onClick);
        return btn;
    };

    toolRow->addWidget(makeTool(tr("表情"), QStyle::SP_DirIcon, []() {}));
    toolRow->addWidget(makeTool(tr("图片"), QStyle::SP_FileIcon, [this]() { openPreviewDialog(); }));
    toolRow->addWidget(makeTool(tr("文件"), QStyle::SP_DriveHDIcon, []() {}));
    row->addLayout(toolRow);

    input_ = new QTextEdit(this);
    input_->setPlaceholderText(tr("输入消息，@用户名 高亮展示"));
    input_->setMinimumHeight(68);
    row->addWidget(input_, 1);

    auto* sendButton = new QPushButton(tr("发送"), this);
    sendButton->setMinimumWidth(80);
    sendButton->setCursor(Qt::PointingHandCursor);
    connect(sendButton, &QPushButton::clicked, this, [this]() {
        const QString text = input_->toPlainText().trimmed();
        if (text.isEmpty()) {
            return;
        }
        const QString now = QTime::currentTime().toString(QStringLiteral("hh:mm"));
        ChatMessage msg{QStringLiteral("Me"), text, now, true};
        addMessage(msg);
        emit messageSent(msg);
        input_->clear();

        QTimer::singleShot(380, this, [this, now]() {
            addMessage({QStringLiteral("Peer"), QStringLiteral("@Me 已收到 (自动回声)"), now, false});
        });
    });
    row->addWidget(sendButton, 0, Qt::AlignBottom);

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
    QTimer::singleShot(0, this, [this]() {
        if (!messageScroll_) {
            return;
        }
        if (auto* bar = messageScroll_->verticalScrollBar()) {
            bar->setValue(bar->maximum());
        }
    });
}

void ChatWindow::openPreviewDialog() {
    if (!previewDialog_) {
        return;
    }
    QPixmap preview(420, 260);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    QLinearGradient grad(0, 0, preview.width(), preview.height());
    grad.setColorAt(0.0, palette_.accent);
    grad.setColorAt(1.0, palette_.panel);
    painter.fillRect(preview.rect(), grad);
    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 12, QFont::Bold));
    painter.drawText(preview.rect(), Qt::AlignCenter, tr("图片预览占位"));

    previewDialog_->setImage(preview);
    previewDialog_->exec();
}

void ChatWindow::setGroupName(const QString& name) {
    if (titleLabel_) {
        titleLabel_->setText(name);
    }
}

}  // namespace mi::client::ui::widgets
