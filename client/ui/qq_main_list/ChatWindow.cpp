#include "ChatWindow.h"

#include <QHBoxLayout>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDateTime>

#include "../common/IconButton.h"
#include "../common/Theme.h"
#include "BackendAdapter.h"
#include "MessageDelegate.h"
#include "MessageModel.h"

namespace {

struct ChatTokens {
    static QColor windowBg() { return QColor("#14161A"); }
    static QColor panelBg() { return QColor("#191C20"); }
    static QColor hoverBg() { return QColor("#20242A"); }
    static QColor selectedBg() { return QColor("#262B32"); }
    static QColor border() { return QColor("#1E2025"); }
    static QColor textMain() { return QColor("#F0F2F5"); }
    static QColor textSub() { return QColor("#A9ADB3"); }
    static QColor textMuted() { return QColor("#7C8087"); }
    static QColor accentBlue() { return QColor("#2F81E8"); }
    static QColor accentGrey() { return QColor("#2A2D33"); }
    static int radius() { return 10; }
};

IconButton *titleIcon(const QString &glyph, QWidget *parent) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(28, 28);
    btn->setColors(QColor("#D6D9DF"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                   QColor(0, 0, 0, 0), QColor(255, 255, 255, 18), QColor(255, 255, 255, 32));
    return btn;
}

IconButton *toolIcon(const QString &glyph, QWidget *parent) {
    auto *btn = new IconButton(glyph, parent);
    btn->setFixedSize(28, 28);
    btn->setColors(QColor("#C8C8C8"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                   QColor(0, 0, 0, 0), QColor(255, 255, 255, 20), QColor(255, 255, 255, 35));
    return btn;
}

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedSize(78, 30);
    btn->setStyleSheet(
        "QPushButton { color: #E6E6E6; background: #242424; border: 1px solid #4A4A4A; "
        "border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background: #2B2B2B; }"
        "QPushButton:pressed { background: #222222; }");
    return btn;
}

QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedHeight(30);
    btn->setStyleSheet(
        "QPushButton { color: white; background: #2F81E8; border: 1px solid #2F81E8; "
        "border-radius: 6px; padding: 0 14px; font-size: 12px; }"
        "QPushButton:hover { background: #3A8DFA; }"
        "QPushButton:pressed { background: #2A74D0; }");
    return btn;
}

}  // namespace

ChatWindow::ChatWindow(BackendAdapter *backend, QWidget *parent)
    : FramelessWindowBase(parent), backend_(backend) {
    resize(906, 902);
    setMinimumSize(640, 540);
    buildUi();
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
}

void ChatWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(Theme::kTitleBarHeight);
    titleBar->setStyleSheet(QStringLiteral("background: %1;").arg(ChatTokens::windowBg().name()));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(14, 10, 14, 10);
    titleLayout->setSpacing(10);

    titleLabel_ = new QLabel(QStringLiteral("会话"), titleBar);
    titleLabel_->setStyleSheet("color: #EDEDED; font-size: 14px; font-weight: 600;");
    titleLayout->addWidget(titleLabel_);
    titleLayout->addStretch();

    QStringList funcs = {QStringLiteral("\u260E"), QStringLiteral("\u25B6"), QStringLiteral("\u2B1A"),
                         QStringLiteral("\u2702"), QStringLiteral("\u25A3"), QStringLiteral("+"),
                         QStringLiteral("\u22EE")};
    for (const auto &g : funcs) {
        titleLayout->addWidget(titleIcon(g, titleBar));
    }

    auto *downBtn = titleIcon(QStringLiteral("\u25BE"), titleBar);
    auto *minBtn = titleIcon(QStringLiteral("\u2212"), titleBar);
    auto *closeBtn = titleIcon(QStringLiteral("\u2715"), titleBar);
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(downBtn);
    titleLayout->addWidget(minBtn);
    titleLayout->addWidget(closeBtn);

    root->addWidget(titleBar);
    setTitleBar(titleBar);

    // Message area
    auto *body = new QWidget(central);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *messageArea = new QWidget(body);
    messageArea->setStyleSheet(QStringLiteral("background: %1;").arg(ChatTokens::windowBg().name()));
    auto *msgLayout = new QVBoxLayout(messageArea);
    msgLayout->setContentsMargins(4, 6, 4, 0);
    msgLayout->setSpacing(0);
    messageModel_ = new MessageModel(this);
    messageView_ = new QListView(messageArea);
    messageView_->setFrameShape(QFrame::NoFrame);
    messageView_->setItemDelegate(new MessageDelegate(messageView_));
    messageView_->setModel(messageModel_);
    messageView_->setSelectionMode(QAbstractItemView::NoSelection);
    messageView_->setFocusPolicy(Qt::NoFocus);
    messageView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    messageView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageView_->setStyleSheet(
        "QListView { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #2A2D33; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }");
    msgLayout->addWidget(messageView_);
    bodyLayout->addWidget(messageArea, 1);

    // Divider
    auto *divider = new QWidget(body);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QStringLiteral("background: %1;").arg(ChatTokens::border().name()));
    bodyLayout->addWidget(divider);

    // Composer
    auto *composer = new QWidget(body);
    composer->setStyleSheet(QStringLiteral("background: %1;").arg(ChatTokens::panelBg().name()));
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(10, 6, 10, 8);
    composerLayout->setSpacing(6);

    auto *toolsRow = new QHBoxLayout();
    toolsRow->setSpacing(8);
    QStringList tools = {QStringLiteral(":-)"), QStringLiteral("✂"), QStringLiteral("F"),
                         QStringLiteral("P"), QStringLiteral("T"), QStringLiteral("✉"),
                         QStringLiteral("M")};
    for (const auto &g : tools) {
        toolsRow->addWidget(toolIcon(g, composer));
    }
    toolsRow->addStretch();
    auto *clock = toolIcon(QStringLiteral("\u23F0"), composer);
    toolsRow->addWidget(clock);
    composerLayout->addLayout(toolsRow);

    inputEdit_ = new QPlainTextEdit(composer);
    inputEdit_->setPlaceholderText(QStringLiteral("输入消息..."));
    inputEdit_->setStyleSheet(
        "QPlainTextEdit { background: #181B1F; border: 1px solid #1F2025; border-radius: 8px; "
        "color: #E6E6E6; padding: 8px; font-size: 13px; }"
        "QPlainTextEdit:focus { border-color: #2F81E8; }");
    inputEdit_->installEventFilter(this);
    composerLayout->addWidget(inputEdit_);

    auto *sendRow = new QHBoxLayout();
    sendRow->setSpacing(8);
    auto *placeholder = new QLabel(QStringLiteral(""), composer);
    placeholder->setMinimumWidth(120);
    sendRow->addWidget(placeholder, 1);

    auto *closeBtnAction = outlineButton(QStringLiteral("关闭"), composer);
    connect(closeBtnAction, &QPushButton::clicked, this, &QWidget::close);
    auto *sendBtn = primaryButton(QStringLiteral("发送"), composer);
    connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    auto *sendMore = new IconButton(QStringLiteral("\u25BE"), composer);
    sendMore->setFixedSize(26, 30);
    sendMore->setColors(QColor("#E6E6E6"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                        ChatTokens::accentBlue(), ChatTokens::accentBlue().lighter(110),
                        ChatTokens::accentBlue().darker(115));
    sendMenu_ = new QMenu(sendMore);
    sendMenu_->setStyleSheet(
        "QMenu { background: #1B1E22; color: #E6E6E6; border: 1px solid #2A2D33; }"
        "QMenu::item { padding: 6px 18px; }"
        "QMenu::item:selected { background: #2A2D33; }");
    QAction *sendFileAction = sendMenu_->addAction(QStringLiteral("发送文件"));
    connect(sendFileAction, &QAction::triggered, this, &ChatWindow::sendFilePlaceholder);
    connect(sendMore, &QToolButton::clicked, this, [this]() {
        if (sendMenu_) {
            sendMenu_->exec(QCursor::pos());
        }
    });
    sendRow->addWidget(closeBtnAction, 0, Qt::AlignRight);
    sendRow->addWidget(sendBtn, 0, Qt::AlignRight);
    sendRow->addWidget(sendMore, 0, Qt::AlignRight);
    composerLayout->addLayout(sendRow);

    bodyLayout->addWidget(composer);
    root->addWidget(body);

    setCentralWidget(central);
}

void ChatWindow::setConversation(const QString &id, const QString &title) {
    conversationId_ = id;
    titleLabel_->setText(title);
    updateOverlayForTitle(title);
    messageModel_->setConversation(id);
}

void ChatWindow::appendIncomingMessage(const QString &text, const QDateTime &time) {
    messageModel_->appendTextMessage(conversationId_, false, text, time);
    messageView_->scrollToBottom();
}

bool ChatWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == inputEdit_ && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers().testFlag(Qt::ShiftModifier)) {
                // New line with Shift+Enter
                return false;
            }
            sendMessage();
            return true;
        }
    }
    return FramelessWindowBase::eventFilter(obj, event);
}

void ChatWindow::sendMessage() {
    const QString text = inputEdit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }
    messageModel_->appendTextMessage(conversationId_, true, text, QDateTime::currentDateTime());
    messageView_->scrollToBottom();
    inputEdit_->clear();

    if (backend_) {
        QString err;
        if (!backend_->sendText(conversationId_, text, err)) {
            messageModel_->appendSystemMessage(conversationId_,
                                               QStringLiteral("发送失败：%1").arg(err),
                                               QDateTime::currentDateTime());
            messageView_->scrollToBottom();
        }
    }
}

void ChatWindow::appendMessage(const QString &text) {
    messageModel_->appendTextMessage(conversationId_, true, text, QDateTime::currentDateTime());
    messageView_->scrollToBottom();
}

void ChatWindow::sendFilePlaceholder() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择要发送的文件"));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    QString err;
    if (backend_ && backend_->sendFile(conversationId_, path, err)) {
        const QString note = QStringLiteral("已发送文件：%1").arg(fi.fileName());
        messageModel_->appendSystemMessage(conversationId_, note, QDateTime::currentDateTime());
    } else {
        const QString note =
            QStringLiteral("发送文件失败：%1").arg(err.isEmpty() ? QStringLiteral("未连接后端") : err);
        messageModel_->appendSystemMessage(conversationId_, note, QDateTime::currentDateTime());
    }
    messageView_->scrollToBottom();
}

void ChatWindow::updateOverlayForTitle(const QString &title) {
    if (title.contains(QStringLiteral("群"))) {
        setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_group_chat.png"));
    } else {
        setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
    }
}
