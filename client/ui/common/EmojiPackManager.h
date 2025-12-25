#pragma once

#include <QHash>
#include <QPixmap>
#include <QSharedPointer>
#include <QString>
#include <QVector>
#include <QtGlobal>

class QMovie;
class QTimer;
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
    void ensureMovieGcTimer();
    void trimInactiveMovies();

    struct MovieEntry {
        QSharedPointer<QMovie> movie;
        qint64 lastAccessMs{0};
    };

    QVector<Item> items_;
    QHash<QString, int> index_;
    mutable QHash<QString, QPixmap> pixmapCache_;
    QHash<QString, MovieEntry> movieCache_;
    QTimer *movieGcTimer_{nullptr};
    bool initialized_{false};
};
