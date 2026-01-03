// Message list model for chat window.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>
#include <QColor>
#include <QtGlobal>

struct MessageItem {
    enum class Type { Text, TimeDivider, System };
    enum class Status { Sent, Delivered, Read, Failed, Pending };
    enum class FileTransfer { None = 0, Uploading = 1, Downloading = 2 };

    qint64 insertedAtMs{0};
    QString id;
    QString convId;
    QString sender;
    bool outgoing{false};
    QString text;
    QDateTime time;
    Type type{Type::Text};
    Status status{Status::Sent};
    bool isFile{false};
    qint64 fileSize{0};
    QString filePath;
    FileTransfer fileTransfer{FileTransfer::None};
    int fileProgress{-1};  // 0-100, -1 unknown
    bool isSticker{false};
    QString stickerId;
    QColor avatarColor{Qt::gray};
    QString systemText;
};

class MessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        InsertedAtRole,
        MessageIdRole,
        OutgoingRole,
        SenderRole,
        TextRole,
        TimeRole,
        StatusRole,
        IsFileRole,
        FileSizeRole,
        FilePathRole,
        FileTransferRole,
        FileProgressRole,
        IsStickerRole,
        StickerIdRole,
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
                           const QDateTime &time,
                           const QString &messageId = QString(),
                           MessageItem::Status status = MessageItem::Status::Sent,
                           const QString &sender = QString(),
                           bool markInserted = true);
    void appendFileMessage(const QString &convId, bool outgoing, const QString &fileName,
                           qint64 fileSize, const QString &filePath, const QDateTime &time,
                           const QString &messageId = QString(),
                           MessageItem::Status status = MessageItem::Status::Sent,
                           const QString &sender = QString(),
                           bool markInserted = true);
    void appendStickerMessage(const QString &convId,
                              bool outgoing,
                              const QString &stickerId,
                              const QDateTime &time,
                              const QString &messageId = QString(),
                              MessageItem::Status status = MessageItem::Status::Sent,
                              const QString &sender = QString(),
                              bool markInserted = true);
    void appendSystemMessage(const QString &convId, const QString &text, const QDateTime &time);
    bool updateMessageStatus(const QString &messageId, MessageItem::Status status);
    bool updateFileTransfer(const QString &messageId, MessageItem::FileTransfer transfer, int progress = -1);
    bool updateFilePath(const QString &messageId, const QString &filePath);
    bool hasActiveFileTransfers() const;

private:
    void maybeInsertDivider(const QDateTime &time);

    QVector<MessageItem> items_;
    QString currentConvId_;
    QDateTime lastMessageTime_;
};
