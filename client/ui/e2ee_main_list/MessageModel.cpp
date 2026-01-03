#include "MessageModel.h"

#include "../common/Theme.h"

namespace {

QColor AvatarColorFor(const QString &seed, bool outgoing) {
    if (outgoing) {
        return Theme::uiAccentBlue();
    }
    const uint h = qHash(seed);
    const int hue = static_cast<int>(h % 360u);
    const int sat = 140 + static_cast<int>((h >> 8) % 70u);
    const int val = 170 + static_cast<int>((h >> 16) % 70u);
    return QColor::fromHsv(hue, sat, val);
}

QString AvatarSeedForMessage(const QString &convId, bool outgoing, const QString &sender) {
    if (outgoing) {
        return QStringLiteral("self");
    }
    const QString s = sender.trimmed();
    return s.isEmpty() ? convId : s;
}

}  // namespace

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
        case InsertedAtRole:
            return item.insertedAtMs;
        case MessageIdRole:
            return item.id;
        case OutgoingRole:
            return item.outgoing;
        case SenderRole:
            return item.sender;
        case TextRole:
            return item.text;
        case TimeRole:
            return item.time;
        case StatusRole:
            return static_cast<int>(item.status);
        case IsFileRole:
            return item.isFile;
        case FileSizeRole:
            return item.fileSize;
        case FilePathRole:
            return item.filePath;
        case FileTransferRole:
            return static_cast<int>(item.fileTransfer);
        case FileProgressRole:
            return item.fileProgress;
        case IsStickerRole:
            return item.isSticker;
        case StickerIdRole:
            return item.stickerId;
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
        {InsertedAtRole, "insertedAtMs"},
        {MessageIdRole, "messageId"},
        {OutgoingRole, "outgoing"},
        {SenderRole, "sender"},
        {TextRole, "text"},
        {TimeRole, "time"},
        {StatusRole, "status"},
        {IsFileRole, "isFile"},
        {FileSizeRole, "fileSize"},
        {FilePathRole, "filePath"},
        {FileTransferRole, "fileTransfer"},
        {FileProgressRole, "fileProgress"},
        {IsStickerRole, "isSticker"},
        {StickerIdRole, "stickerId"},
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
        divider.insertedAtMs = 0;
        divider.text = time.toString("yyyy/MM/dd HH:mm");
        divider.time = time;
        items_.append(divider);
    }
    lastMessageTime_ = time;
}

void MessageModel::appendTextMessage(const QString &convId, bool outgoing, const QString &text,
                                     const QDateTime &time, const QString &messageId,
                                     MessageItem::Status status, const QString &sender,
                                     bool markInserted) {
    if (convId != currentConvId_) {
        setConversation(convId);
    }

    if (!messageId.trimmed().isEmpty()) {
        auto rank = [](MessageItem::Status s) {
            switch (s) {
            case MessageItem::Status::Read:
                return 4;
            case MessageItem::Status::Delivered:
                return 3;
            case MessageItem::Status::Sent:
                return 2;
            case MessageItem::Status::Pending:
                return 1;
            case MessageItem::Status::Failed:
                return 0;
            }
            return 0;
        };
        for (int i = 0; i < items_.size(); ++i) {
            if (items_[i].type != MessageItem::Type::Text) {
                continue;
            }
            if (items_[i].convId == convId && items_[i].id == messageId && !items_[i].isFile && !items_[i].isSticker) {
                const auto best = rank(status) > rank(items_[i].status) ? status : items_[i].status;
                if (best != items_[i].status) {
                    items_[i].status = best;
                    const QModelIndex idx = index(i, 0);
                    emit dataChanged(idx, idx, {StatusRole});
                }
                return;
            }
        }
    }

    maybeInsertDivider(time);

    MessageItem msg;
    msg.type = MessageItem::Type::Text;
    msg.insertedAtMs = markInserted ? QDateTime::currentMSecsSinceEpoch() : 0;
    msg.id = messageId;
    msg.convId = convId;
    msg.sender = sender;
    msg.outgoing = outgoing;
    msg.text = text;
    msg.time = time;
    msg.status = status;
    msg.isFile = false;
    msg.fileSize = 0;
    msg.fileTransfer = MessageItem::FileTransfer::None;
    msg.fileProgress = -1;
    msg.isSticker = false;
    msg.avatarColor = AvatarColorFor(AvatarSeedForMessage(convId, outgoing, sender), outgoing);

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(msg);
    endInsertRows();
}

void MessageModel::appendFileMessage(const QString &convId, bool outgoing, const QString &fileName,
                                     qint64 fileSize, const QString &filePath, const QDateTime &time,
                                     const QString &messageId, MessageItem::Status status,
                                     const QString &sender, bool markInserted) {
    if (convId != currentConvId_) {
        setConversation(convId);
    }

    if (!messageId.trimmed().isEmpty()) {
        auto rank = [](MessageItem::Status s) {
            switch (s) {
            case MessageItem::Status::Read:
                return 4;
            case MessageItem::Status::Delivered:
                return 3;
            case MessageItem::Status::Sent:
                return 2;
            case MessageItem::Status::Pending:
                return 1;
            case MessageItem::Status::Failed:
                return 0;
            }
            return 0;
        };
        for (int i = 0; i < items_.size(); ++i) {
            if (items_[i].type != MessageItem::Type::Text) {
                continue;
            }
            if (items_[i].convId == convId && items_[i].id == messageId && items_[i].isFile) {
                const auto best = rank(status) > rank(items_[i].status) ? status : items_[i].status;
                bool changed = false;
                if (best != items_[i].status) {
                    items_[i].status = best;
                    changed = true;
                }
                if (!filePath.trimmed().isEmpty() && items_[i].filePath.trimmed().isEmpty()) {
                    items_[i].filePath = filePath;
                    changed = true;
                }
                if (changed) {
                    const QModelIndex idx = index(i, 0);
                    emit dataChanged(idx, idx, {StatusRole, FilePathRole});
                }
                return;
            }
        }
    }

    maybeInsertDivider(time);

    MessageItem msg;
    msg.type = MessageItem::Type::Text;
    msg.insertedAtMs = markInserted ? QDateTime::currentMSecsSinceEpoch() : 0;
    msg.id = messageId;
    msg.convId = convId;
    msg.sender = sender;
    msg.outgoing = outgoing;
    msg.text = fileName;
    msg.time = time;
    msg.status = status;
    msg.isFile = true;
    msg.fileSize = fileSize;
    msg.filePath = filePath;
    msg.fileTransfer = MessageItem::FileTransfer::None;
    msg.fileProgress = -1;
    msg.isSticker = false;
    msg.avatarColor = AvatarColorFor(AvatarSeedForMessage(convId, outgoing, sender), outgoing);

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(msg);
    endInsertRows();
}

void MessageModel::appendStickerMessage(const QString &convId,
                                        bool outgoing,
                                        const QString &stickerId,
                                        const QDateTime &time,
                                        const QString &messageId,
                                        MessageItem::Status status,
                                        const QString &sender,
                                        bool markInserted) {
    if (convId != currentConvId_) {
        setConversation(convId);
    }

    if (!messageId.trimmed().isEmpty()) {
        auto rank = [](MessageItem::Status s) {
            switch (s) {
            case MessageItem::Status::Read:
                return 4;
            case MessageItem::Status::Delivered:
                return 3;
            case MessageItem::Status::Sent:
                return 2;
            case MessageItem::Status::Pending:
                return 1;
            case MessageItem::Status::Failed:
                return 0;
            }
            return 0;
        };
        for (int i = 0; i < items_.size(); ++i) {
            if (items_[i].type != MessageItem::Type::Text) {
                continue;
            }
            if (items_[i].convId == convId && items_[i].id == messageId && items_[i].isSticker) {
                const auto best = rank(status) > rank(items_[i].status) ? status : items_[i].status;
                if (best != items_[i].status) {
                    items_[i].status = best;
                    const QModelIndex idx = index(i, 0);
                    emit dataChanged(idx, idx, {StatusRole});
                }
                return;
            }
        }
    }

    maybeInsertDivider(time);

    MessageItem msg;
    msg.type = MessageItem::Type::Text;
    msg.insertedAtMs = markInserted ? QDateTime::currentMSecsSinceEpoch() : 0;
    msg.id = messageId;
    msg.convId = convId;
    msg.sender = sender;
    msg.outgoing = outgoing;
    msg.text = QStringLiteral("[贴纸]");
    msg.time = time;
    msg.status = status;
    msg.isFile = false;
    msg.fileSize = 0;
    msg.fileTransfer = MessageItem::FileTransfer::None;
    msg.fileProgress = -1;
    msg.isSticker = true;
    msg.stickerId = stickerId;
    msg.avatarColor = AvatarColorFor(AvatarSeedForMessage(convId, outgoing, sender), outgoing);

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
    msg.insertedAtMs = 0;
    msg.convId = convId;
    msg.text = text;
    msg.time = time;
    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(msg);
    endInsertRows();
}

bool MessageModel::updateMessageStatus(const QString &messageId, MessageItem::Status status) {
    if (messageId.isEmpty()) {
        return false;
    }
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id == messageId) {
            if (items_[i].status == MessageItem::Status::Read) {
                return true;
            }
            if (items_[i].status == MessageItem::Status::Delivered &&
                status != MessageItem::Status::Read) {
                return true;
            }
            if (items_[i].status == MessageItem::Status::Sent &&
                status == MessageItem::Status::Pending) {
                return true;
            }
            items_[i].status = status;
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, {StatusRole});
            return true;
        }
    }
    return false;
}

bool MessageModel::updateFileTransfer(const QString &messageId, MessageItem::FileTransfer transfer,
                                      int progress) {
    if (messageId.trimmed().isEmpty()) {
        return false;
    }
    const int clamped = (progress < 0) ? -1 : qBound(0, progress, 100);
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id != messageId || !items_[i].isFile) {
            continue;
        }
        bool changed = false;
        if (items_[i].fileTransfer != transfer) {
            items_[i].fileTransfer = transfer;
            if (transfer == MessageItem::FileTransfer::None) {
                items_[i].fileProgress = -1;
            }
            changed = true;
        }
        if (transfer != MessageItem::FileTransfer::None && items_[i].fileProgress != clamped) {
            items_[i].fileProgress = clamped;
            changed = true;
        }
        if (changed) {
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, {FileTransferRole, FileProgressRole});
        }
        return true;
    }
    return false;
}

bool MessageModel::updateFilePath(const QString &messageId, const QString &filePath) {
    const QString path = filePath.trimmed();
    if (messageId.trimmed().isEmpty() || path.isEmpty()) {
        return false;
    }
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id != messageId || !items_[i].isFile) {
            continue;
        }
        if (items_[i].filePath == path) {
            return true;
        }
        items_[i].filePath = path;
        const QModelIndex idx = index(i, 0);
        emit dataChanged(idx, idx, {FilePathRole});
        return true;
    }
    return false;
}

bool MessageModel::hasActiveFileTransfers() const {
    for (const auto &item : items_) {
        if (!item.isFile) {
            continue;
        }
        if (item.fileTransfer != MessageItem::FileTransfer::None) {
            return true;
        }
    }
    return false;
}
