#include "chat_window.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>
#include <QToolButton>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {

ChatWindow::ChatWindow(const UiPalette& palette, QWidget* parent, bool showHeader)
    : QWidget(parent), palette_(palette), showHeader_(showHeader) {
    if (parent == nullptr) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMinimumSize(360, 260);
    }
    setStyleSheet(QStringLiteral("background:transparent;"));
    setObjectName(QStringLiteral("Panel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // rounded wrapper for whole content
    auto* wrapper = new QFrame(this);
    wrapper->setObjectName(QStringLiteral("Wrapper"));
    wrapper->setStyleSheet(QStringLiteral(
        "QFrame#Wrapper { background:%1; border-radius:14px; border:none; }")
                               .arg(QStringLiteral("#101018")));
    auto* wrapLayout = new QVBoxLayout(wrapper);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->setSpacing(0);

    buildHeader(wrapLayout);
    buildMessageArea(wrapLayout);
    buildInputArea(wrapLayout);

    root->addWidget(wrapper);
}

void ChatWindow::buildHeader(QVBoxLayout* parentLayout) {
    titleBar_ = new QWidget(this);
    titleBar_->setObjectName(QStringLiteral("TitleBar"));
    titleBar_->setStyleSheet(QStringLiteral("QWidget#TitleBar { background:%1; border-radius:12px; }")
                                 .arg(QStringLiteral("#11111a")));
    auto* layout = new QHBoxLayout(titleBar_);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    titleLabel_ = new QLabel(tr(""), titleBar_);
    titleLabel_->setStyleSheet(QStringLiteral("color:%1; font-weight:700; font-size:14px;")
                                   .arg(palette_.textPrimary.name()));
    layout->addWidget(titleLabel_, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    auto makeBtn = [&](const QString& text) {
        auto* btn = new QToolButton(titleBar_);
        btn->setText(text);
        btn->setFixedSize(18, 18);
        btn->setStyleSheet(QStringLiteral(
                               "background:%1; color:%2; border:none; border-radius:9px;"
                               "font-weight:900; font-size:10px; padding:0; font-family:'Segoe UI Symbol','Microsoft YaHei';")
                               .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    btnMin_ = makeBtn(QStringLiteral("–"));
    btnMax_ = makeBtn(QStringLiteral("□"));
    btnClose_ = makeBtn(QStringLiteral("×"));
    
    layout->addWidget(btnMin_, 0, Qt::AlignRight);
    layout->addWidget(btnMax_, 0, Qt::AlignRight);
    layout->addWidget(btnClose_, 0, Qt::AlignRight);

    connect(btnMin_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            window()->showMinimized();
        }
    });
    connect(btnMax_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            if (window()->isMaximized()) {
                window()->showNormal();
            } else {
                window()->showMaximized();
            }
        }
    });
    connect(btnClose_, &QToolButton::clicked, this, [this]() {
        if (window()) {
            window()->close();
        }
    });

    titleBar_->installEventFilter(this);
    parentLayout->addWidget(titleBar_);
}

void ChatWindow::buildMessageArea(QVBoxLayout* parentLayout) {
    messageContainer_ = new QWidget(this);
    messageContainer_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(4, 2, 4, 2);
    messageLayout_->setSpacing(4);
    messageLayout_->setAlignment(Qt::AlignTop);

    messageScroll_ = new QScrollArea(this);
    messageScroll_->setWidgetResizable(true);
    messageScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    messageScroll_->setFrameShape(QFrame::NoFrame);
    messageScroll_->setWidget(messageContainer_);

    parentLayout->addWidget(messageScroll_, 1);
}

void ChatWindow::buildInputArea(QVBoxLayout* parentLayout) {
    auto* toolsRow = new QHBoxLayout();
    toolsRow->setContentsMargins(2, 0, 2, 0);
    toolsRow->setSpacing(4);
    
    auto* folderBtn = new QToolButton(this);
    folderBtn->setText(QStringLiteral("📁"));
    folderBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    folderBtn->setCursor(Qt::PointingHandCursor);
    folderBtn->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:10px; padding:6px 10px;")
                                 .arg(palette_.buttonDark.name(), palette_.textPrimary.name()));
    auto* menu = new QMenu(folderBtn);
    menu->addAction(tr("文件上传（占位）"));
    menu->addAction(tr("拉取离线文件"));
    connect(folderBtn, &QToolButton::clicked, this, [folderBtn, menu]() {
        if (!folderBtn || !menu) {
            return;
        }
        menu->exec(folderBtn->mapToGlobal(QPoint(0, folderBtn->height())));
    });
    toolsRow->addWidget(folderBtn, 0, Qt::AlignLeft);
    toolsRow->addStretch(1);
    parentLayout->addLayout(toolsRow);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("输入消息（本地展示，后续仍走触发/轮换路径）"));
    row->addWidget(input_, 1);

    auto* sendButton = new QPushButton(tr("发送消息"), this);
    sendButton->setMinimumWidth(100);
    sendButton->setMaximumWidth(150);
    sendButton->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:8px;")
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
    inputPanel->setContentsMargins(2, 2, 2, 0);
    parentLayout->addWidget(inputPanel);
}

void ChatWindow::addMessage(const ChatMessage& message) {
    if (!messageLayout_) {
        return;
    }
    auto* wrapper = new QWidget(messageContainer_);
    auto* row = new QHBoxLayout(wrapper);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

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
    const QString bg = message.fromSelf ? QStringLiteral("#1a1f28") : QStringLiteral("#161c25");
    bubble->setStyleSheet(QStringLiteral("QFrame#Bubble { background:%1; border-radius:16px; border:none; }")
                              .arg(bg));

    auto* layout = new QHBoxLayout(bubble);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto addAvatar = [&](Qt::Alignment align) {
        auto* avatar = new QLabel(bubble);
        const QString avatarText = message.sender.isEmpty() ? QStringLiteral("S") : message.sender;
        avatar->setPixmap(BuildAvatar(avatarText, QColor(QStringLiteral("#8fb7ff")), 28));
        avatar->setFixedSize(28, 28);
        avatar->setScaledContents(true);
        avatar->setStyleSheet(QStringLiteral("background:transparent;"));
        layout->addWidget(avatar, 0, align);
    };

    auto* column = new QVBoxLayout();
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(1);

    auto* textLabel =
        new QLabel(message.text.isEmpty() ? tr("示例消息") : message.text, bubble);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:13px; background:transparent;")
            .arg(palette_.textPrimary.name()));
    column->addWidget(textLabel);

    auto* timeLabel = new QLabel(message.time, bubble);
    timeLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px; background:transparent;")
            .arg(palette_.textSecondary.name()));
    column->addWidget(timeLabel, 0, Qt::AlignLeft);

    if (!message.fromSelf) {
        addAvatar(Qt::AlignTop);
        layout->addLayout(column, 1);
    } else {
        layout->addLayout(column, 1);
        addAvatar(Qt::AlignTop);
    }

    return bubble;
}

bool ChatWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == titleBar_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                dragPos_ = me->globalPosition().toPoint();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                const QPoint delta = me->globalPosition().toPoint() - dragPos_;
                dragPos_ = me->globalPosition().toPoint();
                if (window()) {
                    window()->move(window()->pos() + delta);
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
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
    if (titleLabel_) {
        titleLabel_->setText(name);
    }
}

}  // namespace mi::client::ui::widgets
