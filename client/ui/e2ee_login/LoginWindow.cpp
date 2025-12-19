#include "LoginWindow.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpacerItem>
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

QLabel *createAvatar(QWidget *parent) {
    auto *avatar = new QLabel(parent);
    avatar->setFixedSize(108, 108);
    avatar->setStyleSheet(
        QStringLiteral(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);"
            "border: 2px solid %3;"
            "border-radius: 54px;")
            .arg(Tokens::accent().lighter(118).name(),
                 Tokens::accent().darker(105).name(),
                 Tokens::border().name()));
    auto *shadow = new QGraphicsDropShadowEffect(avatar);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 120));
    avatar->setGraphicsEffect(shadow);
    return avatar;
}

QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setFixedSize(260, 44);
    btn->setCursor(Qt::PointingHandCursor);
    const QColor base = Tokens::accent();
    const QColor hover = base.lighter(112);
    const QColor pressed = base.darker(110);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  color: white;"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 10px;"
            "  font-size: 15px;"
            "}"
            "QPushButton:hover { background: %2; }"
            "QPushButton:pressed { background: %3; }")
            .arg(base.name(),
                 hover.name(),
                 pressed.name()));
    return btn;
}

QLabel *linkLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                             .arg(Tokens::accent().name()));
    return label;
}

}  // namespace

LoginWindow::LoginWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(420, 560);
    setMinimumSize(360, 480);
    frameWidget()->setStyleSheet(
        QStringLiteral(
            "#frameContainer {"
            "background: %1;"
            "border: 1px solid %2;"
            "border-radius: 10px; }")
            .arg(Tokens::panelBg().name(),
                 Tokens::border().name()));

    auto *central = new QWidget(this);
    central->setContentsMargins(0, 0, 0, 0);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(24, 18, 24, 24);
    mainLayout->setSpacing(12);

    // Title bar
    auto *titleBar = new QWidget(central);
    titleBar->setFixedHeight(36);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addStretch();
    auto *cubeBtn = new IconButton(QString(), titleBar);
    cubeBtn->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/maximize.svg"), 14);
    cubeBtn->setFixedSize(26, 26);
    cubeBtn->setColors(Tokens::textSub(), Tokens::textMain(), Tokens::textMain(),
                       QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
    auto *closeBtn = new IconButton(QString(), titleBar);
    closeBtn->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 14);
    closeBtn->setFixedSize(26, 26);
    closeBtn->setColors(Tokens::textSub(), Tokens::textMain(), Theme::uiDangerRed(),
                        QColor(0, 0, 0, 0), Tokens::hoverBg(), Tokens::selectedBg());
    connect(cubeBtn, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    titleLayout->addWidget(cubeBtn);
    titleLayout->addSpacing(4);
    titleLayout->addWidget(closeBtn);
    mainLayout->addWidget(titleBar);
    setTitleBar(titleBar);

    mainLayout->addSpacing(8);

    // Center content
    auto *title = new QLabel(QStringLiteral("E2EE"), central);
    title->setAlignment(Qt::AlignHCenter);
    title->setFont(Theme::defaultFont(34, QFont::DemiBold));
    title->setStyleSheet(QStringLiteral("color: %1;").arg(Tokens::accent().name()));

    auto *avatar = createAvatar(central);
    auto *nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignHCenter);
    auto *name = new QLabel(QStringLiteral("eds"), central);
    name->setFont(Theme::defaultFont(16, QFont::DemiBold));
    name->setStyleSheet(QStringLiteral("color: %1;").arg(Tokens::textMain().name()));
    auto *arrow = new QLabel(QStringLiteral("\u25BE"), central);
    arrow->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Tokens::textMuted().name()));
    nameLayout->addWidget(name);
    nameLayout->addSpacing(6);
    nameLayout->addWidget(arrow);

    auto *loginBtn = primaryButton(UiSettings::Tr(QStringLiteral("登录"), QStringLiteral("Login")), central);

    auto *linksLayout = new QHBoxLayout();
    linksLayout->setAlignment(Qt::AlignHCenter);
    linksLayout->setSpacing(10);
    linksLayout->addWidget(linkLabel(UiSettings::Tr(QStringLiteral("添加账号"),
                                                    QStringLiteral("Add account")),
                                     central));
    auto *divider = new QLabel(QStringLiteral("|"), central);
    divider->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Tokens::textMuted().name()));
    linksLayout->addWidget(divider);
    linksLayout->addWidget(linkLabel(UiSettings::Tr(QStringLiteral("移除账号"),
                                                    QStringLiteral("Remove account")),
                                     central));

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setAlignment(Qt::AlignHCenter);
    contentLayout->setSpacing(14);
    contentLayout->addWidget(title);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(avatar, 0, Qt::AlignHCenter);
    contentLayout->addLayout(nameLayout);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(loginBtn, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(10);
    contentLayout->addLayout(linksLayout);

    mainLayout->addLayout(contentLayout);
    mainLayout->addStretch();

    setCentralWidget(central);
    setOverlayImage(QStringLiteral(UI_REF_DIR "/ref_login.png"));
}
