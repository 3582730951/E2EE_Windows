#include "EmojiPackManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMovie>
#include <QSet>
#include <QWidget>

#include "UiRuntimePaths.h"

namespace {
constexpr qint64 kMaxStickerBytes = 8 * 1024 * 1024;
constexpr int kMaxStickerDim = 512;
constexpr int kMaxStickerFrames = 200;
constexpr int kMaxStickerItems = 2048;
constexpr int kMaxStickerPacks = 64;

QSet<QString> AllowedStickerExts() {
    return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("gif"), QStringLiteral("webp")};
}

QString CleanPath(const QString &path) {
    return QDir::cleanPath(path);
}

bool IsUnderDir(const QString &filePath, const QString &dirPath) {
    if (filePath.isEmpty() || dirPath.isEmpty()) {
        return false;
    }
    const QString base = dirPath + QDir::separator();
    return filePath == dirPath || filePath.startsWith(base, Qt::CaseInsensitive);
}
}  // namespace

EmojiPackManager &EmojiPackManager::Instance() {
    static EmojiPackManager inst;
    if (!inst.initialized_) {
        inst.Reload();
    }
    return inst;
}

QString EmojiPackManager::packRootDir() const {
    const QString root = UiRuntimePaths::AppRootDir();
    const QString base = root.isEmpty() ? QCoreApplication::applicationDirPath() : root;
    return QDir(base + QStringLiteral("/database/emoji_packs")).absolutePath();
}

void EmojiPackManager::clearCaches() {
    pixmapCache_.clear();
    movieCache_.clear();
}

void EmojiPackManager::Reload() {
    items_.clear();
    index_.clear();
    clearCaches();
    initialized_ = true;

    const QString root = packRootDir();
    QDir rootDir(root);
    if (!rootDir.exists()) {
        return;
    }
    const QStringList packDirs =
        rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    const QSet<QString> allowedExts = AllowedStickerExts();
    int packCount = 0;
    for (const QString &packName : packDirs) {
        if (packCount >= kMaxStickerPacks) {
            break;
        }
        const QDir packDir(rootDir.absoluteFilePath(packName));
        const QString manifestPath = packDir.filePath(QStringLiteral("manifest.json"));
        QFile file(manifestPath);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray raw = file.readAll();
        file.close();

        QJsonParseError jsonErr;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &jsonErr);
        if (doc.isNull() || !doc.isObject()) {
            continue;
        }
        const QJsonObject rootObj = doc.object();
        const QJsonArray items = rootObj.value(QStringLiteral("items")).toArray();

        const QString packCanonical =
            CleanPath(QFileInfo(packDir.absolutePath()).canonicalFilePath());
        if (packCanonical.isEmpty()) {
            continue;
        }

        for (const auto &val : items) {
            if (items_.size() >= kMaxStickerItems) {
                break;
            }
            if (!val.isObject()) {
                continue;
            }
            const QJsonObject obj = val.toObject();
            const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
            const QString fileRel = obj.value(QStringLiteral("file")).toString().trimmed();
            const QString title = obj.value(QStringLiteral("title")).toString().trimmed();
            if (id.isEmpty() || fileRel.isEmpty()) {
                continue;
            }
            if (index_.contains(id)) {
                continue;
            }

            const QString absPath = packDir.absoluteFilePath(fileRel);
            QFileInfo fi(absPath);
            if (!fi.exists() || !fi.isFile()) {
                continue;
            }
            const QString canonicalFile = CleanPath(fi.canonicalFilePath());
            if (canonicalFile.isEmpty() || !IsUnderDir(canonicalFile, packCanonical)) {
                continue;
            }
            if (fi.size() > kMaxStickerBytes) {
                continue;
            }

            const QString ext = fi.suffix().toLower();
            if (!allowedExts.contains(ext)) {
                continue;
            }

            QImageReader reader(canonicalFile);
            if (!reader.canRead()) {
                continue;
            }
            QSize size = reader.size();
            if (!size.isValid()) {
                const QImage img = reader.read();
                size = img.size();
            }
            if (!size.isValid()) {
                continue;
            }
            if (size.width() > kMaxStickerDim || size.height() > kMaxStickerDim) {
                continue;
            }

            const bool animated = reader.supportsAnimation() || ext == QStringLiteral("gif");
            const int frames = reader.imageCount();
            if (animated && frames > 0 && frames > kMaxStickerFrames) {
                continue;
            }

            Item item;
            item.id = id;
            item.title = title;
            item.filePath = canonicalFile;
            item.animated = animated;
            index_.insert(id, items_.size());
            items_.push_back(std::move(item));
        }

        ++packCount;
    }
}

QVector<EmojiPackManager::Item> EmojiPackManager::Items() const {
    return items_;
}

const EmojiPackManager::Item *EmojiPackManager::Find(const QString &id) const {
    const QString key = id.trimmed();
    if (key.isEmpty()) {
        return nullptr;
    }
    const auto it = index_.find(key);
    if (it == index_.end()) {
        return nullptr;
    }
    const int idx = it.value();
    if (idx < 0 || idx >= items_.size()) {
        return nullptr;
    }
    return &items_[idx];
}

QPixmap EmojiPackManager::StickerPixmap(const QString &id, int size) const {
    if (size <= 0) {
        return {};
    }
    const Item *item = Find(id);
    if (!item) {
        return {};
    }
    const QString key = id + QStringLiteral(":") + QString::number(size);
    const auto it = pixmapCache_.constFind(key);
    if (it != pixmapCache_.constEnd()) {
        return it.value();
    }

    QImageReader reader(item->filePath);
    if (!reader.canRead()) {
        return {};
    }
    reader.setScaledSize(QSize(size, size));
    const QImage img = reader.read();
    if (img.isNull()) {
        return {};
    }
    const QPixmap pm = QPixmap::fromImage(img);
    pixmapCache_.insert(key, pm);
    return pm;
}

QMovie *EmojiPackManager::StickerMovie(const QString &id, int size, QWidget *viewport) {
    const Item *item = Find(id);
    if (!item || !item->animated || size <= 0) {
        return nullptr;
    }
    const QString key = id + QStringLiteral(":") + QString::number(size);
    auto it = movieCache_.find(key);
    if (it != movieCache_.end()) {
        if (viewport) {
            QObject::connect(it.value().data(), &QMovie::frameChanged,
                             viewport, qOverload<>(&QWidget::update),
                             Qt::UniqueConnection);
        }
        return it.value().data();
    }

    auto movie = QSharedPointer<QMovie>::create(item->filePath);
    movie->setCacheMode(QMovie::CacheAll);
    movie->setScaledSize(QSize(size, size));
    movie->start();
    if (viewport) {
        QObject::connect(movie.data(), &QMovie::frameChanged,
                         viewport, qOverload<>(&QWidget::update),
                         Qt::UniqueConnection);
    }
    movieCache_.insert(key, movie);
    return movie.data();
}
