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
#include <QTimer>
#include <QDateTime>
#include <QWidget>
#include <QImageWriter>
#include <QProcess>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include "UiRuntimePaths.h"

namespace {
constexpr qint64 kMaxStickerBytes = 8 * 1024 * 1024;
constexpr int kMaxStickerDim = 512;
constexpr int kMaxStickerFrames = 200;
constexpr int kMaxStickerItems = 2048;
constexpr int kMaxStickerPacks = 64;
constexpr qint64 kMovieIdleMs = 1200;
constexpr int kMovieSweepMs = 500;

QSet<QString> AllowedStickerExts() {
    return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("gif"), QStringLiteral("webp")};
}

QSet<QString> AllowedImportImageExts() {
    return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp")};
}

QSet<QString> AllowedImportVideoExts() {
    return {QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
            QStringLiteral("webm"), QStringLiteral("avi")};
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

QString MakeStickerId() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint32 rnd = QRandomGenerator::global()->generate();
    return QStringLiteral("u_%1_%2").arg(now).arg(rnd, 8, 16, QChar('0'));
}

bool WriteJsonFile(const QString &path, const QJsonObject &obj, QString *outError) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outError) {
            *outError = QStringLiteral("Failed to write sticker manifest");
        }
        return false;
    }
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (outError) {
            *outError = QStringLiteral("Sticker manifest commit failed");
        }
        return false;
    }
    return true;
}

bool ConvertImageToGif(const QString &srcPath, const QString &dstPath,
                       QString *outError) {
    QImageReader reader(srcPath);
    if (!reader.canRead()) {
        if (outError) {
            *outError = QStringLiteral("Image read failed");
        }
        return false;
    }
    QSize size = reader.size();
    if (!size.isValid()) {
        const QImage img = reader.read();
        size = img.size();
    }
    if (!size.isValid()) {
        if (outError) {
            *outError = QStringLiteral("Invalid image size");
        }
        return false;
    }
    if (size.width() > kMaxStickerDim || size.height() > kMaxStickerDim) {
        const qreal scale =
            qMin(static_cast<qreal>(kMaxStickerDim) / size.width(),
                 static_cast<qreal>(kMaxStickerDim) / size.height());
        const QSize scaled(qMax(1, static_cast<int>(size.width() * scale)),
                           qMax(1, static_cast<int>(size.height() * scale)));
        reader.setScaledSize(scaled);
    }
    const QImage img = reader.read();
    if (img.isNull()) {
        if (outError) {
            *outError = QStringLiteral("Image decode failed");
        }
        return false;
    }
    QImageWriter writer(dstPath, "gif");
    if (!writer.canWrite()) {
        if (outError) {
            *outError = QStringLiteral("GIF writer unavailable");
        }
        return false;
    }
    if (!writer.write(img)) {
        if (outError) {
            *outError = QStringLiteral("GIF encode failed");
        }
        return false;
    }
    return true;
}

bool ConvertVideoToGif(const QString &srcPath, const QString &dstPath,
                       QString *outError) {
    QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        QString baseDir = UiRuntimePaths::AppRootDir();
        if (baseDir.isEmpty()) {
            baseDir = QCoreApplication::applicationDirPath();
        }
        const QString local = QDir(baseDir).filePath(QStringLiteral("ffmpeg.exe"));
        if (QFileInfo::exists(local)) {
            ffmpeg = local;
        }
    }
    if (ffmpeg.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("ffmpeg not found");
        }
        return false;
    }
    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << srcPath
         << QStringLiteral("-t") << QStringLiteral("4")
         << QStringLiteral("-vf")
         << QStringLiteral("fps=12,scale=512:-1:flags=lanczos")
         << dstPath;
    const int code = QProcess::execute(ffmpeg, args);
    if (code != 0) {
        if (outError) {
            *outError = QStringLiteral("Video to GIF failed");
        }
        return false;
    }
    return QFileInfo::exists(dstPath);
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
    if (movieGcTimer_) {
        movieGcTimer_->stop();
        movieGcTimer_->deleteLater();
        movieGcTimer_ = nullptr;
    }
}

void EmojiPackManager::ensureMovieGcTimer() {
    if (movieCache_.isEmpty()) {
        return;
    }
    if (movieGcTimer_) {
        if (!movieGcTimer_->isActive()) {
            movieGcTimer_->start();
        }
        return;
    }
    QObject *parent = QCoreApplication::instance();
    if (!parent) {
        return;
    }
    movieGcTimer_ = new QTimer(parent);
    movieGcTimer_->setInterval(kMovieSweepMs);
    movieGcTimer_->setTimerType(Qt::CoarseTimer);
    QObject::connect(movieGcTimer_, &QTimer::timeout, movieGcTimer_, [this]() {
        trimInactiveMovies();
    });
    movieGcTimer_->start();
}

void EmojiPackManager::trimInactiveMovies() {
    if (movieCache_.isEmpty()) {
        if (movieGcTimer_) {
            movieGcTimer_->stop();
        }
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool anyActive = false;
    for (auto it = movieCache_.begin(); it != movieCache_.end(); ++it) {
        auto &entry = it.value();
        if (!entry.movie) {
            continue;
        }
        const qint64 idle = now - entry.lastAccessMs;
        if (idle > kMovieIdleMs) {
            if (entry.movie->state() == QMovie::Running) {
                entry.movie->setPaused(true);
            }
            continue;
        }
        anyActive = true;
        if (entry.movie->state() == QMovie::Paused) {
            entry.movie->setPaused(false);
        } else if (entry.movie->state() == QMovie::NotRunning) {
            entry.movie->start();
        }
    }
    if (!anyActive && movieGcTimer_) {
        movieGcTimer_->stop();
    }
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

bool EmojiPackManager::ImportSticker(const QString &srcPath, QString *outId,
                                     QString *outError) {
    if (outId) {
        outId->clear();
    }
    if (outError) {
        outError->clear();
    }
    QString input = srcPath.trimmed();
    if (input.startsWith(QStringLiteral("file:"))) {
        input = QUrl(input).toLocalFile();
    }
    QFileInfo inputInfo(input);
    if (!inputInfo.exists() || !inputInfo.isFile()) {
        if (outError) {
            *outError = QStringLiteral("Source file missing");
        }
        return false;
    }
    const QString ext = inputInfo.suffix().toLower();
    const bool isVideo = AllowedImportVideoExts().contains(ext);
    const bool isImage = AllowedImportImageExts().contains(ext);
    if (!isVideo && !isImage) {
        if (outError) {
            *outError = QStringLiteral("Unsupported file type");
        }
        return false;
    }

    QDir root(packRootDir());
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        if (outError) {
            *outError = QStringLiteral("Unable to create sticker directory");
        }
        return false;
    }
    const QString packName = QStringLiteral("user");
    if (!root.exists(packName) && !root.mkpath(packName)) {
        if (outError) {
            *outError = QStringLiteral("Unable to create sticker pack");
        }
        return false;
    }
    QDir packDir(root.filePath(packName));
    const QString manifestPath = packDir.filePath(QStringLiteral("manifest.json"));

    QJsonObject manifestObj;
    QJsonArray itemsArray;
    if (QFile::exists(manifestPath)) {
        QFile file(manifestPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError jsonErr;
            const QJsonDocument doc =
                QJsonDocument::fromJson(file.readAll(), &jsonErr);
            if (!doc.isNull() && doc.isObject()) {
                manifestObj = doc.object();
                itemsArray = manifestObj.value(QStringLiteral("items")).toArray();
            }
            file.close();
        }
    }

    if (itemsArray.size() >= kMaxStickerItems) {
        if (outError) {
            *outError = QStringLiteral("Sticker limit reached");
        }
        return false;
    }

    QString id;
    for (int attempt = 0; attempt < 6; ++attempt) {
        const QString candidate = MakeStickerId();
        if (index_.contains(candidate)) {
            continue;
        }
        id = candidate;
        break;
    }
    if (id.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Failed to generate sticker id");
        }
        return false;
    }

    QString outExt = ext;
    const bool needsConvert = isVideo || (isImage && ext != QStringLiteral("gif") &&
                                          ext != QStringLiteral("webp"));
    if (needsConvert) {
        outExt = QStringLiteral("gif");
    }
    const QString fileName = id + QStringLiteral(".") + outExt;
    const QString destPath = packDir.filePath(fileName);

    bool ok = false;
    if (isVideo) {
        ok = ConvertVideoToGif(inputInfo.absoluteFilePath(), destPath, outError);
    } else if (needsConvert) {
        ok = ConvertImageToGif(inputInfo.absoluteFilePath(), destPath, outError);
    } else {
        ok = QFile::copy(inputInfo.absoluteFilePath(), destPath);
        if (!ok && outError) {
            *outError = QStringLiteral("Failed to copy sticker");
        }
    }
    if (!ok) {
        if (QFileInfo::exists(destPath)) {
            QFile::remove(destPath);
        }
        return false;
    }

    QFileInfo outInfo(destPath);
    if (!outInfo.exists() || !outInfo.isFile()) {
        if (outError) {
            *outError = QStringLiteral("Sticker output missing");
        }
        return false;
    }
    if (outInfo.size() > kMaxStickerBytes) {
        QFile::remove(destPath);
        if (outError) {
            *outError = QStringLiteral("Sticker file too large");
        }
        return false;
    }

    QJsonObject item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("title"), inputInfo.baseName());
    item.insert(QStringLiteral("file"), fileName);
    itemsArray.append(item);
    manifestObj.insert(QStringLiteral("items"), itemsArray);
    if (!WriteJsonFile(manifestPath, manifestObj, outError)) {
        QFile::remove(destPath);
        return false;
    }

    Reload();
    if (outId) {
        *outId = id;
    }
    return true;
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
        it.value().lastAccessMs = QDateTime::currentMSecsSinceEpoch();
        if (it.value().movie) {
            if (it.value().movie->state() == QMovie::Paused) {
                it.value().movie->setPaused(false);
            } else if (it.value().movie->state() == QMovie::NotRunning) {
                it.value().movie->start();
            }
        }
        ensureMovieGcTimer();
        if (viewport) {
            QObject::connect(it.value().movie.data(), &QMovie::frameChanged,
                             viewport, qOverload<>(&QWidget::update),
                             Qt::UniqueConnection);
        }
        return it.value().movie.data();
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
    MovieEntry entry;
    entry.movie = movie;
    entry.lastAccessMs = QDateTime::currentMSecsSinceEpoch();
    movieCache_.insert(key, entry);
    ensureMovieGcTimer();
    return movie.data();
}
