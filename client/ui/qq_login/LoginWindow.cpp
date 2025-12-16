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

namespace {

QLabel *createAvatar(QWidget *parent) {
    auto *avatar = new QLabel(parent);
    avatar->setFixedSize(108, 108);
    avatar->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #4A89FF, "
        "stop:1 #7BB1FF);"
        "border: 4px solid rgba(255,255,255,0.9);"
        "border-radius: 54px;");
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
    btn->setStyleSheet(
        "QPushButton {"
        "  color: white;"
        "  background: #2D8DFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-size: 15px;"
        "}"
        "QPushButton:hover { background: #3D9DFF; }"
        "QPushButton:pressed { background: #1C7CE6; }");
    return btn;
}

QLabel *linkLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet("color: #3D9DFF; font-size: 11px;");
    return label;
}

}  // namespace

LoginWindow::LoginWindow(QWidget *parent) : FramelessWindowBase(parent) {
    resize(569, 647);
    setMinimumSize(569, 647);
    frameWidget()->setStyleSheet(
        "#frameContainer {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1A1B3A, "
        "stop:1 #16244C);"
        "border-radius: 10px; }");

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
    cubeBtn->setColors(QColor("#A0B3E8"), QColor("#C2D4FF"), QColor("#88A0D8"),
                       QColor(0, 0, 0, 0), QColor(255, 255, 255, 30),
                       QColor(255, 255, 255, 60));
    auto *closeBtn = new IconButton(QString(), titleBar);
    closeBtn->setSvgIcon(QStringLiteral(":/mi/e2ee/ui/icons/close.svg"), 14);
    closeBtn->setFixedSize(26, 26);
    closeBtn->setColors(QColor("#C4C8D2"), QColor("#FFFFFF"), QColor("#FF6666"),
                        QColor(0, 0, 0, 0), QColor(255, 255, 255, 20),
                        QColor(255, 255, 255, 30));
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
    auto *title = new QLabel(QStringLiteral("QQ"), central);
    title->setAlignment(Qt::AlignHCenter);
    title->setStyleSheet(
        "color: #6FC1FF; font-size: 34px; font-weight: 600; "
        "text-shadow: 0px 0px 12px rgba(111,193,255,0.65);");
    auto *titleGlow = new QGraphicsDropShadowEffect(title);
    titleGlow->setBlurRadius(24);
    titleGlow->setOffset(0, 0);
    titleGlow->setColor(QColor(111, 193, 255, 180));
    title->setGraphicsEffect(titleGlow);

    auto *avatar = createAvatar(central);
    auto *nameLayout = new QHBoxLayout();
    nameLayout->setAlignment(Qt::AlignHCenter);
    auto *name = new QLabel(QStringLiteral("eds"), central);
    name->setStyleSheet("color: white; font-size: 16px; font-weight: 600;");
    auto *arrow = new QLabel(QStringLiteral("\u25BE"), central);
    arrow->setStyleSheet("color: #9BB8E0; font-size: 12px;");
    nameLayout->addWidget(name);
    nameLayout->addSpacing(6);
    nameLayout->addWidget(arrow);

    auto *loginBtn = primaryButton(QStringLiteral("登录"), central);

    auto *linksLayout = new QHBoxLayout();
    linksLayout->setAlignment(Qt::AlignHCenter);
    linksLayout->setSpacing(10);
    linksLayout->addWidget(linkLabel(QStringLiteral("添加账号"), central));
    auto *divider = new QLabel(QStringLiteral("|"), central);
    divider->setStyleSheet("color: #4D78B3; font-size: 11px;");
    linksLayout->addWidget(divider);
    linksLayout->addWidget(linkLabel(QStringLiteral("移除账号"), central));

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
