#include "ChatEmptyWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "../common/IconButton.h"
#include "../common/Theme.h"
#include "../common/UiSettings.h"

namespace {

struct Tokens {
    static QColor windowBg() { return Theme::uiWindowBg(); }
    static QColor panelBg() { return Theme::uiPanelBg(); }
    static QColor border() { return Theme::uiBorder(); }
    static QColor textMain() { return Theme::uiTextMain(); }
    static QColor textSub() { return Theme::uiTextSub(); }
    static QColor textMuted() { return Theme::uiTextMuted(); }
    static QColor hoverBg() { return Theme::uiHoverBg(); }
    static QColor selectedBg() { return Theme::uiSelectedBg(); }
    static QColor accent() { return Theme::uiAccentBlue(); }
};

IconButton *titleIcon(const QString &glyphOrSvg, QWidget *parent, int svgSize = 16) {
    auto *btn = new IconButton(QString(), parent);
    const QString v = glyphOrSvg.trimmed();
    if (v.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive) || v.startsWith(QStringLiteral(":/"))) {
        btn->setSvgIcon(v, svgSize);
    } else {
        btn->setGlyph(v, 10);
    }
    btn->setFixedSize(32, 32);
    btn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(),
                   QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
    return btn;
}

QWidget *toolbarRow(QWidget *parent) {
    auto *bar = new QWidget(parent);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(10);
    QStringList icons = {
        QStringLiteral(":/mi/e2ee/ui/icons/emoji.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/image.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/file.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/image.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/chat.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/send.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/mic.svg"),
    };
    for (const auto &path : icons) {
        auto *btn = new IconButton(QString(), bar);
        btn->setSvgIcon(path, 16);
        btn->setFixedSize(28, 28);
        btn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(),
                       QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
        layout->addWidget(btn);
    }
    layout->addStretch();
    auto *clock = new IconButton(QString(), bar);
    clock->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/clock.svg"), 16);
    clock->setFixedSize(28, 28);
    clock->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(),
                     QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
    layout->addWidget(clock);
    return bar;
}

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedHeight(32);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; "
            "border-radius: 8px; padding: 0 14px; font-size: 12px; }"
            "QPushButton:disabled { color: %4; background: %5; border-color: %3; }"
            "QPushButton:hover:enabled { background: %6; }"
            "QPushButton:pressed:enabled { background: %7; }")
            .arg(Tokens::textMain().name(),
                 Tokens::panelBg().name(),
                 Tokens::border().name(),
                 Tokens::textMuted().name(),
                 Tokens::hoverBg().name(),
                 Tokens::hoverBg().name(),
                 Tokens::selectedBg().name()));
    return btn;
}

QPushButton *primaryButton(const QString &text, QWidget *parent, bool enabled) {
    auto *btn = new QPushButton(text, parent);
    btn->setEnabled(enabled);
    btn->setFixedHeight(32);
    const QColor base = Tokens::accent();
    const QColor hover = base.lighter(112);
    const QColor pressed = base.darker(110);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: white; background: %1; border: 1px solid %1; "
            "border-radius: 8px; padding: 0 14px; font-size: 12px; }"
            "QPushButton:disabled { background: %2; border-color: %2; color: %3; }"
            "QPushButton:hover:enabled { background: %4; }"
            "QPushButton:pressed:enabled { background: %5; }")
            .arg(base.name(),
                 Tokens::hoverBg().name(),
                 Tokens::textMuted().name(),
                 hover.name(),
                 pressed.name()));
    return btn;
}

QWidget *inputFooter(QWidget *parent) {
    auto *footer = new QWidget(parent);
    auto *layout = new QHBoxLayout(footer);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    auto *placeholder = new QLabel(UiSettings::Tr(QStringLiteral("输入消息…"),
                                                  QStringLiteral("Type a message…")),
                                   footer);
    placeholder->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px;").arg(Tokens::textMuted().name()));
    layout->addWidget(placeholder, 1);

    auto *closeBtn = outlineButton(UiSettings::Tr(QStringLiteral("关闭"), QStringLiteral("Close")),
                                   footer);
    auto *sendBtn = primaryButton(UiSettings::Tr(QStringLiteral("发送"), QStringLiteral("Send")),
                                  footer, false);

    layout->addWidget(closeBtn, 0);
    layout->addWidget(sendBtn, 0);
    return footer;
}

}  // namespace

ChatEmptyWindow::ChatEmptyWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(906, 902);
    setMinimumSize(640, 540);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(Theme::kTitleBarHeight);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(14, 10, 14, 10);
    titleLayout->setSpacing(10);

    auto *titleLabel = new QLabel(UiSettings::Tr(QStringLiteral("会话"),
                                                 QStringLiteral("Chat")),
                                  titleBar);
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;")
                                  .arg(Tokens::textMain().name()));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    QStringList funcIcons = {
        QStringLiteral(":/mi/e2ee/ui/icons/phone.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/video.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/image.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/image.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/search.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/plus.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/more.svg"),
    };
    for (const auto &iconPath : funcIcons) {
        titleLayout->addWidget(titleIcon(iconPath, titleBar));
    }

    auto *downBtn = titleIcon(QStringLiteral(":/mi/e2ee/ui/icons/chevron-down.svg"), titleBar, 14);
    auto *minBtn = titleIcon(QStringLiteral(":/mi/e2ee/ui/icons/minimize.svg"), titleBar, 14);
    auto *maxBtn = titleIcon(QStringLiteral(":/mi/e2ee/ui/icons/maximize.svg"), titleBar, 14);
    auto *closeBtn = titleIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), titleBar, 14);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });

    titleLayout->addWidget(downBtn);
    titleLayout->addWidget(minBtn);
    titleLayout->addWidget(maxBtn);
    titleLayout->addWidget(closeBtn);

    mainLayout->addWidget(titleBar);
    setTitleBar(titleBar);

    auto *body = new QWidget(central);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *chatArea = new QWidget(body);
    chatArea->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::windowBg().name()));
    auto *chatLayout = new QVBoxLayout(chatArea);
    chatLayout->setContentsMargins(12, 10, 12, 12);
    chatLayout->setSpacing(0);
    chatLayout->addStretch();  // empty chat placeholder

    auto *separator = new QWidget(chatArea);
    separator->setFixedHeight(1);
    separator->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::border().name()));
    chatLayout->addWidget(separator);
    chatLayout->addWidget(toolbarRow(chatArea));
    chatLayout->addWidget(inputFooter(chatArea));

    auto *statusBar = new QWidget(body);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::panelBg().name()));
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    statusLayout->setSpacing(6);
    auto *statusText =
        new QLabel(QStringLiteral("2 个项目 | 选中 1 个项目 | 291 KB |"), statusBar);
    statusText->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                  .arg(Tokens::textMuted().name()));
    statusLayout->addWidget(statusText, 0, Qt::AlignLeft | Qt::AlignVCenter);
    statusLayout->addStretch();

    bodyLayout->addWidget(chatArea, 1);
    bodyLayout->addWidget(statusBar);

    mainLayout->addWidget(body, 1);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
}
