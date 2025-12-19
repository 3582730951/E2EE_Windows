#include "GroupChatWindow.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "../common/IconButton.h"
#include "../common/Theme.h"
#include "../common/UiSettings.h"

namespace {

struct Tokens {
    static QColor windowBg() { return Theme::uiWindowBg(); }
    static QColor panelBg() { return Theme::uiPanelBg(); }
    static QColor sidebarBg() { return Theme::uiSidebarBg(); }
    static QColor border() { return Theme::uiBorder(); }
    static QColor textMain() { return Theme::uiTextMain(); }
    static QColor textSub() { return Theme::uiTextSub(); }
    static QColor textMuted() { return Theme::uiTextMuted(); }
    static QColor hoverBg() { return Theme::uiHoverBg(); }
    static QColor selectedBg() { return Theme::uiSelectedBg(); }
    static QColor bubbleIn() { return Theme::uiMessageIncomingBg(); }
    static QColor bubbleText() { return Theme::uiMessageText(); }
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

QLabel *memberTag(const QString &text, const QColor &color, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "color: white; background: %1; padding: 2px 6px; border-radius: 8px; "
        "font-size: 11px;")
                             .arg(color.name()));
    return label;
}

QFrame *memberRow(const QString &name, const QString &role, const QColor &roleColor,
                  QWidget *parent) {
    auto *row = new QFrame(parent);
    row->setFixedHeight(46);
    row->setStyleSheet("QFrame { background: transparent; }");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 6, 12, 6);
    layout->setSpacing(10);

    auto *avatar = new QLabel(row);
    avatar->setFixedSize(32, 32);
    avatar->setStyleSheet(QStringLiteral("background: %1; border-radius: 16px;")
                              .arg(Tokens::accent().name()));
    layout->addWidget(avatar);

    auto *nameLabel = new QLabel(name, row);
    nameLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                 .arg(Tokens::textMain().name()));
    layout->addWidget(nameLabel, 1);

    if (!role.isEmpty()) {
        layout->addWidget(memberTag(role, roleColor, row));
    }

    return row;
}

QFrame *robotMessage(QWidget *parent) {
    auto *box = new QFrame(parent);
    box->setStyleSheet("QFrame { background: transparent; }");
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto *avatar = new QLabel(box);
    avatar->setFixedSize(38, 38);
    avatar->setStyleSheet(QStringLiteral("background: %1; border-radius: 19px;")
                              .arg(Tokens::accent().name()));
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(4);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(6);
    auto *name = new QLabel(QStringLiteral("Q群管家"), box);
    name->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
                            .arg(Tokens::textMain().name()));
    auto *dot = new QLabel(box);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;")
                           .arg(Tokens::accent().name()));
    headerLayout->addWidget(name);
    headerLayout->addWidget(dot);
    headerLayout->addStretch();

    auto *bubble = new QFrame(box);
    bubble->setStyleSheet(
        QStringLiteral("QFrame { background: %1; border-radius: 10px; color: %2; }")
            .arg(Tokens::bubbleIn().name(),
                 Tokens::bubbleText().name()));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(8);

    auto *mention = new QLabel(QStringLiteral("@天 涩啥"), bubble);
    mention->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                               .arg(Tokens::textSub().name()));
    auto *image = new QLabel(bubble);
    image->setFixedSize(320, 160);
    image->setStyleSheet(QStringLiteral("background: %1; border-radius: 8px;")
                             .arg(Tokens::hoverBg().name()));

    bubbleLayout->addWidget(mention);
    bubbleLayout->addWidget(image);

    contentLayout->addLayout(headerLayout);
    contentLayout->addWidget(bubble);

    layout->addLayout(contentLayout);
    layout->addStretch();
    return box;
}

QFrame *textMessage(QWidget *parent) {
    auto *box = new QFrame(parent);
    box->setStyleSheet("QFrame { background: transparent; }");
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(10);

    auto *avatar = new QLabel(box);
    avatar->setFixedSize(32, 32);
    avatar->setStyleSheet(QStringLiteral("background: %1; border-radius: 16px;")
                              .arg(Tokens::textMuted().name()));
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(4);
    auto *nameRow = new QHBoxLayout();
    nameRow->setSpacing(6);
    auto *name = new QLabel(QStringLiteral("天"), box);
    name->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
                            .arg(Tokens::textMain().name()));
    auto *level = new QLabel(QStringLiteral("LV1凡人"), box);
    level->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                             .arg(Tokens::textMuted().name()));
    nameRow->addWidget(name);
    nameRow->addWidget(level);
    nameRow->addStretch();

    auto *bubble = new QFrame(box);
    bubble->setStyleSheet(QStringLiteral("QFrame { background: %1; border-radius: 12px; }")
                              .arg(Tokens::bubbleIn().name()));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(14, 12, 14, 12);
    bubbleLayout->setSpacing(6);

    auto *text = new QLabel(QStringLiteral("游戏逆向的半壁江山"), bubble);
    text->setWordWrap(true);
    text->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
                            .arg(Tokens::bubbleText().name()));
    auto *footer = new QLabel(QStringLiteral("推荐群聊"), bubble);
    footer->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                              .arg(Tokens::textMuted().name()));

    bubbleLayout->addWidget(text);
    bubbleLayout->addWidget(footer, 0, Qt::AlignLeft);

    contentLayout->addLayout(nameRow);
    contentLayout->addWidget(bubble);

    layout->addLayout(contentLayout);
    layout->addStretch();
    return box;
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
        QStringLiteral(":/mi/e2ee/ui/icons/bell.svg"),
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
            "QPushButton:hover:enabled { background: %4; }"
            "QPushButton:pressed:enabled { background: %5; }")
            .arg(Tokens::textMain().name(),
                 Tokens::panelBg().name(),
                 Tokens::border().name(),
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

QWidget *inputFooter(QWidget *parent, bool sendEnabled = true) {
    auto *footer = new QWidget(parent);
    auto *layout = new QHBoxLayout(footer);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    auto *placeholder = new QLabel(UiSettings::Tr(QStringLiteral("输入消息…"),
                                                  QStringLiteral("Type a message…")),
                                   footer);
    placeholder->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
                                   .arg(Tokens::textMuted().name()));
    layout->addWidget(placeholder, 1);

    auto *closeBtn = outlineButton(UiSettings::Tr(QStringLiteral("关闭"),
                                                  QStringLiteral("Close")),
                                   footer);
    auto *sendBtn = primaryButton(UiSettings::Tr(QStringLiteral("发送"),
                                                 QStringLiteral("Send")),
                                  footer,
                                  sendEnabled);

    layout->addWidget(closeBtn, 0);
    layout->addWidget(sendBtn, 0);
    return footer;
}

}  // namespace

GroupChatWindow::GroupChatWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(720, 800);
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

    auto *titleLabel = new QLabel(QStringLiteral("逆向思维导图 (1036)"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;")
                                  .arg(Tokens::textMain().name()));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    QStringList funcIcons = {
        QStringLiteral(":/mi/e2ee/ui/icons/phone.svg"),
        QStringLiteral(":/mi/e2ee/ui/icons/video.svg"),
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

    // Body split
    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left chat area
    auto *chatArea = new QWidget(body);
    chatArea->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::windowBg().name()));
    auto *chatLayout = new QVBoxLayout(chatArea);
    chatLayout->setContentsMargins(12, 10, 12, 12);
    chatLayout->setSpacing(10);

    auto *messages = new QWidget(chatArea);
    auto *msgLayout = new QVBoxLayout(messages);
    msgLayout->setContentsMargins(0, 0, 0, 0);
    msgLayout->setSpacing(10);
    msgLayout->addWidget(robotMessage(messages));
    msgLayout->addWidget(textMessage(messages));
    msgLayout->addStretch();
    messages->setLayout(msgLayout);

    auto *msgScroll = new QScrollArea(chatArea);
    msgScroll->setWidgetResizable(true);
    msgScroll->setFrameShape(QFrame::NoFrame);
    msgScroll->setStyleSheet("QScrollArea { background: transparent; }");
    msgScroll->setWidget(messages);
    chatLayout->addWidget(msgScroll, 1);

    auto *separator = new QWidget(chatArea);
    separator->setFixedHeight(1);
    separator->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::border().name()));
    chatLayout->addWidget(separator);

    chatLayout->addWidget(toolbarRow(chatArea));
    chatLayout->addWidget(inputFooter(chatArea, true));

    // Right member list
    auto *memberPanel = new QWidget(body);
    memberPanel->setFixedWidth(220);
    memberPanel->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::sidebarBg().name()));
    auto *memberLayout = new QVBoxLayout(memberPanel);
    memberLayout->setContentsMargins(10, 10, 10, 10);
    memberLayout->setSpacing(8);

    auto *memberHeader = new QWidget(memberPanel);
    memberHeader->setFixedHeight(34);
    auto *headerLayout = new QHBoxLayout(memberHeader);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(6);
    auto *memberTitle = new QLabel(QStringLiteral("群聊成员 1036"), memberHeader);
    memberTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
                                   .arg(Tokens::textMain().name()));
    auto *searchIcon = new IconButton(QString(), memberHeader);
    searchIcon->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"), 14);
    searchIcon->setFixedSize(24, 24);
    searchIcon->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(),
                          QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
    headerLayout->addWidget(memberTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(searchIcon);
    memberLayout->addWidget(memberHeader);

    auto *memberScroll = new QScrollArea(memberPanel);
    memberScroll->setWidgetResizable(true);
    memberScroll->setFrameShape(QFrame::NoFrame);
    memberScroll->setStyleSheet("QScrollArea { background: transparent; }");
    auto *memberContent = new QWidget(memberScroll);
    auto *memberListLayout = new QVBoxLayout(memberContent);
    memberListLayout->setContentsMargins(0, 0, 0, 0);
    memberListLayout->setSpacing(4);
    memberListLayout->addWidget(memberRow(QStringLiteral("Q群管家"), QStringLiteral("群主"),
                                          Theme::accentOrange(), memberContent));
    memberListLayout->addWidget(memberRow(QStringLiteral("天"), QStringLiteral("管理员"),
                                          Theme::accentBlue(), memberContent));
    memberListLayout->addWidget(memberRow(QStringLiteral("逆向学习"), QString(), QColor(), memberContent));
    memberListLayout->addWidget(memberRow(QStringLiteral("逆向新人"), QString(), QColor(), memberContent));
    memberListLayout->addStretch();
    memberContent->setLayout(memberListLayout);
    memberScroll->setWidget(memberContent);

    memberLayout->addWidget(memberScroll);

    // Divider between chat and member list
    auto *divider = new QWidget(body);
    divider->setFixedWidth(1);
    divider->setStyleSheet(QStringLiteral("background: %1;").arg(Tokens::border().name()));

    bodyLayout->addWidget(chatArea, 1);
    bodyLayout->addWidget(divider);
    bodyLayout->addWidget(memberPanel);

    mainLayout->addWidget(body, 1);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_group_chat.png"));
}
