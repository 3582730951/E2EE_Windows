#include "member_panel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace mi::client::ui::widgets {
namespace {

class MemberRow : public QWidget {
public:
    MemberRow(const QString& name, bool isAdmin, const UiPalette& palette, QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(6, 6, 6, 6);
        row->setSpacing(10);

        auto* avatar = new QLabel(this);
        avatar->setPixmap(BuildAvatar(name, isAdmin ? palette.accent : palette.panelMuted, 32));
        avatar->setFixedSize(32, 32);
        avatar->setScaledContents(true);
        row->addWidget(avatar, 0, Qt::AlignVCenter);

        auto* nameLabel = new QLabel(name, this);
        nameLabel->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                                     .arg(palette.textPrimary.name()));
        row->addWidget(nameLabel, 1, Qt::AlignVCenter);

        if (isAdmin) {
            auto* admin = new QLabel(QObject::tr("管理员"), this);
            admin->setStyleSheet(QStringLiteral(
                "color:%1; background:%2; border-radius:8px; padding:4px 8px; font-size:11px;")
                                     .arg(palette.textPrimary.name(), palette.accent.name()));
            row->addWidget(admin, 0, Qt::AlignVCenter);
        }
    }
};

}  // namespace

MemberPanel::MemberPanel(const UiPalette& palette, QWidget* parent)
    : QWidget(parent), palette_(palette) {
    setObjectName(QStringLiteral("Panel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* atAll = new QPushButton(tr("@全体成员"), this);
    atAll->setCursor(Qt::PointingHandCursor);
    atAll->setMinimumHeight(32);
    layout->addWidget(atAll, 0, Qt::AlignTop);

    list_ = new QListWidget(this);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setSpacing(6);
    layout->addWidget(list_, 1);

    addMember(QStringLiteral("Alice"), true);
    addMember(QStringLiteral("Bob"), false);
    addMember(QStringLiteral("Charlie"), false);
    addMember(QStringLiteral("Dana"), true);
}

void MemberPanel::addMember(const QString& name, bool isAdmin) {
    if (!list_) {
        return;
    }
    auto* item = new QListWidgetItem(list_);
    auto* widget = new MemberRow(name, isAdmin, palette_, list_);
    item->setSizeHint(widget->sizeHint());
    list_->addItem(item);
    list_->setItemWidget(item, widget);
}

}  // namespace mi::client::ui::widgets
