#pragma once

#include <QHash>
#include <QPixmap>
#include <QSharedPointer>
#include <QString>
#include <QVector>

class QMovie;
class QWidget;

class EmojiPackManager {
public:
    struct Item {
        QString id;
        QString title;
        QString filePath;
        bool animated{false};
    };

    static EmojiPackManager &Instance();

    void Reload();
    QVector<Item> Items() const;
    const Item *Find(const QString &id) const;

    QPixmap StickerPixmap(const QString &id, int size) const;
    QMovie *StickerMovie(const QString &id, int size, QWidget *viewport);

private:
    EmojiPackManager() = default;

    QString packRootDir() const;
    void clearCaches();

    QVector<Item> items_;
    QHash<QString, int> index_;
    mutable QHash<QString, QPixmap> pixmapCache_;
    mutable QHash<QString, QSharedPointer<QMovie>> movieCache_;
    bool initialized_{false};
};
