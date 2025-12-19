// Notification center dialog for friend requests / group invites.
#pragma once

#include <QDialog>
#include <QtGlobal>
#include <QString>
#include <QVector>

class QStackedWidget;
class QToolButton;
class QScrollArea;
class QVBoxLayout;

class NotificationCenterDialog : public QDialog {
    Q_OBJECT

public:
    struct FriendRequest {
        QString requester;
        QString remark;
        qint64 receivedMs{0};
    };

    struct GroupInvite {
        QString groupId;
        QString fromUser;
        QString messageId;
        qint64 receivedMs{0};
    };

    enum class FriendRequestAction {
        Accept = 0,
        Reject = 1,
        Block = 2,
    };

    enum class GroupInviteAction {
        Join = 0,
        CopyId = 1,
        Ignore = 2,
    };

    explicit NotificationCenterDialog(QWidget *parent = nullptr);

    void setFriendRequests(const QVector<FriendRequest> &requests);
    void setGroupInvites(const QVector<GroupInvite> &invites);

    void removeFriendRequest(const QString &requester);
    void removeGroupInvite(const QString &groupId, const QString &messageId);

signals:
    void friendRequestActionRequested(const QString &requester, FriendRequestAction action);
    void groupInviteActionRequested(const QString &groupId,
                                    const QString &fromUser,
                                    const QString &messageId,
                                    GroupInviteAction action);
    void refreshRequested();

private:
    void rebuildFriendRequests();
    void rebuildGroupInvites();
    void updateSegmentTitles();

    QVector<FriendRequest> friendRequests_;
    QVector<GroupInvite> groupInvites_;

    QToolButton *requestsBtn_{nullptr};
    QToolButton *invitesBtn_{nullptr};
    QStackedWidget *stack_{nullptr};

    QScrollArea *requestsScroll_{nullptr};
    QWidget *requestsBody_{nullptr};
    QVBoxLayout *requestsLayout_{nullptr};

    QScrollArea *invitesScroll_{nullptr};
    QWidget *invitesBody_{nullptr};
    QVBoxLayout *invitesLayout_{nullptr};
};
