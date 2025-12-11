// Message list model for chat window.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>
#include <QColor>

struct MessageItem {
    enum class Type { Text, TimeDivider, System };

    QString id;
    QString convId;
    bool outgoing{false};
    QString text;
    QDateTime time;
    Type type{Type::Text};
    QColor avatarColor{Qt::gray};
    QString systemText;
};

class MessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        OutgoingRole,
        TextRole,
        TimeRole,
        AvatarRole,
        SystemTextRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void setConversation(const QString &convId);
    void appendTextMessage(const QString &convId, bool outgoing, const QString &text,
                           const QDateTime &time);
    void appendSystemMessage(const QString &convId, const QString &text, const QDateTime &time);

private:
    void maybeInsertDivider(const QDateTime &time);

    QVector<MessageItem> items_;
    QString currentConvId_;
    QDateTime lastMessageTime_;
};
