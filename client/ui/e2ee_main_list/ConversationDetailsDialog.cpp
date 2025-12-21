#include "ConversationDetailsDialog.h"

#include <algorithm>

#include <QButtonGroup>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "../common/Theme.h"
#include "../common/SecureClipboard.h"
#include "../common/Toast.h"
#include "../common/UiSettings.h"
#include "BackendAdapter.h"

namespace {

QPushButton *outlineButton(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 8px; "
            "padding: 0 14px; font-size: 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %5; }"
            "QPushButton:disabled { color: %6; background: %7; }")
            .arg(Theme::uiTextMain().name(),
                 Theme::uiPanelBg().name(),
                 Theme::uiBorder().name(),
                 Theme::uiHoverBg().name(),
                 Theme::uiSelectedBg().name(),
                 Theme::uiTextMuted().name(),
                 Theme::uiPanelBg().darker(105).name()));
    return btn;
}

QFrame *segmentedFrame(QWidget *parent) {
    auto *seg = new QFrame(parent);
    seg->setFrameShape(QFrame::NoFrame);
    seg->setObjectName(QStringLiteral("seg"));
    seg->setStyleSheet(QStringLiteral(
        "QFrame#seg { background: %1; border: 1px solid %2; border-radius: 12px; }"
        "QToolButton { border: none; background: transparent; padding: 6px 14px; color: %3; font-size: 12px; }"
        "QToolButton:checked { background: %4; color: %5; border-radius: 10px; }")
                           .arg(Theme::uiSearchBg().name(),
                                Theme::uiBorder().name(),
                                Theme::uiTextSub().name(),
                                Theme::uiSelectedBg().name(),
                                Theme::uiTextMain().name()));
    return seg;
}

QLabel *fieldLabel(const QString &text, QWidget *parent) {
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 650;")
                         .arg(Theme::uiTextMain().name()));
    l->setTextFormat(Qt::PlainText);
    return l;
}

QLabel *valuePill(const QString &text, QWidget *parent) {
    auto *l = new QLabel(text, parent);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setStyleSheet(
        QStringLiteral("color: %1; background: %2; border: 1px solid %3; border-radius: 8px; "
                       "padding: 7px 10px; font-size: 12px;")
            .arg(Theme::uiTextMain().name(),
                 Theme::uiInputBg().name(),
                 Theme::uiInputBorder().name()));
    l->setTextFormat(Qt::PlainText);
    return l;
}

QString FormatFileSize(qint64 bytes) {
    if (bytes <= 0) {
        return QStringLiteral("0 B");
    }
    static const char *units[] = {"B", "KB", "MB", "GB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= 1024.0 && unit < 3) {
        v /= 1024.0;
        ++unit;
    }
    const int prec = (unit == 0) ? 0 : (v < 10.0 ? 1 : 0);
    return QStringLiteral("%1 %2").arg(v, 0, 'f', prec).arg(QString::fromLatin1(units[unit]));
}

}  // namespace

ConversationDetailsDialog::ConversationDetailsDialog(BackendAdapter *backend,
                                                     const QString &conversationId,
                                                     const QString &title,
                                                     bool isGroup,
                                                     QWidget *parent)
    : QDialog(parent),
      backend_(backend),
      conversationId_(conversationId.trimmed()),
      title_(title.trimmed()),
      isGroup_(isGroup) {
    buildUi();
}

void ConversationDetailsDialog::setStartPage(StartPage page) {
    if (!stack_ || !infoBtn_ || !filesBtn_) {
        return;
    }
    if (page == StartPage::Files) {
        filesBtn_->setChecked(true);
        stack_->setCurrentIndex(1);
        ensureFilesLoaded();
        return;
    }
    infoBtn_->setChecked(true);
    stack_->setCurrentIndex(0);
    ensureMembersLoaded();
}

void ConversationDetailsDialog::buildUi() {
    setWindowTitle(UiSettings::Tr(QStringLiteral("会话详情"), QStringLiteral("Chat details")));
    setModal(true);
    resize(640, 560);
    setStyleSheet(QStringLiteral("QDialog { background: %1; }").arg(Theme::uiWindowBg().name()));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    auto *titleLabel = new QLabel(
        title_.isEmpty() ? UiSettings::Tr(QStringLiteral("会话详情"), QStringLiteral("Chat details")) : title_, this);
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: 650;")
                                  .arg(Theme::uiTextMain().name()));
    titleLabel->setTextFormat(Qt::PlainText);
    header->addWidget(titleLabel, 1);
    header->addStretch();
    auto *copyBtn = outlineButton(UiSettings::Tr(QStringLiteral("复制 ID"), QStringLiteral("Copy ID")), this);
    copyBtn->setFixedHeight(30);
    connect(copyBtn, &QAbstractButton::clicked, this, [this]() {
        if (conversationId_.isEmpty()) {
            return;
        }
        SecureClipboard::SetText(conversationId_);
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("已复制"), QStringLiteral("Copied")),
                    Toast::Level::Info);
    });
    header->addWidget(copyBtn);
    root->addLayout(header);

    auto *seg = segmentedFrame(this);
    auto *segLayout = new QHBoxLayout(seg);
    segLayout->setContentsMargins(6, 6, 6, 6);
    segLayout->setSpacing(6);

    infoBtn_ = new QToolButton(seg);
    infoBtn_->setText(UiSettings::Tr(QStringLiteral("详情"), QStringLiteral("Info")));
    infoBtn_->setCheckable(true);
    filesBtn_ = new QToolButton(seg);
    filesBtn_->setText(UiSettings::Tr(QStringLiteral("共享文件"), QStringLiteral("Shared files")));
    filesBtn_->setCheckable(true);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(infoBtn_, 0);
    group->addButton(filesBtn_, 1);
    infoBtn_->setChecked(true);

    segLayout->addWidget(infoBtn_);
    segLayout->addWidget(filesBtn_);
    segLayout->addStretch();
    root->addWidget(seg);

    stack_ = new QStackedWidget(this);
    stack_->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
    root->addWidget(stack_, 1);

    // Info page
    auto *infoPage = new QWidget(stack_);
    auto *infoLayout = new QVBoxLayout(infoPage);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(12);

    auto *idRow = new QVBoxLayout();
    idRow->setSpacing(8);
    idRow->addWidget(fieldLabel(UiSettings::Tr(QStringLiteral("会话 ID"), QStringLiteral("Conversation ID")), infoPage));
    idValue_ = valuePill(conversationId_.isEmpty() ? QStringLiteral("-") : conversationId_, infoPage);
    idRow->addWidget(idValue_);
    infoLayout->addLayout(idRow);

    auto *typeRow = new QVBoxLayout();
    typeRow->setSpacing(8);
    typeRow->addWidget(fieldLabel(UiSettings::Tr(QStringLiteral("类型"), QStringLiteral("Type")), infoPage));
    const QString typeText =
        isGroup_
            ? UiSettings::Tr(QStringLiteral("群聊"), QStringLiteral("Group"))
            : UiSettings::Tr(QStringLiteral("私聊"), QStringLiteral("Direct message"));
    typeValue_ = valuePill(typeText, infoPage);
    typeRow->addWidget(typeValue_);
    infoLayout->addLayout(typeRow);

    membersHint_ = new QLabel(infoPage);
    membersHint_->setTextFormat(Qt::PlainText);
    membersHint_->setWordWrap(true);
    membersHint_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                    .arg(Theme::uiTextMuted().name()));
    membersHint_->setVisible(isGroup_);
    infoLayout->addWidget(membersHint_);

    membersList_ = new QListWidget(infoPage);
    membersList_->setVisible(isGroup_);
    membersList_->setSelectionMode(QAbstractItemView::NoSelection);
    membersList_->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 12px; padding: 6px; color: %3; }"
        "QListWidget::item { padding: 8px 10px; border-radius: 10px; }"
        "QListWidget::item:hover { background: %4; }")
                                     .arg(Theme::uiPanelBg().name(),
                                          Theme::uiBorder().name(),
                                          Theme::uiTextMain().name(),
                                          Theme::uiHoverBg().name()));
    infoLayout->addWidget(membersList_, 1);

    refreshMembersBtn_ = outlineButton(UiSettings::Tr(QStringLiteral("刷新成员"), QStringLiteral("Refresh members")), infoPage);
    refreshMembersBtn_->setVisible(isGroup_);
    connect(refreshMembersBtn_, &QAbstractButton::clicked, this, [this]() { reloadMembers(); });
    infoLayout->addWidget(refreshMembersBtn_, 0, Qt::AlignRight);

    infoLayout->addStretch();
    stack_->addWidget(infoPage);

    // Files page
    auto *filesPage = new QWidget(stack_);
    auto *filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(10);

    auto *filesTop = new QHBoxLayout();
    filesTop->setSpacing(10);
    filesHint_ = new QLabel(filesPage);
    filesHint_->setTextFormat(Qt::PlainText);
    filesHint_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                  .arg(Theme::uiTextMuted().name()));
    filesTop->addWidget(filesHint_, 1);
    refreshFilesBtn_ = outlineButton(UiSettings::Tr(QStringLiteral("刷新"), QStringLiteral("Refresh")), filesPage);
    saveFileBtn_ = outlineButton(UiSettings::Tr(QStringLiteral("保存所选"), QStringLiteral("Save selected")), filesPage);
    connect(refreshFilesBtn_, &QAbstractButton::clicked, this, [this]() { reloadFiles(); });
    connect(saveFileBtn_, &QAbstractButton::clicked, this, [this]() { saveSelectedFile(); });
    filesTop->addWidget(refreshFilesBtn_);
    filesTop->addWidget(saveFileBtn_);
    filesLayout->addLayout(filesTop);

    filesTable_ = new QTableWidget(filesPage);
    filesTable_->setColumnCount(4);
    filesTable_->setHorizontalHeaderLabels({
        UiSettings::Tr(QStringLiteral("文件"), QStringLiteral("File")),
        UiSettings::Tr(QStringLiteral("大小"), QStringLiteral("Size")),
        UiSettings::Tr(QStringLiteral("发送者"), QStringLiteral("Sender")),
        UiSettings::Tr(QStringLiteral("时间"), QStringLiteral("Time")),
    });
    filesTable_->horizontalHeader()->setStretchLastSection(true);
    filesTable_->verticalHeader()->setVisible(false);
    filesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    filesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    filesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    filesTable_->setShowGrid(false);
    filesTable_->setAlternatingRowColors(false);
    filesTable_->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; border: 1px solid %2; border-radius: 12px; color: %3; }"
        "QHeaderView::section { background: %4; color: %3; border: none; padding: 8px 10px; font-weight: 650; }"
        "QTableWidget::item { padding: 8px 10px; }"
        "QTableWidget::item:selected { background: %5; }")
                                   .arg(Theme::uiPanelBg().name(),
                                        Theme::uiBorder().name(),
                                        Theme::uiTextMain().name(),
                                        Theme::uiSearchBg().name(),
                                        Theme::uiSelectedBg().name()));
    filesLayout->addWidget(filesTable_, 1);
    connect(filesTable_, &QTableWidget::itemDoubleClicked, this, [this]() { saveSelectedFile(); });

    stack_->addWidget(filesPage);

    connect(group, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        if (!stack_) {
            return;
        }
        stack_->setCurrentIndex(id);
        if (id == 0) {
            ensureMembersLoaded();
        } else {
            ensureFilesLoaded();
        }
    });

    ensureMembersLoaded();
}

void ConversationDetailsDialog::ensureMembersLoaded() {
    if (!isGroup_ || membersLoaded_) {
        return;
    }
    reloadMembers();
}

void ConversationDetailsDialog::ensureFilesLoaded() {
    if (filesLoaded_) {
        return;
    }
    reloadFiles();
}

void ConversationDetailsDialog::reloadMembers() {
    membersLoaded_ = true;
    if (!isGroup_) {
        return;
    }
    if (!backend_ || conversationId_.isEmpty()) {
        if (membersHint_) {
            membersHint_->setText(UiSettings::Tr(QStringLiteral("未连接后端"), QStringLiteral("Backend is offline")));
        }
        return;
    }

    QString err;
    const auto list = backend_->listGroupMembers(conversationId_, err);
    if (membersList_) {
        membersList_->clear();
        for (const auto &u : list) {
            const QString name = u.trimmed();
            if (!name.isEmpty()) {
                membersList_->addItem(name);
            }
        }
    }

    if (membersHint_) {
        if (list.isEmpty()) {
            membersHint_->setText(err.trimmed().isEmpty()
                                      ? UiSettings::Tr(QStringLiteral("暂无成员信息"), QStringLiteral("No members info"))
                                      : err.trimmed());
        } else {
            membersHint_->setText(UiSettings::Tr(QStringLiteral("成员（%1）").arg(list.size()),
                                                 QStringLiteral("Members (%1)").arg(list.size())));
        }
    }
}

void ConversationDetailsDialog::reloadFiles() {
    filesLoaded_ = true;
    files_.clear();
    if (!backend_ || conversationId_.isEmpty()) {
        if (filesHint_) {
            filesHint_->setText(UiSettings::Tr(QStringLiteral("未连接后端"), QStringLiteral("Backend is offline")));
        }
        if (filesTable_) {
            filesTable_->setRowCount(0);
        }
        return;
    }

    QVector<BackendAdapter::HistoryMessageEntry> entries;
    QString err;
    if (!backend_->loadChatHistory(conversationId_, isGroup_, 240, entries, err)) {
        if (filesHint_) {
            filesHint_->setText(err.trimmed().isEmpty()
                                    ? UiSettings::Tr(QStringLiteral("加载失败"), QStringLiteral("Load failed"))
                                    : err.trimmed());
        }
        if (filesTable_) {
            filesTable_->setRowCount(0);
        }
        return;
    }

    for (const auto &e : entries) {
        if (e.kind != 2) {
            continue;
        }
        FileRow row;
        row.messageId = e.messageId.trimmed();
        row.name = !e.fileName.trimmed().isEmpty() ? e.fileName.trimmed() : e.text.trimmed();
        row.sender = e.outgoing
                         ? UiSettings::Tr(QStringLiteral("我"), QStringLiteral("Me"))
                         : (e.sender.trimmed().isEmpty() ? QStringLiteral("-") : e.sender.trimmed());
        row.size = e.fileSize;
        row.timestampSec = static_cast<qint64>(e.timestampSec);
        row.outgoing = e.outgoing;
        if (!row.name.isEmpty()) {
            files_.push_back(row);
        }
    }

    std::sort(files_.begin(), files_.end(), [](const FileRow &a, const FileRow &b) {
        return a.timestampSec > b.timestampSec;
    });

    if (filesTable_) {
        filesTable_->setRowCount(files_.size());
        for (int i = 0; i < files_.size(); ++i) {
            const auto &f = files_[i];
            auto *nameItem = new QTableWidgetItem(f.name);
            nameItem->setData(Qt::UserRole + 1, f.messageId);
            nameItem->setData(Qt::UserRole + 2, f.outgoing);
            auto *sizeItem = new QTableWidgetItem(FormatFileSize(f.size));
            auto *senderItem = new QTableWidgetItem(f.sender);
            const QDateTime ts = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(f.timestampSec));
            auto *timeItem = new QTableWidgetItem(ts.isValid() ? ts.toString(QStringLiteral("MM-dd HH:mm")) : QString());
            filesTable_->setItem(i, 0, nameItem);
            filesTable_->setItem(i, 1, sizeItem);
            filesTable_->setItem(i, 2, senderItem);
            filesTable_->setItem(i, 3, timeItem);
            filesTable_->setRowHeight(i, 42);
        }
        filesTable_->resizeColumnsToContents();
    }

    if (filesHint_) {
        filesHint_->setText(files_.isEmpty()
                                ? UiSettings::Tr(QStringLiteral("暂无共享文件"), QStringLiteral("No shared files"))
                                : UiSettings::Tr(QStringLiteral("共享文件（%1）").arg(files_.size()),
                                                 QStringLiteral("Shared files (%1)").arg(files_.size())));
    }
}

void ConversationDetailsDialog::saveSelectedFile() {
    if (!backend_ || conversationId_.isEmpty() || !filesTable_) {
        return;
    }
    const QModelIndexList rows = filesTable_->selectionModel()
                                     ? filesTable_->selectionModel()->selectedRows()
                                     : QModelIndexList{};
    if (rows.isEmpty()) {
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("请选择一条文件消息"), QStringLiteral("Select a file item")),
                    Toast::Level::Info);
        return;
    }
    const int row = rows.first().row();
    QTableWidgetItem *nameItem = filesTable_->item(row, 0);
    if (!nameItem) {
        return;
    }
    const QString messageId = nameItem->data(Qt::UserRole + 1).toString().trimmed();
    const bool outgoing = nameItem->data(Qt::UserRole + 2).toBool();
    const QString name = nameItem->text().trimmed();
    if (messageId.isEmpty()) {
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("缺少 messageId，无法保存"), QStringLiteral("Missing messageId")),
                    Toast::Level::Warning);
        return;
    }
    if (outgoing) {
        Toast::Show(this,
                    UiSettings::Tr(QStringLiteral("仅支持保存接收的文件"), QStringLiteral("Only received files can be saved")),
                    Toast::Level::Info);
        return;
    }

    const QString outPath = QFileDialog::getSaveFileName(
        this,
        UiSettings::Tr(QStringLiteral("保存文件"), QStringLiteral("Save file")),
        name.isEmpty() ? QStringLiteral("file") : name);
    if (outPath.isEmpty()) {
        return;
    }

    QString err;
    if (!backend_->saveReceivedFile(conversationId_, messageId, outPath, err)) {
        Toast::Show(this,
                    err.isEmpty()
                        ? UiSettings::Tr(QStringLiteral("保存失败"), QStringLiteral("Save failed"))
                        : UiSettings::Tr(QStringLiteral("保存失败：%1").arg(err),
                                         QStringLiteral("Save failed: %1").arg(err)),
                    Toast::Level::Error);
        return;
    }
    Toast::Show(this,
                UiSettings::Tr(QStringLiteral("开始保存…"), QStringLiteral("Saving…")),
                Toast::Level::Info);
}
