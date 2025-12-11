#include "MessageModel.h"

MessageModel::MessageModel(QObject *parent) : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return items_.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
        return QVariant();
    }
    const auto &item = items_.at(index.row());
    switch (role) {
        case TypeRole:
            return static_cast<int>(item.type);
        case OutgoingRole:
            return item.outgoing;
        case TextRole:
            return item.text;
        case TimeRole:
            return item.time;
        case AvatarRole:
            return item.avatarColor;
        case SystemTextRole:
            return item.systemText;
        default:
            break;
    }
    return QVariant();
}

QHash<int, QByteArray> MessageModel::roleNames() const {
    return {
        {TypeRole, "type"},
        {OutgoingRole, "outgoing"},
        {TextRole, "text"},
        {TimeRole, "time"},
        {AvatarRole, "avatar"},
        {SystemTextRole, "systemText"},
    };
}

void MessageModel::clear() {
    beginResetModel();
    items_.clear();
    lastMessageTime_ = QDateTime();
    endResetModel();
}

void MessageModel::setConversation(const QString &convId) {
    beginResetModel();
    currentConvId_ = convId;
    items_.clear();
    lastMessageTime_ = QDateTime();
    endResetModel();
}

void MessageModel::maybeInsertDivider(const QDateTime &time) {
    if (!lastMessageTime_.isValid() || lastMessageTime_.secsTo(time) > 300) {
        MessageItem divider;
        divider.type = MessageItem::Type::TimeDivider;
        divider.text = time.toString("yyyy/MM/dd HH:mm");
        divider.time = time;
        items_.append(divider);
    }
    lastMessageTime_ = time;
}

void MessageModel::appendTextMessage(const QString &convId, bool outgoing, const QString &text,
                                     const QDateTime &time) {
    if (convId != currentConvId_) {
        setConversation(convId);
    }

    maybeInsertDivider(time);

    MessageItem msg;
    msg.type = MessageItem::Type::Text;
    msg.convId = convId;
    msg.outgoing = outgoing;
    msg.text = text;
    msg.time = time;
    msg.avatarColor = QColor(outgoing ? "#5D8CFF" : "#FFAF7A");

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(msg);
    endInsertRows();
}

void MessageModel::appendSystemMessage(const QString &convId, const QString &text,
                                       const QDateTime &time) {
    if (convId != currentConvId_) {
        setConversation(convId);
    }
    maybeInsertDivider(time);
    MessageItem msg;
    msg.type = MessageItem::Type::System;
    msg.convId = convId;
    msg.text = text;
    msg.time = time;
    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(msg);
    endInsertRows();
}
