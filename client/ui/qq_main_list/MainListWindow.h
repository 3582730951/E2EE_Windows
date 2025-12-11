// QQ main session list window replica.
#pragma once

#include <QHash>
#include <QPointer>
#include <QHash>

#include <QListView>
#include <QLineEdit>
#include <QStandardItemModel>

#include "../common/FramelessWindowBase.h"

class ChatWindow;
class BackendAdapter;

class MainListWindow : public FramelessWindowBase {
    Q_OBJECT

public:
    explicit MainListWindow(BackendAdapter *backend, QWidget *parent = nullptr);

private slots:
    void openChatForIndex(const QModelIndex &index);
    void handleAddFriend();
    void handleSearchTextChanged(const QString &text);
    void handleOfflineMessage(const QString &convId, const QString &text, bool isFile);

private:
    QListView *listView_{nullptr};
    QStandardItemModel *model_{nullptr};
    QHash<QString, QPointer<ChatWindow>> chatWindows_;
    QLineEdit *searchEdit_{nullptr};
    BackendAdapter *backend_{nullptr};
};
