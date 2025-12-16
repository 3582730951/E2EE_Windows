#include "ChatEmptyWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

QWidget *inputFooter(QWidget *parent) {
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
    auto *sendBtn = makeBtn(QStringLiteral("发送"), QColor("white"), QColor("#3A3A3A"),
                            QColor("#2A2A2A"), false);

    layout->addWidget(closeBtn, 0);
    layout->addWidget(sendBtn, 0);
    return footer;
}

}  // namespace

ChatEmptyWindow::ChatEmptyWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(906, 902);
    setMinimumSize(906, 902);

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

    auto *titleLabel = new QLabel(QStringLiteral("飞子"), titleBar);
    titleLabel->setStyleSheet("color: #EDEDED; font-size: 14px; font-weight: 600;");
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
    chatArea->setStyleSheet("background: #151515;");
    auto *chatLayout = new QVBoxLayout(chatArea);
    chatLayout->setContentsMargins(12, 10, 12, 12);
    chatLayout->setSpacing(0);
    chatLayout->addStretch();  // empty chat placeholder

    auto *separator = new QWidget(chatArea);
    separator->setFixedHeight(1);
    separator->setStyleSheet("background: #1E1E1E;");
    chatLayout->addWidget(separator);
    chatLayout->addWidget(toolbarRow(chatArea));
    chatLayout->addWidget(inputFooter(chatArea));

    auto *statusBar = new QWidget(body);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("background: #0F0F0F;");
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    statusLayout->setSpacing(6);
    auto *statusText =
        new QLabel(QStringLiteral("2 个项目 | 选中 1 个项目 | 291 KB |"), statusBar);
    statusText->setStyleSheet("color: #7A7A7A; font-size: 11px;");
    statusLayout->addWidget(statusText, 0, Qt::AlignLeft | Qt::AlignVCenter);
    statusLayout->addStretch();

    bodyLayout->addWidget(chatArea, 1);
    bodyLayout->addWidget(statusBar);

    mainLayout->addWidget(body, 1);

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_chat_empty.png"));
}
