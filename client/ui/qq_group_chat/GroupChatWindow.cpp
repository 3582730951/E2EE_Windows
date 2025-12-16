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

namespace {

IconButton *titleIcon(const QString &glyphOrSvg, QWidget *parent, int svgSize = 16) {
    auto *btn = new IconButton(QString(), parent);
    const QString v = glyphOrSvg.trimmed();
    if (v.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive) || v.startsWith(QStringLiteral(":/"))) {
        btn->setSvgIcon(v, svgSize);
    } else {
        btn->setGlyph(v, 10);
    }
    btn->setFixedSize(32, 32);
    btn->setColors(QColor("#D3D3D3"), QColor("#FFFFFF"), QColor("#D8D8D8"),
                   QColor("#1F1F1F"), QColor("#2B2B2B"), QColor("#222222"));
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
    avatar->setStyleSheet("background: #4E7BFF; border-radius: 16px;");
    layout->addWidget(avatar);

    auto *nameLabel = new QLabel(name, row);
    nameLabel->setStyleSheet("color: #EAEAEA; font-size: 12px;");
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
    avatar->setStyleSheet("background: #3A7AFE; border-radius: 19px;");
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(4);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(6);
    auto *name = new QLabel(QStringLiteral("Q群管家"), box);
    name->setStyleSheet("color: #E9E9E9; font-size: 13px; font-weight: 600;");
    auto *dot = new QLabel(box);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet("background: #3A8CFF; border-radius: 4px;");
    headerLayout->addWidget(name);
    headerLayout->addWidget(dot);
    headerLayout->addStretch();

    auto *bubble = new QFrame(box);
    bubble->setStyleSheet(
        "QFrame { background: #262626; border-radius: 10px; color: #CFCFCF; }");
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(8);

    auto *mention = new QLabel(QStringLiteral("@天 涩啥"), bubble);
    mention->setStyleSheet("color: #D5D5D5; font-size: 12px;");
    auto *image = new QLabel(bubble);
    image->setFixedSize(320, 160);
    image->setStyleSheet("background: #3A3A3A; border-radius: 8px;");

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
    avatar->setStyleSheet("background: #888; border-radius: 16px;");
    layout->addWidget(avatar, 0, Qt::AlignTop);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(4);
    auto *nameRow = new QHBoxLayout();
    nameRow->setSpacing(6);
    auto *name = new QLabel(QStringLiteral("天"), box);
    name->setStyleSheet("color: #E6E6E6; font-size: 12px; font-weight: 600;");
    auto *level = new QLabel(QStringLiteral("LV1凡人"), box);
    level->setStyleSheet("color: #A0A0A0; font-size: 11px;");
    nameRow->addWidget(name);
    nameRow->addWidget(level);
    nameRow->addStretch();

    auto *bubble = new QFrame(box);
    bubble->setStyleSheet("QFrame { background: #2A2A2A; border-radius: 12px; }");
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(14, 12, 14, 12);
    bubbleLayout->setSpacing(6);

    auto *text = new QLabel(QStringLiteral("游戏逆向的半壁江山"), bubble);
    text->setWordWrap(true);
    text->setStyleSheet("color: #E6E6E6; font-size: 13px;");
    auto *footer = new QLabel(QStringLiteral("推荐群聊"), bubble);
    footer->setStyleSheet("color: #A0A0A0; font-size: 11px;");

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
        btn->setColors(QColor("#C8C8C8"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                       QColor(0, 0, 0, 0), QColor(255, 255, 255, 20),
                       QColor(255, 255, 255, 35));
        layout->addWidget(btn);
    }
    layout->addStretch();
    auto *clock = new IconButton(QString(), bar);
    clock->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/clock.svg"), 16);
    clock->setFixedSize(28, 28);
    clock->setColors(QColor("#C8C8C8"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                     QColor(0, 0, 0, 0), QColor(255, 255, 255, 20),
                     QColor(255, 255, 255, 35));
    layout->addWidget(clock);
    return bar;
}

QWidget *inputFooter(QWidget *parent, bool sendEnabled = true) {
    auto *footer = new QWidget(parent);
    auto *layout = new QHBoxLayout(footer);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    auto *placeholder = new QLabel(QStringLiteral("DDDDDDDDDDDDDDDD"), footer);
    placeholder->setStyleSheet("color: #6E6E6E; font-size: 13px;");
    layout->addWidget(placeholder, 1);

    auto makeBtn = [&](const QString &text, const QColor &fg, const QColor &border,
                       const QColor &bg, bool enabled) {
        auto *btn = new QPushButton(text, footer);
        btn->setEnabled(enabled);
        btn->setFixedHeight(32);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; "
            "border-radius: 6px; padding: 0 14px; font-size: 12px; }"
            "QPushButton:disabled { color: #7A7A7A; border-color: #3A3A3A; "
            "background: #2A2A2A; }"
            "QPushButton:hover:!disabled { background: %4; }"
            "QPushButton:pressed:!disabled { background: %5; }")
                                 .arg(fg.name())
                                 .arg(bg.name())
                                 .arg(border.name())
                                 .arg(bg.lighter(110).name())
                                 .arg(bg.darker(115).name()));
        return btn;
    };

    auto *closeBtn =
        makeBtn(QStringLiteral("关闭"), QColor("#E6E6E6"), QColor("#4A4A4A"),
                QColor("#242424"), true);
    auto *sendBtn = makeBtn(QStringLiteral("发送"), QColor("white"), QColor("#2F81E8"),
                            QColor("#2F81E8"), sendEnabled);

    layout->addWidget(closeBtn, 0);
    layout->addWidget(sendBtn, 0);
    return footer;
}

}  // namespace

GroupChatWindow::GroupChatWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(720, 800);
    setMinimumSize(720, 800);

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
    titleLabel->setStyleSheet("color: #EDEDED; font-size: 14px; font-weight: 600;");
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
    chatArea->setStyleSheet("background: #141414;");
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
    separator->setStyleSheet("background: #1E1E1E;");
    chatLayout->addWidget(separator);

    chatLayout->addWidget(toolbarRow(chatArea));
    chatLayout->addWidget(inputFooter(chatArea, true));

    // Right member list
    auto *memberPanel = new QWidget(body);
    memberPanel->setFixedWidth(220);
    memberPanel->setStyleSheet("background: #121212;");
    auto *memberLayout = new QVBoxLayout(memberPanel);
    memberLayout->setContentsMargins(10, 10, 10, 10);
    memberLayout->setSpacing(8);

    auto *memberHeader = new QWidget(memberPanel);
    memberHeader->setFixedHeight(34);
    auto *headerLayout = new QHBoxLayout(memberHeader);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(6);
    auto *memberTitle = new QLabel(QStringLiteral("群聊成员 1036"), memberHeader);
    memberTitle->setStyleSheet("color: #E6E6E6; font-size: 12px; font-weight: 600;");
    auto *searchIcon = new IconButton(QString(), memberHeader);
    searchIcon->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/search.svg"), 14);
    searchIcon->setFixedSize(24, 24);
    searchIcon->setColors(QColor("#C8C8C8"), QColor("#FFFFFF"), QColor("#E0E0E0"),
                          QColor(0, 0, 0, 0), QColor(255, 255, 255, 30),
                          QColor(255, 255, 255, 50));
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
    divider->setStyleSheet("background: #1E1E1E;");

    bodyLayout->addWidget(chatArea, 1);
    bodyLayout->addWidget(divider);
    bodyLayout->addWidget(memberPanel);

    mainLayout->addWidget(body, 1);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_group_chat.png"));
}
