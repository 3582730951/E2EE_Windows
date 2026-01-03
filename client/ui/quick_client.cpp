#include "quick_client.h"

#include <QAbstractVideoBuffer>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QClipboard>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSettings>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>
#include <QVector>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <utility>

#include "common/EmojiPackManager.h"
#include "common/ImePluginLoader.h"
#include "common/UiRuntimePaths.h"

namespace mi::client::ui {

namespace {
constexpr char kCallVoicePrefix[] = "[call]voice:";
constexpr char kCallVideoPrefix[] = "[call]video:";
constexpr int kMaxPinyinCandidatesPerKey = 5;
constexpr int kMaxAbbrInputLength = 10;
constexpr const char kPinyinDictResourcePath[] = ":/mi/e2ee/ui/ime/pinyin.dat";
constexpr const char kPinyinAbbrDictResourcePath[] =
    ":/mi/e2ee/ui/ime/pinyin_short.dat";

std::filesystem::path ToFsPath(const QString& path) {
#ifdef _WIN32
  return std::filesystem::path(path.toStdWString());
#else
  return std::filesystem::path(path.toStdString());
#endif
}

QString ResolveLocalFilePath(const QString& urlOrPath) {
  const QString trimmed = urlOrPath.trimmed();
  if (trimmed.startsWith(QStringLiteral("file:"))) {
    return QUrl(trimmed).toLocalFile();
  }
  return trimmed;
}

QString ResolveUiDataDir() {
  const QByteArray env = qgetenv("MI_E2EE_DATA_DIR");
  if (!env.isEmpty()) {
    return QString::fromLocal8Bit(env);
  }
  QString baseDir = UiRuntimePaths::AppRootDir();
  if (baseDir.isEmpty()) {
    baseDir = QCoreApplication::applicationDirPath();
  }
  return QDir(baseDir).filePath(QStringLiteral("database"));
}

QString SanitizeFileStem(QString name) {
  if (name.trimmed().isEmpty()) {
    return QStringLiteral("image");
  }
  const QString base = QFileInfo(name).completeBaseName();
  QString out;
  out.reserve(base.size());
  for (const QChar ch : base) {
    if (ch == QLatin1Char('<') || ch == QLatin1Char('>') ||
        ch == QLatin1Char(':') || ch == QLatin1Char('"') ||
        ch == QLatin1Char('/') || ch == QLatin1Char('\\') ||
        ch == QLatin1Char('|') || ch == QLatin1Char('?') ||
        ch == QLatin1Char('*')) {
      out.append(QLatin1Char('_'));
    } else {
      out.append(ch);
    }
  }
  if (out.trimmed().isEmpty()) {
    return QStringLiteral("image");
  }
  return out;
}

constexpr int kAiEnhanceScaleX2 = 2;
constexpr int kAiEnhanceScaleX4 = 4;

int ClampEnhanceScale(int scale) {
  return scale == kAiEnhanceScaleX4 ? kAiEnhanceScaleX4 : kAiEnhanceScaleX2;
}

int ResolveEnhanceScale(int requestedScale, bool x4Confirmed) {
  const int clamped = ClampEnhanceScale(requestedScale);
  if (clamped == kAiEnhanceScaleX4 && !x4Confirmed) {
    return kAiEnhanceScaleX2;
  }
  return clamped;
}

QString AiSettingsPath() {
  const QString dataDir = ResolveUiDataDir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("ai_settings.ini"));
}

struct AiEnhanceRecommendation {
  int perf_scale{kAiEnhanceScaleX2};
  int quality_scale{kAiEnhanceScaleX2};
};

AiEnhanceRecommendation BuildAiEnhanceRecommendation(int gpuSeries,
                                                     bool gpuAvailable) {
  AiEnhanceRecommendation rec;
  if (!gpuAvailable) {
    return rec;
  }
  if (gpuSeries >= 40) {
    rec.quality_scale = kAiEnhanceScaleX4;
  } else if (gpuSeries >= 30) {
    rec.quality_scale = kAiEnhanceScaleX4;
  } else if (gpuSeries >= 20) {
    rec.quality_scale = kAiEnhanceScaleX2;
  } else if (gpuSeries >= 10) {
    rec.quality_scale = kAiEnhanceScaleX2;
  }
  return rec;
}

QString RunCommandOutput(const QString& program,
                         const QStringList& args,
                         int timeoutMs) {
  QProcess proc;
  proc.start(program, args);
  if (!proc.waitForFinished(timeoutMs)) {
    proc.kill();
    proc.waitForFinished(250);
    return {};
  }
  QByteArray output = proc.readAllStandardOutput();
  if (output.trimmed().isEmpty()) {
    output = proc.readAllStandardError();
  }
  return QString::fromLocal8Bit(output).trimmed();
}

QStringList ParseGpuNames(const QString& output) {
  QStringList lines =
      output.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                   Qt::SkipEmptyParts);
  QStringList names;
  for (const auto& line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }
    if (trimmed.compare(QStringLiteral("Name"), Qt::CaseInsensitive) == 0) {
      continue;
    }
    names.push_back(trimmed);
  }
  return names;
}

QString PickPreferredGpuName(const QStringList& names) {
  for (const auto& name : names) {
    if (name.contains(QStringLiteral("NVIDIA"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("RTX"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("GTX"), Qt::CaseInsensitive)) {
      return name.trimmed();
    }
  }
  for (const auto& name : names) {
    if (!name.trimmed().isEmpty()) {
      return name.trimmed();
    }
  }
  return {};
}

QString QueryGpuName() {
#ifdef _WIN32
  const QString wmicOutput = RunCommandOutput(
      QStringLiteral("wmic"),
      {QStringLiteral("path"),
       QStringLiteral("win32_VideoController"),
       QStringLiteral("get"),
       QStringLiteral("Name")},
      2000);
  QStringList names = ParseGpuNames(wmicOutput);
  if (names.isEmpty()) {
    const QString psOutput = RunCommandOutput(
        QStringLiteral("powershell"),
        {QStringLiteral("-NoProfile"),
         QStringLiteral("-Command"),
         QStringLiteral("Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name")},
        2500);
    names = ParseGpuNames(psOutput);
  }
  return PickPreferredGpuName(names);
#else
  return {};
#endif
}

int ParseNvidiaSeries(const QString& gpuName) {
  const QString lowered = gpuName.toLower();
  if (!lowered.contains(QStringLiteral("nvidia")) &&
      !lowered.contains(QStringLiteral("rtx")) &&
      !lowered.contains(QStringLiteral("gtx"))) {
    return 0;
  }
  const QRegularExpression re(
      QStringLiteral("(rtx|gtx)\\s*(\\d{4})"),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = re.match(gpuName);
  if (!match.hasMatch()) {
    return 0;
  }
  bool ok = false;
  const int model = match.captured(2).toInt(&ok);
  if (!ok || model < 1000) {
    return 0;
  }
  const int series = (model / 1000) * 10;
  if (series < 10 || series > 50) {
    return 0;
  }
  return series;
}

QString FindRealEsrganPath(bool* supportsGpu) {
  if (supportsGpu) {
    *supportsGpu = false;
  }
  const QStringList names = {
      QStringLiteral("realesrgan-ncnn-vulkan.exe"),
      QStringLiteral("realesrgan-ncnn-vulkan"),
      QStringLiteral("realesrgan-ncnn.exe"),
      QStringLiteral("realesrgan-ncnn")
  };

  for (const auto& name : names) {
    const QString hit = QStandardPaths::findExecutable(name);
    if (!hit.isEmpty()) {
      if (supportsGpu) {
        *supportsGpu = name.contains(QStringLiteral("vulkan"),
                                     Qt::CaseInsensitive);
      }
      return hit;
    }
  }

  QString baseDir = UiRuntimePaths::AppRootDir();
  if (baseDir.isEmpty()) {
    baseDir = QCoreApplication::applicationDirPath();
  }
  const QString runtimeDir = UiRuntimePaths::RuntimeDir();
  const QStringList roots = {
      baseDir,
      QDir(baseDir).filePath(QStringLiteral("tools/realesrgan")),
      runtimeDir,
      runtimeDir.isEmpty() ? QString() : QDir(runtimeDir).filePath(QStringLiteral("tools/realesrgan"))
  };

  for (const auto& root : roots) {
    if (root.isEmpty()) {
      continue;
    }
    for (const auto& name : names) {
      const QString candidate = QDir(root).filePath(name);
      if (QFileInfo::exists(candidate)) {
        if (supportsGpu) {
          *supportsGpu = name.contains(QStringLiteral("vulkan"),
                                       Qt::CaseInsensitive);
        }
        return candidate;
      }
    }
  }
  return {};
}

bool DetectAiEnhanceGpuAvailable() {
  bool gpuSupported = false;
  const QString exe = FindRealEsrganPath(&gpuSupported);
  if (exe.isEmpty() || !gpuSupported) {
    return false;
  }
#ifdef _WIN32
  const QString systemRoot = qEnvironmentVariable("SystemRoot");
  if (!systemRoot.isEmpty()) {
    const QString vulkan =
        QDir(systemRoot).filePath(QStringLiteral("System32/vulkan-1.dll"));
    if (!QFileInfo::exists(vulkan)) {
      return false;
    }
  }
#endif
  return true;
}

QString FindRealEsrganModelDir(const QString& exePath,
                               const QString& modelName) {
  const QString trimmedModel = modelName.trimmed();
  if (trimmedModel.isEmpty()) {
    return {};
  }
  const QString dataDir = ResolveUiDataDir();
  const QString baseDir = UiRuntimePaths::AppRootDir();
  const QString runtimeDir = UiRuntimePaths::RuntimeDir();
  const QString exeDir = exePath.isEmpty()
                             ? QString()
                             : QFileInfo(exePath).dir().absolutePath();
  const QStringList roots = {
      exeDir.isEmpty() ? QString() : QDir(exeDir).filePath(QStringLiteral("models")),
      dataDir.isEmpty() ? QString() : QDir(dataDir).filePath(QStringLiteral("ai_models/realesrgan")),
      baseDir.isEmpty() ? QString() : QDir(baseDir).filePath(QStringLiteral("models/realesrgan")),
      runtimeDir.isEmpty() ? QString() : QDir(runtimeDir).filePath(QStringLiteral("models/realesrgan"))
  };
  for (const auto& root : roots) {
    if (root.isEmpty()) {
      continue;
    }
    const QString param =
        QDir(root).filePath(trimmedModel + QStringLiteral(".param"));
    const QString bin =
        QDir(root).filePath(trimmedModel + QStringLiteral(".bin"));
    if (QFileInfo::exists(param) && QFileInfo::exists(bin)) {
      return root;
    }
  }
  return {};
}

QString SelectRealEsrganModelName(int scale, bool anime) {
  const int clamped = ClampEnhanceScale(scale);
  if (clamped == kAiEnhanceScaleX2) {
    return QStringLiteral("realesrgan-x2plus");
  }
  if (anime) {
    return QStringLiteral("realesrgan-x4plus-anime");
  }
  return QStringLiteral("realesrgan-x4plus");
}

bool LoadAiEnhanceSettings(bool gpuAvailable,
                           const AiEnhanceRecommendation& rec,
                           bool& enabled,
                           int& quality,
                           bool& x4Confirmed) {
  const QString path = AiSettingsPath();
  if (path.isEmpty()) {
    enabled = gpuAvailable;
    quality = rec.perf_scale;
    x4Confirmed = false;
    return false;
  }
  if (!QFileInfo::exists(path)) {
    enabled = gpuAvailable;
    quality = rec.perf_scale;
    x4Confirmed = false;
    return false;
  }
  QSettings settings(path, QSettings::IniFormat);
  enabled = settings.value(QStringLiteral("ai/enabled"), gpuAvailable).toBool();
  quality = settings.value(QStringLiteral("ai/quality"), rec.perf_scale).toInt();
  const bool hasConfirm = settings.contains(QStringLiteral("ai/x4_confirmed"));
  x4Confirmed =
      settings.value(QStringLiteral("ai/x4_confirmed"), false).toBool();
  quality = ClampEnhanceScale(quality);
  if (!hasConfirm && quality == kAiEnhanceScaleX4) {
    x4Confirmed = true;
  }
  return true;
}

void SaveAiEnhanceSettings(bool enabled, int quality, bool x4Confirmed) {
  const QString path = AiSettingsPath();
  if (path.isEmpty()) {
    return;
  }
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(QStringLiteral("ai/enabled"), enabled);
  settings.setValue(QStringLiteral("ai/quality"), ClampEnhanceScale(quality));
  settings.setValue(QStringLiteral("ai/x4_confirmed"), x4Confirmed);
  settings.sync();
}

bool IsSessionInvalidError(const QString& message) {
  const QString lowered = message.trimmed().toLower();
  return lowered == QStringLiteral("unauthorized") ||
         lowered == QStringLiteral("session invalid") ||
         lowered == QStringLiteral("not logged in");
}

struct PinyinIndex {
  QHash<QString, QStringList> dict;
  QVector<QString> keys;
  QSet<QString> keySet;
  int maxKeyLength{0};
  QHash<QString, QStringList> abbrDict;
  QVector<QString> abbrKeys;
};

bool LoadPinyinDictFromResource(const char* resourcePath,
                                QHash<QString, QStringList>& dict,
                                int* maxKeyLength) {
  QFile file(QString::fromLatin1(resourcePath));
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
  int maxLen = 0;
  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    if (line.isEmpty() || line.startsWith(QChar('#'))) {
      continue;
    }
    const int tab = line.indexOf(QChar('\t'));
    if (tab <= 0) {
      continue;
    }
    const QString key = line.left(tab).trimmed();
    const QString phrase = line.mid(tab + 1).trimmed();
    if (key.isEmpty() || phrase.isEmpty()) {
      continue;
    }
    auto& list = dict[key];
    if (list.size() >= kMaxPinyinCandidatesPerKey || list.contains(phrase)) {
      continue;
    }
    list.push_back(phrase);
    maxLen = qMax(maxLen, key.size());
  }
  if (maxKeyLength) {
    *maxKeyLength = maxLen;
  }
  return !dict.isEmpty();
}

PinyinIndex BuildPinyinIndex() {
  PinyinIndex index;
  LoadPinyinDictFromResource(kPinyinDictResourcePath, index.dict,
                             &index.maxKeyLength);
  LoadPinyinDictFromResource(kPinyinAbbrDictResourcePath, index.abbrDict,
                             nullptr);
  index.keys.reserve(index.dict.size());
  for (auto it = index.dict.constBegin(); it != index.dict.constEnd(); ++it) {
    index.keys.push_back(it.key());
    index.keySet.insert(it.key());
    index.maxKeyLength = qMax(index.maxKeyLength, it.key().size());
  }
  std::sort(index.keys.begin(), index.keys.end());
  index.abbrKeys.reserve(index.abbrDict.size());
  for (auto it = index.abbrDict.constBegin(); it != index.abbrDict.constEnd();
       ++it) {
    index.abbrKeys.push_back(it.key());
  }
  std::sort(index.abbrKeys.begin(), index.abbrKeys.end());
  return index;
}

const PinyinIndex& GetPinyinIndex() {
  static PinyinIndex index = BuildPinyinIndex();
  return index;
}

void AppendCandidate(QStringList& list, const QString& candidate, int limit) {
  if (candidate.isEmpty() || list.contains(candidate)) {
    return;
  }
  list.push_back(candidate);
  if (limit > 0 && list.size() > limit) {
    list.removeLast();
  }
}

bool IsAudioFormatSupported(const QAudioDevice& device,
                            int sampleRate,
                            int channels) {
  if (device.isNull() || sampleRate <= 0 || channels <= 0) {
    return false;
  }
  QAudioFormat format;
  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(QAudioFormat::Int16);
  return device.isFormatSupported(format);
}

bool PickPreferredAudioFormat(const QAudioDevice& device,
                              int& sampleRate,
                              int& channels) {
  if (device.isNull()) {
    return false;
  }
  const QAudioFormat preferred = device.preferredFormat();
  if (preferred.sampleFormat() != QAudioFormat::Int16) {
    return false;
  }
  const int rate = preferred.sampleRate();
  const int ch = preferred.channelCount();
  if (rate <= 0 || ch <= 0) {
    return false;
  }
  if (!device.isFormatSupported(preferred)) {
    return false;
  }
  sampleRate = rate;
  channels = ch;
  return true;
}

bool FindCandidateAudioFormat(const QAudioDevice& inDevice,
                              const QAudioDevice& outDevice,
                              bool checkIn,
                              bool checkOut,
                              int& sampleRate,
                              int& channels) {
  const std::array<int, 5> rates = {48000, 44100, 32000, 24000, 16000};
  const std::array<int, 2> chans = {1, 2};
  for (const int rate : rates) {
    for (const int ch : chans) {
      if (checkIn && !IsAudioFormatSupported(inDevice, rate, ch)) {
        continue;
      }
      if (checkOut && !IsAudioFormatSupported(outDevice, rate, ch)) {
        continue;
      }
      sampleRate = rate;
      channels = ch;
      return true;
    }
  }
  return false;
}

void AdjustAudioConfigForDevices(
    const QAudioDevice& inDevice,
    const QAudioDevice& outDevice,
    mi::client::media::AudioPipelineConfig& config) {
  const bool haveIn = !inDevice.isNull();
  const bool haveOut = !outDevice.isNull();
  if (!haveIn && !haveOut) {
    return;
  }
  const bool inOk =
      !haveIn || IsAudioFormatSupported(inDevice, config.sample_rate,
                                        config.channels);
  const bool outOk =
      !haveOut || IsAudioFormatSupported(outDevice, config.sample_rate,
                                         config.channels);
  if (inOk && outOk) {
    return;
  }

  int rate = config.sample_rate;
  int ch = config.channels;
  if (haveIn && haveOut) {
    if (FindCandidateAudioFormat(inDevice, outDevice, true, true, rate, ch)) {
      config.sample_rate = rate;
      config.channels = ch;
      return;
    }
    int prefRate = 0;
    int prefCh = 0;
    if (PickPreferredAudioFormat(inDevice, prefRate, prefCh) &&
        IsAudioFormatSupported(outDevice, prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
    if (PickPreferredAudioFormat(outDevice, prefRate, prefCh) &&
        IsAudioFormatSupported(inDevice, prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
  if (haveIn) {
    int prefRate = 0;
    int prefCh = 0;
    if (PickPreferredAudioFormat(inDevice, prefRate, prefCh) ||
        FindCandidateAudioFormat(inDevice, outDevice, true, false,
                                 prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
  if (haveOut) {
    int prefRate = 0;
    int prefCh = 0;
    if (PickPreferredAudioFormat(outDevice, prefRate, prefCh) ||
        FindCandidateAudioFormat(inDevice, outDevice, false, true,
                                 prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
}

QString SegmentFallback(const QString& pinyin) {
  const auto& index = GetPinyinIndex();
  const int n = pinyin.size();
  const int maxLen = index.maxKeyLength;
  if (n <= 0 || maxLen <= 0) {
    return {};
  }
  QVector<int> score(n + 1, -1);
  QVector<int> prev(n + 1, -1);
  QVector<QString> prevKey(n + 1);
  score[0] = 0;
  for (int i = 0; i < n; ++i) {
    if (score[i] < 0) {
      continue;
    }
    const int limit = qMin(maxLen, n - i);
    for (int len = 1; len <= limit; ++len) {
      const QString key = pinyin.mid(i, len);
      if (!index.keySet.contains(key)) {
        continue;
      }
      const int j = i + len;
      const int nextScore = score[i] + len * 2 - 1;
      if (nextScore > score[j]) {
        score[j] = nextScore;
        prev[j] = i;
        prevKey[j] = key;
      }
    }
  }
  if (score[n] < 0) {
    return {};
  }
  QStringList chunks;
  int cur = n;
  while (cur > 0 && prev[cur] >= 0) {
    const QString key = prevKey[cur];
    const auto it = index.dict.constFind(key);
    if (it != index.dict.constEnd() && !it.value().isEmpty()) {
      chunks.push_front(it.value().front());
    }
    cur = prev[cur];
  }
  return chunks.join(QString());
}

QStringList BuildPinyinCandidates(const QString& pinyin, int limit) {
  const auto& index = GetPinyinIndex();
  QStringList list;
  if (pinyin.isEmpty()) {
    return list;
  }
  const auto it = index.dict.constFind(pinyin);
  if (it != index.dict.constEnd()) {
    list = it.value();
  }
  const bool allowAbbr = pinyin.size() <= kMaxAbbrInputLength;
  if (allowAbbr) {
    const auto abbrIt = index.abbrDict.constFind(pinyin);
    if (abbrIt != index.abbrDict.constEnd()) {
      for (const auto& cand : abbrIt.value()) {
        AppendCandidate(list, cand, limit);
        if (limit > 0 && list.size() >= limit) {
          break;
        }
      }
    }
  }
  const QString fallback = SegmentFallback(pinyin);
  if (!fallback.isEmpty()) {
    AppendCandidate(list, fallback, limit);
  }
  if (list.size() < limit) {
    auto itKey = std::lower_bound(index.keys.begin(), index.keys.end(),
                                  pinyin);
    for (; itKey != index.keys.end(); ++itKey) {
      if (!itKey->startsWith(pinyin)) {
        break;
      }
      if (*itKey == pinyin) {
        continue;
      }
      const auto hit = index.dict.constFind(*itKey);
      if (hit == index.dict.constEnd() || hit.value().isEmpty()) {
        continue;
      }
      AppendCandidate(list, hit.value().front(), limit);
      if (list.size() >= limit) {
        break;
      }
    }
  }
  if (allowAbbr && list.size() < limit) {
    auto itKey =
        std::lower_bound(index.abbrKeys.begin(), index.abbrKeys.end(), pinyin);
    for (; itKey != index.abbrKeys.end(); ++itKey) {
      if (!itKey->startsWith(pinyin)) {
        break;
      }
      if (*itKey == pinyin) {
        continue;
      }
      const auto hit = index.abbrDict.constFind(*itKey);
      if (hit == index.abbrDict.constEnd() || hit.value().isEmpty()) {
        continue;
      }
      AppendCandidate(list, hit.value().front(), limit);
      if (list.size() >= limit) {
        break;
      }
    }
  }
  if (list.isEmpty()) {
    list.push_back(pinyin);
  }
  if (limit > 0 && list.size() > limit) {
    list = list.mid(0, limit);
  }
  return list;
}

QString FindConfigFile(const QString& name) {
  if (name.isEmpty()) {
    return {};
  }
  const QFileInfo info(name);
  const QString appRoot = UiRuntimePaths::AppRootDir();
  const QString baseDir =
      appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
  if (info.isAbsolute()) {
    return QFile::exists(name) ? name : QString();
  }
  if (info.path() != QStringLiteral(".") && !info.path().isEmpty()) {
    const QString candidate = baseDir + QStringLiteral("/") + name;
    if (QFile::exists(candidate)) {
      return candidate;
    }
    if (QFile::exists(name)) {
      return QFileInfo(name).absoluteFilePath();
    }
    return {};
  }
  const QString in_config = baseDir + QStringLiteral("/config/") + name;
  if (QFile::exists(in_config)) {
    return in_config;
  }
  const QString in_app = baseDir + QStringLiteral("/") + name;
  if (QFile::exists(in_app)) {
    return in_app;
  }
  if (QFile::exists(name)) {
    return QFileInfo(name).absoluteFilePath();
  }
  return {};
}

QString NowTimeString() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

struct CallInvite {
  bool ok{false};
  bool video{false};
  QString callId;
};

CallInvite ParseCallInvite(const QString& text) {
  CallInvite invite;
  if (text.startsWith(QString::fromLatin1(kCallVoicePrefix))) {
    invite.ok = true;
    invite.video = false;
    invite.callId = text.mid(static_cast<int>(std::strlen(kCallVoicePrefix)));
  } else if (text.startsWith(QString::fromLatin1(kCallVideoPrefix))) {
    invite.ok = true;
    invite.video = true;
    invite.callId = text.mid(static_cast<int>(std::strlen(kCallVideoPrefix)));
  }
  invite.callId = invite.callId.trimmed();
  if (invite.callId.isEmpty()) {
    invite.ok = false;
  }
  return invite;
}

QString FormatCoordE7(std::int32_t v_e7) {
  const qint64 v64 = static_cast<qint64>(v_e7);
  const bool neg = v64 < 0;
  const quint64 abs = static_cast<quint64>(neg ? -v64 : v64);
  const quint64 deg = abs / 10000000ULL;
  const quint64 frac = abs % 10000000ULL;
  return QStringLiteral("%1%2.%3")
      .arg(neg ? QStringLiteral("-") : QString())
      .arg(deg)
      .arg(frac, 7, 10, QChar('0'));
}

QString FormatLocationText(double lat, double lon, const QString& label) {
  const auto lat_e7 = static_cast<std::int32_t>(std::llround(lat * 10000000.0));
  const auto lon_e7 = static_cast<std::int32_t>(std::llround(lon * 10000000.0));
  const QString safeLabel =
      label.trimmed().isEmpty() ? QStringLiteral("（未命名）") : label.trimmed();
  return QStringLiteral("【位置】%1\nlat:%2, lon:%3")
      .arg(safeLabel, FormatCoordE7(lat_e7), FormatCoordE7(lon_e7));
}

QString SanitizeFileId(const QString& fileId) {
  QString out;
  out.reserve(fileId.size());
  for (const QChar ch : fileId) {
    if ((ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) ||
        (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) ||
        (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) ||
        ch == QLatin1Char('_') || ch == QLatin1Char('-')) {
      out.append(ch);
    } else {
      out.append(QLatin1Char('_'));
    }
  }
  if (out.isEmpty()) {
    out = QStringLiteral("file");
  }
  if (out.size() > 64) {
    out = out.left(64);
  }
  return out;
}

QString ResolveAiUpscaleDir() {
  const QString dataDir = ResolveUiDataDir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("ai_upscale"));
}

bool EnsureAiUpscaleDir(QDir& outDir, QString& error) {
  const QString dirPath = ResolveAiUpscaleDir();
  if (dirPath.isEmpty()) {
    error = QStringLiteral("存储目录无效");
    return false;
  }
  QDir dir(dirPath);
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    error = QStringLiteral("创建超清目录失败");
    return false;
  }
  outDir = dir;
  return true;
}

QString BuildEnhancedImagePath(const QString& messageId,
                               int scale,
                               QString& error) {
  const QString trimmed = messageId.trimmed();
  if (trimmed.isEmpty()) {
    error = QStringLiteral("图片标识无效");
    return {};
  }
  QDir outDir;
  if (!EnsureAiUpscaleDir(outDir, error)) {
    return {};
  }
  const QString token = SanitizeFileId(trimmed);
  if (token.isEmpty()) {
    error = QStringLiteral("图片标识无效");
    return {};
  }
  const int clamped = ClampEnhanceScale(scale);
  return outDir.filePath(
      QStringLiteral("msg_%1_x%2.png").arg(token).arg(clamped));
}

QString EnhancedImagePathIfExists(const QString& messageId) {
  QString error;
  const std::array<int, 2> scales = {kAiEnhanceScaleX4, kAiEnhanceScaleX2};
  for (const int scale : scales) {
    const QString path = BuildEnhancedImagePath(messageId, scale, error);
    if (path.isEmpty()) {
      continue;
    }
    if (QFileInfo::exists(path)) {
      return path;
    }
  }
  return {};
}

struct ImageQualityMetrics {
  bool valid{false};
  int width{0};
  int height{0};
  bool low_res{false};
  double sharpness{0.0};
  double noise{0.0};
  bool anime_like{false};
};

ImageQualityMetrics AnalyzeImageQuality(const QString& path) {
  ImageQualityMetrics metrics;
  QImageReader reader(path);
  reader.setAutoTransform(true);
  QSize size = reader.size();
  QImage image;
  constexpr int kAnalyzeMaxDim = 256;

  if (size.isValid()) {
    metrics.width = size.width();
    metrics.height = size.height();
    QSize scaled = size;
    if (std::max(scaled.width(), scaled.height()) > kAnalyzeMaxDim) {
      scaled.scale(kAnalyzeMaxDim, kAnalyzeMaxDim, Qt::KeepAspectRatio);
      reader.setScaledSize(scaled);
    }
    image = reader.read();
  } else {
    image = reader.read();
    if (!image.isNull()) {
      size = image.size();
      metrics.width = size.width();
      metrics.height = size.height();
      if (std::max(size.width(), size.height()) > kAnalyzeMaxDim) {
        image = image.scaled(kAnalyzeMaxDim, kAnalyzeMaxDim,
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
      }
    }
  }

  if (metrics.width > 0 && metrics.height > 0) {
    const int minSide = std::min(metrics.width, metrics.height);
    const std::int64_t area =
        static_cast<std::int64_t>(metrics.width) *
        static_cast<std::int64_t>(metrics.height);
    constexpr int kLowResMinSide = 900;
    constexpr std::int64_t kLowResArea = 1000000;
    metrics.low_res = (minSide < kLowResMinSide) || (area < kLowResArea);
  }

  if (image.isNull()) {
    return metrics;
  }

  QImage color = image.convertToFormat(QImage::Format_ARGB32);
  const int colorW = color.width();
  const int colorH = color.height();
  int colorSamples = 0;
  int uniqueColors = 0;
  double saturationSum = 0.0;
  std::vector<bool> colorSeen(32768, false);
  if (colorW > 0 && colorH > 0) {
    const int step = std::max(colorW, colorH) > 128 ? 2 : 1;
    for (int y = 0; y < colorH; y += step) {
      const QRgb* line =
          reinterpret_cast<const QRgb*>(color.constScanLine(y));
      for (int x = 0; x < colorW; x += step) {
        const QRgb px = line[x];
        const int r = qRed(px);
        const int g = qGreen(px);
        const int b = qBlue(px);
        const int maxc = std::max({r, g, b});
        const int minc = std::min({r, g, b});
        if (maxc > 0) {
          saturationSum +=
              static_cast<double>(maxc - minc) / static_cast<double>(maxc);
        }
        const int key =
            ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
        if (!colorSeen[static_cast<std::size_t>(key)]) {
          colorSeen[static_cast<std::size_t>(key)] = true;
          ++uniqueColors;
        }
        ++colorSamples;
      }
    }
  }

  QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
  const int w = gray.width();
  const int h = gray.height();
  if (w < 3 || h < 3) {
    metrics.valid = true;
    return metrics;
  }

  double sum = 0.0;
  double sum2 = 0.0;
  double noiseSum = 0.0;
  int edgeCount = 0;
  int count = 0;
  constexpr int kEdgeThreshold = 25;

  for (int y = 1; y < h - 1; ++y) {
    const uchar* prev = gray.constScanLine(y - 1);
    const uchar* cur = gray.constScanLine(y);
    const uchar* next = gray.constScanLine(y + 1);
    for (int x = 1; x < w - 1; ++x) {
      const int center = cur[x];
      const int lap =
          -4 * center + cur[x - 1] + cur[x + 1] + prev[x] + next[x];
      sum += lap;
      sum2 += static_cast<double>(lap) * static_cast<double>(lap);
      if (std::abs(lap) > kEdgeThreshold) {
        ++edgeCount;
      }
      const int mean =
          (center + cur[x - 1] + cur[x + 1] + prev[x] + next[x] +
           prev[x - 1] + prev[x + 1] + next[x - 1] + next[x + 1]) /
          9;
      noiseSum += std::abs(center - mean);
      ++count;
    }
  }

  if (count > 0) {
    const double mean = sum / count;
    metrics.sharpness = (sum2 / count) - (mean * mean);
    metrics.noise = noiseSum / count;
    const double edgeRatio =
        static_cast<double>(edgeCount) / static_cast<double>(count);
    const double uniqueRatio =
        colorSamples > 0
            ? static_cast<double>(uniqueColors) /
                  static_cast<double>(colorSamples)
            : 1.0;
    const double avgSat =
        colorSamples > 0 ? saturationSum / static_cast<double>(colorSamples)
                         : 0.0;
    metrics.anime_like =
        avgSat > 0.08 && uniqueRatio < 0.18 && edgeRatio > 0.08;
  }
  metrics.valid = true;
  return metrics;
}

bool ShouldAutoEnhanceImage(const QString& path) {
  const ImageQualityMetrics metrics = AnalyzeImageQuality(path);
  if (!metrics.valid) {
    return false;
  }
  if (metrics.low_res) {
    return true;
  }
  constexpr double kSharpnessThreshold = 100.0;
  constexpr double kNoiseThreshold = 12.0;
  return metrics.sharpness < kSharpnessThreshold ||
         metrics.noise > kNoiseThreshold;
}

bool IsVideoExt(const QString& ext) {
  const QString e = ext.toLower();
  return e == QStringLiteral("mp4") || e == QStringLiteral("mov") ||
         e == QStringLiteral("mkv") || e == QStringLiteral("webm") ||
         e == QStringLiteral("avi");
}

bool IsImageExt(const QString& ext) {
  const QString e = ext.toLower();
  return e == QStringLiteral("png") || e == QStringLiteral("jpg") ||
         e == QStringLiteral("jpeg") || e == QStringLiteral("webp") ||
         e == QStringLiteral("bmp");
}

bool IsGifExt(const QString& ext) {
  return ext.toLower() == QStringLiteral("gif");
}

bool IsAlreadyCompressedExt(const QString& ext) {
  const QString e = ext.toLower();
  static const QSet<QString> kCompressed = {
      QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
      QStringLiteral("gif"),  QStringLiteral("webp"), QStringLiteral("bmp"),
      QStringLiteral("ico"),  QStringLiteral("heic"),
      QStringLiteral("mp4"),  QStringLiteral("mkv"),  QStringLiteral("mov"),
      QStringLiteral("webm"), QStringLiteral("avi"),  QStringLiteral("flv"),
      QStringLiteral("m4v"),  QStringLiteral("mp3"),  QStringLiteral("m4a"),
      QStringLiteral("aac"),  QStringLiteral("ogg"),  QStringLiteral("opus"),
      QStringLiteral("flac"), QStringLiteral("wav"),  QStringLiteral("zip"),
      QStringLiteral("rar"),  QStringLiteral("7z"),   QStringLiteral("gz"),
      QStringLiteral("bz2"),  QStringLiteral("xz"),   QStringLiteral("zst"),
      QStringLiteral("pdf"),  QStringLiteral("docx"), QStringLiteral("xlsx"),
      QStringLiteral("pptx")
  };
  return kCompressed.contains(e);
}

constexpr quint64 kMaxAttachmentCacheBytes = 200ull * 1024ull * 1024ull * 1024ull;
constexpr quint64 kTier128M = 128ull * 1024ull * 1024ull;
constexpr quint64 kTier256M = 256ull * 1024ull * 1024ull;
constexpr quint64 kTier512M = 512ull * 1024ull * 1024ull;
constexpr quint64 kTier1G = 1024ull * 1024ull * 1024ull;
constexpr quint64 kTier2G = 2ull * 1024ull * 1024ull * 1024ull;
constexpr quint64 kTier10G = 10ull * 1024ull * 1024ull * 1024ull;

constexpr char kAttachmentCacheMagic[8] = {'M', 'I', 'A', 'C',
                                           'A', 'C', 'H', 'E'};
constexpr quint8 kAttachmentCacheVersion = 1;
constexpr char kAttachmentChunkMagic[4] = {'M', 'I', 'A', 'C'};
constexpr quint8 kAttachmentChunkVersion = 1;

enum class CacheChunkMethod : quint8 {
  kRaw = 0,
  kDeflate = 1,
  kDeflate2 = 2
};

struct CachePolicy {
  int level{1};
  int passes{1};
  quint64 chunkBytes{4ull * 1024ull * 1024ull};
  bool keepRaw{false};
  bool forceRaw{false};
};

struct CacheIndex {
  quint64 fileSize{0};
  quint64 chunkBytes{0};
  quint32 chunkCount{0};
  quint8 flags{0};
  quint8 level{0};
  quint8 passes{1};
  QString fileName;
  QString rawName;
};

constexpr quint8 kCacheFlagKeepRaw = 0x1;
constexpr quint8 kCacheFlagForceRaw = 0x2;

class LambdaTask final : public QRunnable {
 public:
  explicit LambdaTask(std::function<void()> fn) : fn_(std::move(fn)) {}
  void run() override { fn_(); }

 private:
  std::function<void()> fn_;
};

CachePolicy SelectCachePolicy(quint64 fileSize) {
  CachePolicy policy;
  if (fileSize == 0) {
    policy.level = 5;
    policy.passes = 1;
    policy.chunkBytes = 8ull * 1024ull * 1024ull;
    policy.keepRaw = false;
    return policy;
  }
  if (fileSize <= kTier128M) {
    policy.level = 1;
    policy.passes = 1;
    policy.chunkBytes = 4ull * 1024ull * 1024ull;
    policy.keepRaw = true;
  } else if (fileSize <= kTier256M) {
    policy.level = 3;
    policy.passes = 1;
    policy.chunkBytes = 8ull * 1024ull * 1024ull;
  } else if (fileSize <= kTier512M) {
    policy.level = 5;
    policy.passes = 1;
    policy.chunkBytes = 16ull * 1024ull * 1024ull;
  } else if (fileSize <= kTier1G) {
    policy.level = 7;
    policy.passes = 1;
    policy.chunkBytes = 32ull * 1024ull * 1024ull;
  } else if (fileSize <= kTier2G) {
    policy.level = 9;
    policy.passes = 1;
    policy.chunkBytes = 32ull * 1024ull * 1024ull;
  } else if (fileSize <= kTier10G) {
    policy.level = 9;
    policy.passes = 2;
    policy.chunkBytes = 64ull * 1024ull * 1024ull;
  } else {
    policy.level = 9;
    policy.passes = 2;
    policy.chunkBytes = 128ull * 1024ull * 1024ull;
  }
  return policy;
}

QString CacheChunkName(int index) {
  return QStringLiteral("chunk_%1.bin").arg(index, 8, 10, QLatin1Char('0'));
}

QString CacheIndexPath(const QDir& dir) {
  return dir.filePath(QStringLiteral("cache.idx"));
}

bool ReadCacheIndex(const QString& path, CacheIndex& out, QString& error) {
  out = CacheIndex{};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    error = QStringLiteral("cache index read failed");
    return false;
  }
  QDataStream stream(&file);
  stream.setByteOrder(QDataStream::LittleEndian);
  char magic[sizeof(kAttachmentCacheMagic)] = {};
  if (stream.readRawData(magic, sizeof(magic)) != sizeof(magic)) {
    error = QStringLiteral("cache index read failed");
    return false;
  }
  if (std::memcmp(magic, kAttachmentCacheMagic, sizeof(magic)) != 0) {
    error = QStringLiteral("cache index magic mismatch");
    return false;
  }
  quint8 version = 0;
  stream >> version;
  if (version != kAttachmentCacheVersion) {
    error = QStringLiteral("cache index version mismatch");
    return false;
  }
  stream >> out.flags;
  stream >> out.level;
  stream >> out.passes;
  stream >> out.fileSize;
  stream >> out.chunkBytes;
  stream >> out.chunkCount;
  quint16 nameLen = 0;
  stream >> nameLen;
  if (nameLen > 0) {
    QByteArray name;
    name.resize(nameLen);
    if (stream.readRawData(name.data(), name.size()) != name.size()) {
      error = QStringLiteral("cache index read failed");
      return false;
    }
    out.fileName = QString::fromUtf8(name);
  }
  quint16 rawLen = 0;
  stream >> rawLen;
  if (rawLen > 0) {
    QByteArray raw;
    raw.resize(rawLen);
    if (stream.readRawData(raw.data(), raw.size()) != raw.size()) {
      error = QStringLiteral("cache index read failed");
      return false;
    }
    out.rawName = QString::fromUtf8(raw);
  }
  return true;
}

bool WriteCacheIndex(const QString& path, const CacheIndex& index, QString& error) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    error = QStringLiteral("cache index write failed");
    return false;
  }
  QDataStream stream(&file);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.writeRawData(kAttachmentCacheMagic, sizeof(kAttachmentCacheMagic));
  stream << static_cast<quint8>(kAttachmentCacheVersion);
  stream << index.flags;
  stream << index.level;
  stream << index.passes;
  stream << index.fileSize;
  stream << index.chunkBytes;
  stream << index.chunkCount;
  const QByteArray name = index.fileName.toUtf8();
  stream << static_cast<quint16>(name.size());
  if (!name.isEmpty()) {
    stream.writeRawData(name.constData(), name.size());
  }
  const QByteArray raw = index.rawName.toUtf8();
  stream << static_cast<quint16>(raw.size());
  if (!raw.isEmpty()) {
    stream.writeRawData(raw.constData(), raw.size());
  }
  if (!file.commit()) {
    error = QStringLiteral("cache index write failed");
    return false;
  }
  return true;
}

bool CacheChunksReady(const QDir& dir, const CacheIndex& index) {
  if (index.chunkCount == 0) {
    return index.fileSize == 0;
  }
  for (quint32 i = 0; i < index.chunkCount; ++i) {
    if (!QFileInfo::exists(dir.filePath(CacheChunkName(static_cast<int>(i))))) {
      return false;
    }
  }
  return true;
}

bool WriteChunkFile(const QString& path,
                    const QByteArray& payload,
                    CacheChunkMethod method,
                    int level,
                    quint32 rawSize,
                    QString& error) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    error = QStringLiteral("cache chunk write failed");
    return false;
  }
  QDataStream stream(&file);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.writeRawData(kAttachmentChunkMagic, sizeof(kAttachmentChunkMagic));
  stream << static_cast<quint8>(kAttachmentChunkVersion);
  stream << static_cast<quint8>(method);
  stream << static_cast<quint8>(level);
  stream << static_cast<quint8>(0);
  stream << rawSize;
  stream << static_cast<quint32>(payload.size());
  if (!payload.isEmpty()) {
    if (file.write(payload) != payload.size()) {
      error = QStringLiteral("cache chunk write failed");
      return false;
    }
  }
  if (!file.commit()) {
    error = QStringLiteral("cache chunk write failed");
    return false;
  }
  return true;
}

bool CompressChunk(const QByteArray& input,
                   const CachePolicy& policy,
                   QByteArray& output,
                   CacheChunkMethod& method) {
  if (policy.forceRaw) {
    output = input;
    method = CacheChunkMethod::kRaw;
    return true;
  }
  QByteArray compressed = qCompress(input, policy.level);
  if (policy.passes > 1) {
    compressed = qCompress(compressed, policy.level);
  }
  if (compressed.size() >= input.size()) {
    output = input;
    method = CacheChunkMethod::kRaw;
    return true;
  }
  output = compressed;
  method = policy.passes > 1 ? CacheChunkMethod::kDeflate2
                             : CacheChunkMethod::kDeflate;
  return true;
}

bool BuildChunkedCache(const QString& sourcePath,
                       const CachePolicy& policy,
                       QDir& dir,
                       quint64& outFileSize,
                       quint32& outChunkCount,
                       QString& error) {
  if (policy.chunkBytes == 0) {
    error = QStringLiteral("cache chunk size invalid");
    return false;
  }
  QFile source(sourcePath);
  if (!source.open(QIODevice::ReadOnly)) {
    error = QStringLiteral("cache source open failed");
    return false;
  }
  const quint64 totalSize = static_cast<quint64>(source.size());
  if (totalSize > kMaxAttachmentCacheBytes) {
    error = QStringLiteral("cache file too large");
    return false;
  }
  outFileSize = totalSize;
  outChunkCount = 0;
  quint64 remaining = totalSize;
  while (remaining > 0) {
    const quint64 want = std::min(policy.chunkBytes, remaining);
    const qint64 wantRead = static_cast<qint64>(want);
    QByteArray chunk = source.read(wantRead);
    if (chunk.size() != wantRead) {
      error = QStringLiteral("cache source read failed");
      return false;
    }
    QByteArray payload;
    CacheChunkMethod method = CacheChunkMethod::kRaw;
    if (!CompressChunk(chunk, policy, payload, method)) {
      error = QStringLiteral("cache compress failed");
      return false;
    }
    const QString chunkPath =
        dir.filePath(CacheChunkName(static_cast<int>(outChunkCount)));
    if (!WriteChunkFile(chunkPath,
                        payload,
                        method,
                        policy.level,
                        static_cast<quint32>(chunk.size()),
                        error)) {
      return false;
    }
    remaining -= static_cast<quint64>(chunk.size());
    ++outChunkCount;
  }
  return true;
}

bool EnsureCacheRootDir(QDir& out, QString& error) {
  QString baseDir = UiRuntimePaths::AppRootDir();
  if (baseDir.isEmpty()) {
    baseDir = QCoreApplication::applicationDirPath();
  }
  QDir root(baseDir);
  if (!root.mkpath(QStringLiteral("database/attachments_cache"))) {
    error = QStringLiteral("cache dir failed");
    return false;
  }
  root.cd(QStringLiteral("database/attachments_cache"));
  out = root;
  return true;
}

bool CopyFileToPath(const QString& src,
                    const QString& dest,
                    QString& error) {
  if (src.isEmpty() || dest.isEmpty()) {
    error = QStringLiteral("cache copy failed");
    return false;
  }
  if (QFileInfo(src).absoluteFilePath() == QFileInfo(dest).absoluteFilePath()) {
    return true;
  }
  if (QFileInfo::exists(dest)) {
    QFile::remove(dest);
  }
  if (!QFile::copy(src, dest)) {
    error = QStringLiteral("cache copy failed");
    return false;
  }
  return true;
}

bool ReadChunkFile(const QString& path,
                   CacheChunkMethod& method,
                   quint32& rawSize,
                   QByteArray& payload,
                   QString& error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    error = QStringLiteral("cache chunk read failed");
    return false;
  }
  QDataStream stream(&file);
  stream.setByteOrder(QDataStream::LittleEndian);
  char magic[sizeof(kAttachmentChunkMagic)] = {};
  if (stream.readRawData(magic, sizeof(magic)) != sizeof(magic)) {
    error = QStringLiteral("cache chunk read failed");
    return false;
  }
  if (std::memcmp(magic, kAttachmentChunkMagic, sizeof(magic)) != 0) {
    error = QStringLiteral("cache chunk invalid");
    return false;
  }
  quint8 version = 0;
  quint8 methodByte = 0;
  quint8 level = 0;
  quint8 reserved = 0;
  quint32 payloadSize = 0;
  stream >> version;
  stream >> methodByte;
  stream >> level;
  stream >> reserved;
  stream >> rawSize;
  stream >> payloadSize;
  (void)level;
  (void)reserved;
  if (version != kAttachmentChunkVersion) {
    error = QStringLiteral("cache chunk invalid");
    return false;
  }
  if (payloadSize == 0 && rawSize == 0) {
    payload.clear();
    method = static_cast<CacheChunkMethod>(methodByte);
    return true;
  }
  if (payloadSize >
      static_cast<quint32>(file.size() - file.pos())) {
    error = QStringLiteral("cache chunk invalid");
    return false;
  }
  payload = file.read(static_cast<qint64>(payloadSize));
  if (payload.size() != static_cast<int>(payloadSize)) {
    error = QStringLiteral("cache chunk read failed");
    return false;
  }
  method = static_cast<CacheChunkMethod>(methodByte);
  return true;
}

bool DecompressChunk(CacheChunkMethod method,
                     const QByteArray& payload,
                     quint32 rawSize,
                     QByteArray& out,
                     QString& error) {
  out.clear();
  if (rawSize == 0) {
    return true;
  }
  if (method == CacheChunkMethod::kRaw) {
    out = payload;
  } else if (method == CacheChunkMethod::kDeflate) {
    out = qUncompress(payload);
  } else if (method == CacheChunkMethod::kDeflate2) {
    const QByteArray stage1 = qUncompress(payload);
    out = qUncompress(stage1);
  } else {
    error = QStringLiteral("cache chunk invalid");
    return false;
  }
  if (out.size() != static_cast<int>(rawSize)) {
    error = QStringLiteral("cache chunk invalid");
    return false;
  }
  return true;
}

bool RestoreChunkedCache(const QDir& dir,
                         const CacheIndex& index,
                         const QString& destPath,
                         const std::function<void(double)>& onProgress,
                         QString& error) {
  QFile out(destPath);
  if (QFileInfo::exists(destPath)) {
    QFile::remove(destPath);
  }
  if (!out.open(QIODevice::WriteOnly)) {
    error = QStringLiteral("cache restore failed");
    return false;
  }
  if (index.chunkCount == 0) {
    out.close();
    if (onProgress) {
      onProgress(1.0);
    }
    return true;
  }
  for (quint32 i = 0; i < index.chunkCount; ++i) {
    const QString chunkPath = dir.filePath(CacheChunkName(static_cast<int>(i)));
    CacheChunkMethod method = CacheChunkMethod::kRaw;
    quint32 rawSize = 0;
    QByteArray payload;
    if (!ReadChunkFile(chunkPath, method, rawSize, payload, error)) {
      out.close();
      QFile::remove(destPath);
      return false;
    }
    QByteArray plain;
    if (!DecompressChunk(method, payload, rawSize, plain, error)) {
      out.close();
      QFile::remove(destPath);
      return false;
    }
    if (!plain.isEmpty()) {
      if (out.write(plain) != plain.size()) {
        error = QStringLiteral("cache restore failed");
        out.close();
        QFile::remove(destPath);
        return false;
      }
    }
    if (onProgress) {
      onProgress(static_cast<double>(i + 1) /
                 static_cast<double>(index.chunkCount));
    }
  }
  out.close();
  if (index.fileSize > 0 &&
      static_cast<quint64>(QFileInfo(destPath).size()) != index.fileSize) {
    error = QStringLiteral("cache restore failed");
    QFile::remove(destPath);
    return false;
  }
  return true;
}

struct CacheTaskResult {
  bool ok{false};
  QString fileUrl;
  QString previewUrl;
  QString error;
};

QString FindFfmpegPath();

CacheTaskResult BuildAttachmentCache(
    ClientCore& core,
    const QString& fileId,
    const std::array<std::uint8_t, 32>& fileKey,
    const QString& fileName,
    qint64 fileSize,
    const std::function<void(double)>& onProgress) {
  CacheTaskResult result;
  QDir cacheRoot;
  QString error;
  if (!EnsureCacheRootDir(cacheRoot, error)) {
    result.error = error;
    return result;
  }

  const QString safeId = SanitizeFileId(fileId);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = IsImageExt(ext) || IsGifExt(ext) || IsVideoExt(ext);

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    const QString previewPath =
        cacheRoot.filePath(safeId + QStringLiteral(".preview.jpg"));
    if (!QFileInfo::exists(filePath)) {
      ClientCore::ChatFileMessage file;
      file.file_id = fileId.toStdString();
      file.file_key = fileKey;
      file.file_name = fileName.toStdString();
      if (fileSize > 0) {
        file.file_size = static_cast<std::uint64_t>(fileSize);
      }
      if (!core.DownloadChatFileToPath(
              file, filePath.toStdString(), true,
              [onProgress](std::uint64_t done, std::uint64_t total) {
                if (!onProgress || total == 0) {
                  return;
                }
                onProgress(static_cast<double>(done) /
                           static_cast<double>(total));
              })) {
        result.error = QString::fromStdString(core.last_error());
        return result;
      }
    }
    result.fileUrl = filePath;
    if (IsVideoExt(ext)) {
      if (!QFileInfo::exists(previewPath)) {
        const QString ffmpeg = FindFfmpegPath();
        if (!ffmpeg.isEmpty()) {
          QStringList args;
          args << QStringLiteral("-y")
               << QStringLiteral("-ss") << QStringLiteral("0.2")
               << QStringLiteral("-i") << filePath
               << QStringLiteral("-frames:v") << QStringLiteral("1")
               << QStringLiteral("-vf") << QStringLiteral("scale=480:-1")
               << previewPath;
          QProcess::execute(ffmpeg, args);
        }
      }
      if (QFileInfo::exists(previewPath)) {
        result.previewUrl = previewPath;
      }
    } else {
      result.previewUrl = filePath;
    }
    result.ok = true;
    return result;
  }

  QDir fileDir(cacheRoot.filePath(safeId));
  if (!fileDir.exists()) {
    if (!cacheRoot.mkpath(safeId)) {
      result.error = QStringLiteral("cache dir failed");
      return result;
    }
  }
  fileDir.setPath(cacheRoot.filePath(safeId));

  const QString indexPath = CacheIndexPath(fileDir);
  CacheIndex existing;
  if (QFileInfo::exists(indexPath)) {
    if (ReadCacheIndex(indexPath, existing, error) &&
        CacheChunksReady(fileDir, existing)) {
      if ((existing.flags & kCacheFlagKeepRaw) && !existing.rawName.isEmpty()) {
        const QString rawPath = fileDir.filePath(existing.rawName);
        if (QFileInfo::exists(rawPath)) {
          result.fileUrl = rawPath;
        }
      }
      result.ok = true;
      return result;
    }
  }

  const QFileInfoList oldFiles =
      fileDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  for (const auto& entry : oldFiles) {
    QFile::remove(entry.absoluteFilePath());
  }

  const QString tempPath = fileDir.filePath(QStringLiteral("download.tmp"));
  if (QFileInfo::exists(tempPath)) {
    QFile::remove(tempPath);
  }

  ClientCore::ChatFileMessage file;
  file.file_id = fileId.toStdString();
  file.file_key = fileKey;
  file.file_name = fileName.toStdString();
  if (fileSize > 0) {
    file.file_size = static_cast<std::uint64_t>(fileSize);
  }
  if (!core.DownloadChatFileToPath(
          file, tempPath.toStdString(), true,
          [onProgress](std::uint64_t done, std::uint64_t total) {
            if (!onProgress || total == 0) {
              return;
            }
            onProgress(static_cast<double>(done) /
                       static_cast<double>(total));
          })) {
    result.error = QString::fromStdString(core.last_error());
    return result;
  }

  const quint64 tempSize =
      static_cast<quint64>(QFileInfo(tempPath).size());
  if (tempSize > kMaxAttachmentCacheBytes) {
    QFile::remove(tempPath);
    result.error = QStringLiteral("file too large");
    return result;
  }

  quint64 actualSize = 0;
  quint32 chunkCount = 0;
  CachePolicy policy = SelectCachePolicy(tempSize);
  if (IsAlreadyCompressedExt(ext)) {
    policy.forceRaw = true;
  }
  if (!BuildChunkedCache(tempPath, policy, fileDir, actualSize, chunkCount,
                         error)) {
    QFile::remove(tempPath);
    result.error = error;
    return result;
  }

  QString rawName;
  if (policy.keepRaw && actualSize > 0) {
    rawName = QStringLiteral("raw.") + ext;
    const QString rawPath = fileDir.filePath(rawName);
    if (QFileInfo::exists(rawPath)) {
      QFile::remove(rawPath);
    }
    QFile::rename(tempPath, rawPath);
    result.fileUrl = rawPath;
  } else {
    QFile::remove(tempPath);
  }

  CacheIndex index;
  index.fileSize = actualSize;
  index.chunkBytes = policy.chunkBytes;
  index.chunkCount = chunkCount;
  index.level = static_cast<quint8>(policy.level);
  index.passes = static_cast<quint8>(policy.passes);
  index.fileName = fileName;
  index.rawName = rawName;
  if (policy.keepRaw) {
    index.flags |= kCacheFlagKeepRaw;
  }
  if (policy.forceRaw) {
    index.flags |= kCacheFlagForceRaw;
  }
  if (!WriteCacheIndex(indexPath, index, error)) {
    result.error = error;
    return result;
  }

  result.ok = true;
  return result;
}

bool RestoreAttachmentFromCache(const QString& fileId,
                                const QString& fileName,
                                const QString& savePath,
                                const std::function<void(double)>& onProgress,
                                QString& error) {
  QDir cacheRoot;
  if (!EnsureCacheRootDir(cacheRoot, error)) {
    return false;
  }
  const QString safeId = SanitizeFileId(fileId);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = IsImageExt(ext) || IsGifExt(ext) || IsVideoExt(ext);
  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    if (!QFileInfo::exists(filePath)) {
      error = QStringLiteral("cache missing");
      return false;
    }
    if (!CopyFileToPath(filePath, savePath, error)) {
      return false;
    }
    if (onProgress) {
      onProgress(1.0);
    }
    return true;
  }

  const QDir fileDir(cacheRoot.filePath(safeId));
  const QString indexPath = CacheIndexPath(fileDir);
  CacheIndex index;
  if (!QFileInfo::exists(indexPath) ||
      !ReadCacheIndex(indexPath, index, error)) {
    error = QStringLiteral("cache missing");
    return false;
  }
  if ((index.flags & kCacheFlagKeepRaw) && !index.rawName.isEmpty()) {
    const QString rawPath = fileDir.filePath(index.rawName);
    if (QFileInfo::exists(rawPath)) {
      if (!CopyFileToPath(rawPath, savePath, error)) {
        return false;
      }
      if (onProgress) {
        onProgress(1.0);
      }
      return true;
    }
  }
  if (!CacheChunksReady(fileDir, index)) {
    error = QStringLiteral("cache missing");
    return false;
  }
  return RestoreChunkedCache(fileDir, index, savePath, onProgress, error);
}

QString FindFfmpegPath() {
  QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (!ffmpeg.isEmpty()) {
    return ffmpeg;
  }
  QString baseDir = UiRuntimePaths::AppRootDir();
  if (baseDir.isEmpty()) {
    baseDir = QCoreApplication::applicationDirPath();
  }
  const QString local = QDir(baseDir).filePath(QStringLiteral("ffmpeg.exe"));
  if (QFileInfo::exists(local)) {
    return local;
  }
  const QString runtimeDir = UiRuntimePaths::RuntimeDir();
  if (!runtimeDir.isEmpty()) {
    const QString runtime = QDir(runtimeDir).filePath(QStringLiteral("ffmpeg.exe"));
    if (QFileInfo::exists(runtime)) {
      return runtime;
    }
  }
  return {};
}

class Nv12VideoBuffer final : public QAbstractVideoBuffer {
 public:
  Nv12VideoBuffer(std::vector<std::uint8_t>&& data,
                  std::uint32_t width,
                  std::uint32_t height,
                  std::uint32_t stride)
      : format_(QSize(static_cast<int>(width), static_cast<int>(height)),
                QVideoFrameFormat::Format_NV12),
        data_(std::move(data)),
        stride_(static_cast<int>(stride)),
        height_(static_cast<int>(height)) {}

  MapData map(QVideoFrame::MapMode) override {
    MapData out;
    if (data_.empty() || stride_ <= 0 || height_ <= 0) {
      return out;
    }
    const std::size_t y_bytes =
        static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_);
    if (data_.size() < y_bytes) {
      return out;
    }
    out.planeCount = 2;
    out.bytesPerLine[0] = stride_;
    out.bytesPerLine[1] = stride_;
    out.data[0] = data_.data();
    out.data[1] = data_.data() + y_bytes;
    out.dataSize[0] = static_cast<int>(y_bytes);
    out.dataSize[1] = static_cast<int>(data_.size() - y_bytes);
    return out;
  }

  void unmap() override {}

  QVideoFrameFormat format() const override { return format_; }

 private:
  QVideoFrameFormat format_;
  std::vector<std::uint8_t> data_;
  int stride_{0};
  int height_{0};
};

}  // namespace

QuickClient::QuickClient(QObject* parent) : QObject(parent) {
  poll_timer_.setInterval(500);
  poll_timer_.setTimerType(Qt::CoarseTimer);
  connect(&poll_timer_, &QTimer::timeout, this, &QuickClient::PollOnce);
  media_timer_.setInterval(20);
  media_timer_.setTimerType(Qt::PreciseTimer);
  connect(&media_timer_, &QTimer::timeout, this, &QuickClient::PumpMedia);
  local_video_sink_ = new QVideoSink(this);
  remote_video_sink_ = new QVideoSink(this);
  int ideal = QThread::idealThreadCount();
  if (ideal < 1) {
    ideal = 2;
  }
  cache_pool_.setMaxThreadCount(std::clamp(ideal, 4, 12));
  cache_pool_.setExpiryTimeout(30000);
  if (auto* clipboard = QGuiApplication::clipboard()) {
    last_system_clipboard_text_ = clipboard->text();
    last_system_clipboard_ms_ = QDateTime::currentMSecsSinceEpoch();
    connect(clipboard, &QClipboard::dataChanged, this, [this]() {
      if (auto* cb = QGuiApplication::clipboard()) {
        last_system_clipboard_text_ = cb->text();
        last_system_clipboard_ms_ = QDateTime::currentMSecsSinceEpoch();
      }
    });
  }
}

QuickClient::~QuickClient() {
  cache_pool_.clear();
  cache_pool_.waitForDone();
  if (ime_session_) {
    ImePluginLoader::instance().destroySession(ime_session_);
    ime_session_ = nullptr;
  }
  StopMedia();
  StopPolling();
  core_.Logout();
}

bool QuickClient::init(const QString& configPath) {
  const QString appRoot = UiRuntimePaths::AppRootDir();
  const QString baseDir =
      appRoot.isEmpty() ? QCoreApplication::applicationDirPath() : appRoot;
  const QString dataDir = QDir(baseDir).filePath(QStringLiteral("database"));
  QDir().mkpath(dataDir);
  qputenv("MI_E2EE_DATA_DIR",
          QDir::toNativeSeparators(dataDir).toUtf8());
  ai_gpu_name_ = QueryGpuName();
  ai_gpu_series_ = ParseNvidiaSeries(ai_gpu_name_);
  ai_gpu_available_ = DetectAiEnhanceGpuAvailable();
  const AiEnhanceRecommendation rec =
      BuildAiEnhanceRecommendation(ai_gpu_series_, ai_gpu_available_);
  ai_rec_perf_scale_ = rec.perf_scale;
  ai_rec_quality_scale_ = rec.quality_scale;
  bool enabled = ai_enhance_enabled_;
  int quality = ai_rec_perf_scale_;
  bool x4Confirmed = ai_enhance_x4_confirmed_;
  LoadAiEnhanceSettings(ai_gpu_available_, rec, enabled, quality, x4Confirmed);
  ai_enhance_enabled_ = enabled;
  ai_enhance_quality_ = quality;
  ai_enhance_x4_confirmed_ = x4Confirmed;
  if (!configPath.isEmpty()) {
    config_path_ = configPath;
  } else {
    config_path_ = FindConfigFile(QStringLiteral("config/client_config.ini"));
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("client_config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = FindConfigFile(QStringLiteral("config.ini"));
    }
    if (config_path_.isEmpty()) {
      config_path_ = baseDir + QStringLiteral("/config/client_config.ini");
    }
  }
  const bool ok = core_.Init(config_path_.toStdString());
  if (!ok) {
    UpdateLastError(QStringLiteral("初始化失败"));
    emit status(QStringLiteral("初始化失败"));
  } else {
    UpdateLastError(QString());
    emit deviceChanged();
  }
  return ok;
}

bool QuickClient::registerUser(const QString& user, const QString& pass) {
  const QString account = user.trimmed();
  if (account.isEmpty() || pass.isEmpty()) {
    UpdateLastError(QStringLiteral("账号或密码为空"));
    emit status(QStringLiteral("注册失败"));
    return false;
  }
  const bool ok = core_.Register(account.toStdString(), pass.toStdString());
  if (!ok) {
    const QString err = QString::fromStdString(core_.last_error());
    UpdateLastError(err.isEmpty() ? QStringLiteral("注册失败") : err);
    emit status(QStringLiteral("注册失败"));
  } else {
    UpdateLastError(QString());
    emit status(QStringLiteral("注册成功"));
  }
  MaybeEmitTrustSignals();
  return ok;
}

bool QuickClient::login(const QString& user, const QString& pass) {
  const bool ok = core_.Login(user.toStdString(), pass.toStdString());
  if (!ok) {
    emit status(QStringLiteral("登录失败"));
    token_.clear();
    username_.clear();
    UpdateLastError(QString::fromStdString(core_.last_error()));
    StopPolling();
  } else {
    token_ = QString::fromStdString(core_.token());
    username_ = user.trimmed();
    emit status(QStringLiteral("登录成功"));
    UpdateLastError(QString());
    StartPolling();
    UpdateFriendList(core_.ListFriends());
    UpdateFriendRequests(core_.ListFriendRequests());
    emit deviceChanged();
  }
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();
  emit tokenChanged();
  emit userChanged();
  return ok;
}

void QuickClient::logout() {
  StopPolling();
  StopMedia();
  core_.Logout();
  token_.clear();
  username_.clear();
  UpdateLastError(QString());
  friends_.clear();
  groups_.clear();
  friend_requests_.clear();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();
  emit tokenChanged();
  emit userChanged();
  emit friendsChanged();
  emit groupsChanged();
  emit friendRequestsChanged();
  emit callStateChanged();
  emit status(QStringLiteral("已登出"));
}

bool QuickClient::joinGroup(const QString& groupId) {
  const QString trimmed = groupId.trimmed();
  const bool ok = core_.JoinGroup(trimmed.toStdString());
  if (ok) {
    if (AddGroupIfMissing(trimmed)) {
      emit groupsChanged();
    }
    UpdateLastError(QString());
  } else {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  }
  emit status(ok ? QStringLiteral("加入群成功") : QStringLiteral("加入群失败"));
  return ok;
}

QString QuickClient::createGroup() {
  std::string out_id;
  if (!core_.CreateGroup(out_id)) {
    emit status(QStringLiteral("创建群失败"));
    UpdateLastError(QString::fromStdString(core_.last_error()));
    return {};
  }
  const QString group_id = QString::fromStdString(out_id);
  if (AddGroupIfMissing(group_id)) {
    emit groupsChanged();
  }
  emit status(QStringLiteral("已创建群"));
  UpdateLastError(QString());
  return group_id;
}

bool QuickClient::sendGroupInvite(const QString& groupId,
                                  const QString& peerUsername) {
  const QString gid = groupId.trimmed();
  const QString peer = peerUsername.trimmed();
  if (gid.isEmpty() || peer.isEmpty()) {
    UpdateLastError(QStringLiteral("群或成员为空"));
    return false;
  }
  std::string msg_id;
  const bool ok =
      core_.SendGroupInvite(gid.toStdString(), peer.toStdString(), msg_id);
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
    emit status(QStringLiteral("邀请失败"));
    return false;
  }
  UpdateLastError(QString());
  emit status(QStringLiteral("邀请已发送"));
  return true;
}

bool QuickClient::sendText(const QString& convId, const QString& text, bool isGroup) {
  const QString trimmed = convId.trimmed();
  const QString message = text.trimmed();
  if (trimmed.isEmpty() || message.isEmpty()) {
    return false;
  }
  std::string msg_id;
  bool ok = false;
  if (isGroup) {
    ok = core_.SendGroupChatText(trimmed.toStdString(), message.toStdString(),
                                 msg_id);
  } else {
    ok = core_.SendChatText(trimmed.toStdString(), message.toStdString(), msg_id);
  }
  if (!ok) {
    emit status(QStringLiteral("发送失败"));
    UpdateLastError(QString::fromStdString(core_.last_error()));
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
  msg.insert(QStringLiteral("text"), message);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendFile(const QString& convId, const QString& path, bool isGroup) {
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty() || path.trimmed().isEmpty()) {
    return false;
  }
  QString resolved = path;
  if (resolved.startsWith(QStringLiteral("file:"))) {
    resolved = QUrl(resolved).toLocalFile();
  }
  QFileInfo info(resolved);
  if (!info.exists() || !info.isFile()) {
    emit status(QStringLiteral("文件不存在"));
    return false;
  }
  std::string msg_id;
  bool ok = false;
  if (isGroup) {
    ok = core_.SendGroupChatFile(
        trimmed.toStdString(), ToFsPath(info.absoluteFilePath()), msg_id);
  } else {
    ok = core_.SendChatFile(
        trimmed.toStdString(), ToFsPath(info.absoluteFilePath()), msg_id);
  }
  if (!ok) {
    emit status(QStringLiteral("文件发送失败"));
    UpdateLastError(QString::fromStdString(core_.last_error()));
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), isGroup);
  msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
  msg.insert(QStringLiteral("fileName"), info.fileName());
  msg.insert(QStringLiteral("fileSize"), info.size());
  msg.insert(QStringLiteral("filePath"), info.absoluteFilePath());
  msg.insert(QStringLiteral("fileUrl"),
             QUrl::fromLocalFile(info.absoluteFilePath()).toString());
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  MaybeAutoEnhanceImage(QString::fromStdString(msg_id),
                        info.absoluteFilePath(),
                        info.fileName());
  return true;
}

bool QuickClient::sendSticker(const QString& convId,
                              const QString& stickerId,
                              bool isGroup) {
  const QString trimmed = convId.trimmed();
  const QString sid = stickerId.trimmed();
  if (trimmed.isEmpty() || sid.isEmpty()) {
    return false;
  }
  if (isGroup) {
    emit status(QStringLiteral("群聊暂不支持贴纸"));
    return false;
  }

  std::string msg_id;
  const bool ok =
      core_.SendChatSticker(trimmed.toStdString(), sid.toStdString(), msg_id);
  if (!ok) {
    emit status(QStringLiteral("贴纸发送失败"));
    UpdateLastError(QString::fromStdString(core_.last_error()));
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
  msg.insert(QStringLiteral("stickerId"), sid);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  const auto meta = BuildStickerMeta(sid);
  msg.insert(QStringLiteral("stickerUrl"), meta.value(QStringLiteral("stickerUrl")));
  msg.insert(QStringLiteral("stickerAnimated"), meta.value(QStringLiteral("stickerAnimated")));
  EmitMessage(msg);
  return true;
}

bool QuickClient::sendLocation(const QString& convId,
                               double lat,
                               double lon,
                               const QString& label,
                               bool isGroup) {
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  if (isGroup) {
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
      emit status(QStringLiteral("位置参数无效"));
      UpdateLastError(QStringLiteral("位置参数无效"));
      return false;
    }
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
      emit status(QStringLiteral("位置超出范围"));
      UpdateLastError(QStringLiteral("位置超出范围"));
      return false;
    }
    const QString text = FormatLocationText(lat, lon, label);
    std::string msg_id;
    const bool ok = core_.SendGroupChatText(trimmed.toStdString(),
                                            text.toStdString(), msg_id);
    if (!ok) {
      emit status(QStringLiteral("位置发送失败"));
      UpdateLastError(QString::fromStdString(core_.last_error()));
      return false;
    }
    UpdateLastError(QString());
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), trimmed);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("location"));
    msg.insert(QStringLiteral("locationLabel"), label);
    msg.insert(QStringLiteral("locationLat"), lat);
    msg.insert(QStringLiteral("locationLon"), lon);
    msg.insert(QStringLiteral("text"), text);
    msg.insert(QStringLiteral("time"), NowTimeString());
    msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
    EmitMessage(msg);
    return true;
  }
  if (!std::isfinite(lat) || !std::isfinite(lon)) {
    emit status(QStringLiteral("位置参数无效"));
    UpdateLastError(QStringLiteral("位置参数无效"));
    return false;
  }
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    emit status(QStringLiteral("位置超出范围"));
    UpdateLastError(QStringLiteral("位置超出范围"));
    return false;
  }
  const auto lat_e7 = static_cast<std::int32_t>(std::llround(lat * 10000000.0));
  const auto lon_e7 = static_cast<std::int32_t>(std::llround(lon * 10000000.0));
  std::string msg_id;
  const bool ok = core_.SendChatLocation(trimmed.toStdString(), lat_e7, lon_e7,
                                         label.toStdString(), msg_id);
  if (!ok) {
    emit status(QStringLiteral("位置发送失败"));
    UpdateLastError(QString::fromStdString(core_.last_error()));
    return false;
  }

  UpdateLastError(QString());
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), trimmed);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("location"));
  msg.insert(QStringLiteral("locationLabel"), label);
  msg.insert(QStringLiteral("locationLat"), lat);
  msg.insert(QStringLiteral("locationLon"), lon);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  return true;
}

QVariantMap QuickClient::ensureAttachmentCached(const QString& fileId,
                                                const QString& fileKeyHex,
                                                const QString& fileName,
                                                qint64 fileSize) {
  QVariantMap out;
  out.insert(QStringLiteral("ok"), false);
  const QString fid = fileId.trimmed();
  if (fid.isEmpty()) {
    out.insert(QStringLiteral("error"), QStringLiteral("file id empty"));
    return out;
  }
  std::array<std::uint8_t, 32> file_key{};
  if (!HexToBytes32(fileKeyHex.trimmed(), file_key)) {
    out.insert(QStringLiteral("error"), QStringLiteral("invalid file key"));
    return out;
  }
  if (fileSize > 0 &&
      static_cast<quint64>(fileSize) > kMaxAttachmentCacheBytes) {
    out.insert(QStringLiteral("error"), QStringLiteral("file too large"));
    return out;
  }

  QDir cacheRoot;
  QString rootErr;
  if (!EnsureCacheRootDir(cacheRoot, rootErr)) {
    out.insert(QStringLiteral("error"), rootErr);
    return out;
  }

  const QString safeId = SanitizeFileId(fid);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = IsImageExt(ext) || IsGifExt(ext) || IsVideoExt(ext);

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    const QString previewPath =
        cacheRoot.filePath(safeId + QStringLiteral(".preview.jpg"));
    if (QFileInfo::exists(filePath)) {
      out.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(filePath));
      if (IsVideoExt(ext)) {
        if (QFileInfo::exists(previewPath)) {
          out.insert(QStringLiteral("previewUrl"),
                     QUrl::fromLocalFile(previewPath));
        } else if (!cache_inflight_.contains(fid)) {
          cache_inflight_.insert(fid);
          QueueAttachmentCacheTask(fid, file_key, fileName, fileSize, false);
        }
      } else {
        out.insert(QStringLiteral("previewUrl"), QUrl::fromLocalFile(filePath));
      }
      out.insert(QStringLiteral("ok"), true);
      return out;
    }
  } else {
    QDir fileDir(cacheRoot.filePath(safeId));
    const QString indexPath = CacheIndexPath(fileDir);
    CacheIndex existing;
    QString readErr;
    if (QFileInfo::exists(indexPath) &&
        ReadCacheIndex(indexPath, existing, readErr) &&
        CacheChunksReady(fileDir, existing)) {
      if ((existing.flags & kCacheFlagKeepRaw) && !existing.rawName.isEmpty()) {
        const QString rawPath = fileDir.filePath(existing.rawName);
        if (QFileInfo::exists(rawPath)) {
          out.insert(QStringLiteral("fileUrl"), QUrl::fromLocalFile(rawPath));
        }
      }
      out.insert(QStringLiteral("ok"), true);
      return out;
    }
  }

  if (!cache_inflight_.contains(fid)) {
    cache_inflight_.insert(fid);
    QueueAttachmentCacheTask(fid, file_key, fileName, fileSize, false);
  }
  out.insert(QStringLiteral("pending"), true);
  return out;
}

bool QuickClient::requestAttachmentDownload(const QString& fileId,
                                            const QString& fileKeyHex,
                                            const QString& fileName,
                                            qint64 fileSize,
                                            const QString& savePath) {
  const QString fid = fileId.trimmed();
  if (fid.isEmpty()) {
    UpdateLastError(QStringLiteral("file id empty"));
    return false;
  }
  std::array<std::uint8_t, 32> file_key{};
  if (!HexToBytes32(fileKeyHex.trimmed(), file_key)) {
    UpdateLastError(QStringLiteral("invalid file key"));
    return false;
  }
  QString resolved = savePath.trimmed();
  if (resolved.startsWith(QStringLiteral("file:"))) {
    resolved = QUrl(resolved).toLocalFile();
  }
  if (resolved.isEmpty()) {
    UpdateLastError(QStringLiteral("save path empty"));
    return false;
  }
  QFileInfo destInfo(resolved);
  if (destInfo.isDir() || resolved.endsWith(QLatin1Char('/')) ||
      resolved.endsWith(QLatin1Char('\\'))) {
    const QString fallbackName =
        fileName.trimmed().isEmpty()
            ? (SanitizeFileId(fid) + QStringLiteral(".bin"))
            : fileName.trimmed();
    resolved = QDir(resolved).filePath(fallbackName);
    destInfo = QFileInfo(resolved);
  }
  QDir destDir = destInfo.dir();
  if (!destDir.exists() && !destDir.mkpath(QStringLiteral("."))) {
    UpdateLastError(QStringLiteral("save path invalid"));
    return false;
  }
  if (fileSize > 0 &&
      static_cast<quint64>(fileSize) > kMaxAttachmentCacheBytes) {
    UpdateLastError(QStringLiteral("file too large"));
    return false;
  }

  const QString safeId = SanitizeFileId(fid);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  QString effectiveName = fileName.trimmed();
  if (effectiveName.isEmpty()) {
    effectiveName = safeId + QStringLiteral(".") + ext;
  }

  QDir cacheRoot;
  QString rootErr;
  if (!EnsureCacheRootDir(cacheRoot, rootErr)) {
    UpdateLastError(rootErr);
    return false;
  }
  const bool isMedia = IsImageExt(ext) || IsGifExt(ext) || IsVideoExt(ext);
  bool cacheReady = false;

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    if (QFileInfo::exists(filePath)) {
      cacheReady = true;
    }
  } else {
    const QDir fileDir(cacheRoot.filePath(safeId));
    const QString indexPath = CacheIndexPath(fileDir);
    CacheIndex existing;
    QString readErr;
    if (QFileInfo::exists(indexPath) &&
        ReadCacheIndex(indexPath, existing, readErr) &&
        CacheChunksReady(fileDir, existing)) {
      cacheReady = true;
    }
  }

  download_progress_base_.insert(fid, 0.0);
  download_progress_span_.insert(fid, cacheReady ? 1.0 : 0.9);
  EmitDownloadProgress(fid, resolved, 0.0);

  if (cacheReady) {
    QueueAttachmentRestoreTask(fid, effectiveName, resolved, true);
    return true;
  }

  pending_downloads_[fid].append(resolved);
  if (!pending_download_names_.contains(fid) ||
      pending_download_names_.value(fid).isEmpty()) {
    pending_download_names_.insert(fid, effectiveName);
  }
  if (!cache_inflight_.contains(fid)) {
    cache_inflight_.insert(fid);
    QueueAttachmentCacheTask(fid, file_key, effectiveName, fileSize, true);
  }
  return true;
}

bool QuickClient::requestImageEnhance(const QString& fileUrl,
                                      const QString& fileName) {
  return requestImageEnhanceForMessage(QString(), fileUrl, fileName);
}

bool QuickClient::requestImageEnhanceForMessage(const QString& messageId,
                                                const QString& fileUrl,
                                                const QString& fileName) {
  if (!ai_enhance_enabled_) {
    UpdateLastError(QStringLiteral("AI超清已关闭"));
    return false;
  }
  const QString sourceUrl = fileUrl.trimmed();
  const QString sourcePath = ResolveLocalFilePath(sourceUrl);
  if (sourcePath.isEmpty()) {
    UpdateLastError(QStringLiteral("图片路径为空"));
    return false;
  }
  const QFileInfo sourceInfo(sourcePath);
  if (!sourceInfo.exists() || !sourceInfo.isFile()) {
    UpdateLastError(QStringLiteral("图片不存在"));
    return false;
  }
  if (!IsImageExt(sourceInfo.suffix())) {
    UpdateLastError(QStringLiteral("仅支持图片优化"));
    return false;
  }

  const QString trimmedMsg = messageId.trimmed();
  const QString inflightKey = trimmedMsg.isEmpty() ? sourcePath : trimmedMsg;
  if (!inflightKey.isEmpty() && enhance_inflight_.contains(inflightKey)) {
    return true;
  }
  if (!trimmedMsg.isEmpty()) {
    const QString existing = EnhancedImagePathIfExists(trimmedMsg);
    if (!existing.isEmpty()) {
      const QString outputUrl = QUrl::fromLocalFile(existing).toString();
      emit imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, true,
                                QString());
      UpdateLastError(QString());
      return true;
    }
  }

  QString outPath;
  QString pathError;
  const int scale =
      ResolveEnhanceScale(ai_enhance_quality_, ai_enhance_x4_confirmed_);
  if (!trimmedMsg.isEmpty()) {
    outPath = BuildEnhancedImagePath(trimmedMsg, scale, pathError);
  } else {
    QDir outDir;
    if (!EnsureAiUpscaleDir(outDir, pathError)) {
      UpdateLastError(pathError);
      return false;
    }
    const QString stem = SanitizeFileStem(
        fileName.trimmed().isEmpty() ? sourceInfo.fileName() : fileName);
    const QString suffix = QStringLiteral("_x%1").arg(scale);
    outPath = outDir.filePath(stem + suffix + QStringLiteral(".png"));
    if (QFileInfo::exists(outPath)) {
      int suffix = 2;
      while (suffix < 1000) {
        const QString candidate =
            outDir.filePath(stem + QStringLiteral("_x%1_").arg(scale) +
                            QString::number(suffix) +
                            QStringLiteral(".png"));
        if (!QFileInfo::exists(candidate)) {
          outPath = candidate;
          break;
        }
        ++suffix;
      }
    }
  }
  if (outPath.isEmpty()) {
    UpdateLastError(pathError.isEmpty() ? QStringLiteral("创建超清目录失败")
                                        : pathError);
    return false;
  }

  if (QFileInfo::exists(outPath)) {
    const QString outputUrl = QUrl::fromLocalFile(outPath).toString();
    emit imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, true, QString());
    UpdateLastError(QString());
    return true;
  }

  if (!inflightKey.isEmpty()) {
    enhance_inflight_.insert(inflightKey);
  }

  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, trimmedMsg, inflightKey, sourceUrl,
                              sourcePath, outPath, scale]() {
    if (!self) {
      return;
    }
    bool gpuSupported = false;
    const QString exe = FindRealEsrganPath(&gpuSupported);
    const ImageQualityMetrics metrics = AnalyzeImageQuality(sourcePath);
    QString modelName = SelectRealEsrganModelName(scale, metrics.anime_like);
    QString modelDir = FindRealEsrganModelDir(exe, modelName);
    if (modelDir.isEmpty() && metrics.anime_like) {
      modelName = SelectRealEsrganModelName(scale, false);
      modelDir = FindRealEsrganModelDir(exe, modelName);
    }
    QString error;
    bool ok = false;
    QString outputUrl;

    if (exe.isEmpty()) {
      error = QStringLiteral("未找到超清工具");
    } else if (modelDir.isEmpty()) {
      error = QStringLiteral("未找到超清模型");
    } else {
      QStringList args;
      args << QStringLiteral("-i") << sourcePath
           << QStringLiteral("-o") << outPath
           << QStringLiteral("-n") << modelName
           << QStringLiteral("-s") << QString::number(scale)
           << QStringLiteral("-m") << modelDir;
      int exitCode = -1;
      if (gpuSupported) {
        QStringList gpuArgs = args;
        gpuArgs << QStringLiteral("-g") << QStringLiteral("0");
        exitCode = QProcess::execute(exe, gpuArgs);
        if (exitCode != 0) {
          QStringList cpuArgs = args;
          cpuArgs << QStringLiteral("-g") << QStringLiteral("-1");
          exitCode = QProcess::execute(exe, cpuArgs);
        }
      } else {
        exitCode = QProcess::execute(exe, args);
      }

      if (exitCode == 0 && QFileInfo::exists(outPath)) {
        ok = true;
        outputUrl = QUrl::fromLocalFile(outPath).toString();
      } else {
        error = QStringLiteral("超清优化失败");
      }
    }

    QMetaObject::invokeMethod(
        self,
        [self, trimmedMsg, inflightKey, sourceUrl, outputUrl, ok, error]() {
          if (!self) {
            return;
          }
          if (!inflightKey.isEmpty()) {
            self->enhance_inflight_.remove(inflightKey);
          }
          if (!ok) {
            self->UpdateLastError(error);
          }
          emit self->imageEnhanceFinished(trimmedMsg, sourceUrl, outputUrl, ok,
                                          error);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, 0);
  UpdateLastError(QString());
  emit status(QStringLiteral("已提交超清优化"));
  return true;
}

void QuickClient::QueueAttachmentCacheTask(
    const QString& fileId,
    const std::array<std::uint8_t, 32>& fileKey,
    const QString& fileName,
    qint64 fileSize,
    bool highPriority) {
  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, fileId, fileKey, fileName, fileSize]() {
    if (!self) {
      return;
    }
    const CacheTaskResult result = BuildAttachmentCache(
        self->core_, fileId, fileKey, fileName, fileSize,
        [self, fileId](double progress) {
          if (!self) {
            return;
          }
          QMetaObject::invokeMethod(
              self,
              [self, fileId, progress]() {
                if (!self) {
                  return;
                }
                self->EmitDownloadProgress(fileId, QString(), progress);
              },
              Qt::QueuedConnection);
        });
    QMetaObject::invokeMethod(
        self,
        [self, fileId, result]() {
          if (!self) {
            return;
          }
          self->HandleCacheTaskFinished(
              fileId,
              result.fileUrl.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.fileUrl),
              result.previewUrl.isEmpty() ? QUrl() : QUrl::fromLocalFile(result.previewUrl),
              result.error,
              result.ok);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, highPriority ? 1 : 0);
}

void QuickClient::QueueAttachmentRestoreTask(const QString& fileId,
                                             const QString& fileName,
                                             const QString& savePath,
                                             bool highPriority) {
  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, fileId, fileName, savePath]() {
    if (!self) {
      return;
    }
    QString error;
    const bool ok = RestoreAttachmentFromCache(
        fileId, fileName, savePath,
        [self, fileId, savePath](double progress) {
          if (!self) {
            return;
          }
          QMetaObject::invokeMethod(
              self,
              [self, fileId, savePath, progress]() {
                if (!self) {
                  return;
                }
                self->EmitDownloadProgress(fileId, savePath, progress);
              },
              Qt::QueuedConnection);
        },
        error);
    QMetaObject::invokeMethod(
        self,
        [self, fileId, savePath, ok, error]() {
          if (!self) {
            return;
          }
          self->HandleRestoreTaskFinished(fileId, savePath, ok, error);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, highPriority ? 1 : 0);
}

void QuickClient::HandleCacheTaskFinished(const QString& fileId,
                                          const QUrl& fileUrl,
                                          const QUrl& previewUrl,
                                          const QString& error,
                                          bool ok) {
  cache_inflight_.remove(fileId);
  emit attachmentCacheReady(fileId, fileUrl, previewUrl, error);
  if (!pending_downloads_.contains(fileId)) {
    download_progress_base_.remove(fileId);
    download_progress_span_.remove(fileId);
    return;
  }
  const QStringList paths = pending_downloads_.take(fileId);
  const QString name = pending_download_names_.take(fileId);
  if (!ok) {
    download_progress_base_.remove(fileId);
    download_progress_span_.remove(fileId);
    for (const auto& path : paths) {
      emit attachmentDownloadFinished(fileId, path, false, error);
    }
    return;
  }
  if (download_progress_span_.value(fileId, 1.0) < 1.0) {
    download_progress_base_.insert(fileId, 0.9);
    download_progress_span_.insert(fileId, 0.1);
  }
  for (const auto& path : paths) {
    QueueAttachmentRestoreTask(fileId, name, path, true);
  }
}

void QuickClient::HandleRestoreTaskFinished(const QString& fileId,
                                            const QString& savePath,
                                            bool ok,
                                            const QString& error) {
  if (!ok && !error.isEmpty()) {
    UpdateLastError(error);
  }
  emit attachmentDownloadFinished(fileId, savePath, ok, error);
  download_progress_base_.remove(fileId);
  download_progress_span_.remove(fileId);
}

void QuickClient::MaybeAutoEnhanceImage(const QString& messageId,
                                        const QString& filePath,
                                        const QString& fileName) {
  if (!ai_enhance_enabled_) {
    return;
  }
  const QString trimmedMsg = messageId.trimmed();
  if (trimmedMsg.isEmpty()) {
    return;
  }
  const QString trimmedPath = filePath.trimmed();
  if (trimmedPath.isEmpty()) {
    return;
  }
  const QFileInfo info(trimmedPath);
  if (!info.exists() || !info.isFile()) {
    return;
  }
  if (!IsImageExt(info.suffix())) {
    return;
  }
  if (!EnhancedImagePathIfExists(trimmedMsg).isEmpty()) {
    return;
  }

  QPointer<QuickClient> self(this);
  auto task = new LambdaTask([self, trimmedMsg, trimmedPath, fileName]() {
    if (!self) {
      return;
    }
    const bool shouldEnhance = ShouldAutoEnhanceImage(trimmedPath);
    QMetaObject::invokeMethod(
        self,
        [self, trimmedMsg, trimmedPath, fileName, shouldEnhance]() {
          if (!self || !shouldEnhance) {
            return;
          }
          self->requestImageEnhanceForMessage(
              trimmedMsg,
              QUrl::fromLocalFile(trimmedPath).toString(),
              fileName);
        },
        Qt::QueuedConnection);
  });
  task->setAutoDelete(true);
  cache_pool_.start(task, 0);
}

QVariantList QuickClient::loadHistory(const QString& convId, bool isGroup) {
  QVariantList out;
  const QString trimmed = convId.trimmed();
  if (trimmed.isEmpty()) {
    return out;
  }
  const auto entries =
      core_.LoadChatHistory(trimmed.toStdString(), isGroup, 200);
  out.reserve(static_cast<int>(entries.size()));
  for (const auto& entry : entries) {
    out.push_back(BuildHistoryMessage(entry));
  }
  return out;
}

QVariantList QuickClient::listGroupMembersInfo(const QString& groupId) {
  QVariantList out;
  const QString gid = groupId.trimmed();
  if (gid.isEmpty()) {
    return out;
  }
  const auto members = core_.ListGroupMembersInfo(gid.toStdString());
  out.reserve(static_cast<int>(members.size()));
  for (const auto& m : members) {
    QVariantMap map;
    map.insert(QStringLiteral("username"),
               QString::fromStdString(m.username));
    map.insert(QStringLiteral("role"),
               static_cast<int>(m.role));
    out.push_back(map);
  }
  return out;
}

QVariantList QuickClient::stickerItems() {
  QVariantList out;
  const auto items = EmojiPackManager::Instance().Items();
  out.reserve(items.size());
  for (const auto& item : items) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), item.id);
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("animated"), item.animated);
    map.insert(QStringLiteral("path"), QUrl::fromLocalFile(item.filePath));
    out.push_back(map);
  }
  return out;
}

QVariantMap QuickClient::importSticker(const QString& path) {
  QVariantMap out;
  QString id;
  QString err;
  const bool ok = EmojiPackManager::Instance().ImportSticker(path, &id, &err);
  out.insert(QStringLiteral("ok"), ok);
  if (ok) {
    out.insert(QStringLiteral("stickerId"), id);
    emit status(QStringLiteral("贴纸已导入"));
  } else {
    out.insert(QStringLiteral("error"),
               err.isEmpty() ? QStringLiteral("贴纸导入失败") : err);
    emit status(err.isEmpty() ? QStringLiteral("贴纸导入失败") : err);
  }
  return out;
}

bool QuickClient::sendFriendRequest(const QString& targetUsername,
                                    const QString& remark) {
  const QString target = targetUsername.trimmed();
  if (target.isEmpty()) {
    return false;
  }
  const bool ok = core_.SendFriendRequest(target.toStdString(), remark.toStdString());
  emit status(ok ? QStringLiteral("好友请求已发送") : QStringLiteral("好友请求失败"));
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  } else {
    UpdateLastError(QString());
  }
  return ok;
}

bool QuickClient::respondFriendRequest(const QString& requesterUsername,
                                       bool accept) {
  const QString requester = requesterUsername.trimmed();
  if (requester.isEmpty()) {
    return false;
  }
  const bool ok = core_.RespondFriendRequest(requester.toStdString(), accept);
  emit status(ok ? QStringLiteral("好友请求已处理") : QStringLiteral("好友请求处理失败"));
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  } else {
    UpdateLastError(QString());
  }
  if (ok) {
    UpdateFriendRequests(core_.ListFriendRequests());
    if (accept) {
      UpdateFriendList(core_.ListFriends());
    }
  }
  return ok;
}

QVariantList QuickClient::listDevices() {
  QVariantList out;
  const auto devices = core_.ListDevices();
  out.reserve(static_cast<int>(devices.size()));
  for (const auto& d : devices) {
    QVariantMap map;
    map.insert(QStringLiteral("deviceId"),
               QString::fromStdString(d.device_id));
    map.insert(QStringLiteral("lastSeenSec"),
               static_cast<int>(d.last_seen_sec));
    out.push_back(map);
  }
  return out;
}

bool QuickClient::kickDevice(const QString& deviceId) {
  const QString id = deviceId.trimmed();
  if (id.isEmpty()) {
    UpdateLastError(QStringLiteral("设备 ID 为空"));
    return false;
  }
  const bool ok = core_.KickDevice(id.toStdString());
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
    emit status(QStringLiteral("踢出失败"));
    return false;
  }
  UpdateLastError(QString());
  emit status(QStringLiteral("已踢出设备"));
  return true;
}

bool QuickClient::sendReadReceipt(const QString& peerUsername,
                                  const QString& messageId) {
  const QString peer = peerUsername.trimmed();
  const QString msgId = messageId.trimmed();
  if (peer.isEmpty() || msgId.isEmpty()) {
    return false;
  }
  const bool ok =
      core_.SendChatReadReceipt(peer.toStdString(), msgId.toStdString());
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  }
  return ok;
}

bool QuickClient::trustPendingServer(const QString& pin) {
  const QString p = pin.trimmed();
  if (p.isEmpty()) {
    UpdateLastError(QStringLiteral("验证码为空"));
    return false;
  }
  const bool ok = core_.TrustPendingServer(p.toStdString());
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  } else {
    UpdateLastError(QString());
  }
  MaybeEmitTrustSignals();
  UpdateConnectionState(true);
  return ok;
}

bool QuickClient::trustPendingPeer(const QString& pin) {
  const QString p = pin.trimmed();
  if (p.isEmpty()) {
    UpdateLastError(QStringLiteral("验证码为空"));
    return false;
  }
  const bool ok = core_.TrustPendingPeer(p.toStdString());
  if (!ok) {
    UpdateLastError(QString::fromStdString(core_.last_error()));
  } else {
    UpdateLastError(QString());
  }
  MaybeEmitTrustSignals();
  return ok;
}

QString QuickClient::startVoiceCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  std::array<std::uint8_t, 16> call_id{};
  for (auto& b : call_id) {
    b = static_cast<std::uint8_t>(QRandomGenerator::global()->generate() & 0xFF);
  }
  const QString call_hex = BytesToHex(call_id);
  QString err;
  if (!InitMediaSession(peer, call_hex, true, false, err)) {
    emit status(err.isEmpty() ? QStringLiteral("语音通话初始化失败") : err);
    return {};
  }

  const QString invite =
      QString::fromLatin1(kCallVoicePrefix) + call_hex;
  std::string msg_id;
  core_.SendChatText(peer.toStdString(), invite.toStdString(), msg_id);

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), false);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  emit status(QStringLiteral("语音通话已发起"));
  return call_hex;
}

QString QuickClient::startVideoCall(const QString& peerUsername) {
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty()) {
    return {};
  }
  std::array<std::uint8_t, 16> call_id{};
  for (auto& b : call_id) {
    b = static_cast<std::uint8_t>(QRandomGenerator::global()->generate() & 0xFF);
  }
  const QString call_hex = BytesToHex(call_id);
  QString err;
  if (!InitMediaSession(peer, call_hex, true, true, err)) {
    emit status(err.isEmpty() ? QStringLiteral("视频通话初始化失败") : err);
    return {};
  }

  const QString invite =
      QString::fromLatin1(kCallVideoPrefix) + call_hex;
  std::string msg_id;
  core_.SendChatText(peer.toStdString(), invite.toStdString(), msg_id);

  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), peer);
  msg.insert(QStringLiteral("sender"), username_);
  msg.insert(QStringLiteral("outgoing"), true);
  msg.insert(QStringLiteral("isGroup"), false);
  msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
  msg.insert(QStringLiteral("callId"), call_hex);
  msg.insert(QStringLiteral("video"), true);
  msg.insert(QStringLiteral("time"), NowTimeString());
  msg.insert(QStringLiteral("messageId"), QString::fromStdString(msg_id));
  EmitMessage(msg);
  emit status(QStringLiteral("视频通话已发起"));
  return call_hex;
}

bool QuickClient::joinCall(const QString& peerUsername,
                           const QString& callIdHex,
                           bool video) {
  QString err;
  if (!InitMediaSession(peerUsername, callIdHex, false, video, err)) {
    emit status(err.isEmpty() ? QStringLiteral("加入通话失败") : err);
    return false;
  }
  emit status(QStringLiteral("已加入通话"));
  return true;
}

void QuickClient::endCall() {
  StopMedia();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  emit callStateChanged();
  emit status(QStringLiteral("通话已结束"));
}

void QuickClient::bindRemoteVideoSink(QObject* sink) {
  auto* casted = qobject_cast<QVideoSink*>(sink);
  if (!casted || casted == remote_video_sink_) {
    return;
  }
  remote_video_sink_ = casted;
}

void QuickClient::bindLocalVideoSink(QObject* sink) {
  auto* casted = qobject_cast<QVideoSink*>(sink);
  if (!casted || casted == local_video_sink_) {
    return;
  }
  if (local_video_sink_) {
    disconnect(local_video_sink_, nullptr, this, nullptr);
  }
  local_video_sink_ = casted;
  if (auto* session = EnsureCaptureSession()) {
    session->setVideoSink(local_video_sink_);
  }
  connect(local_video_sink_, &QVideoSink::videoFrameChanged, this,
          &QuickClient::HandleLocalVideoFrame);
}

QString QuickClient::serverInfo() const {
  return QStringLiteral("config: %1").arg(config_path_);
}

QString QuickClient::version() const {
  return QStringLiteral("UI QML 1.0");
}

QUrl QuickClient::defaultDownloadFileUrl(const QString& fileName) const {
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  if (base.isEmpty()) {
    base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  }
  if (base.isEmpty()) {
    base = QDir::homePath();
  }
  if (fileName.trimmed().isEmpty()) {
    return QUrl::fromLocalFile(base);
  }
  return QUrl::fromLocalFile(QDir(base).filePath(fileName.trimmed()));
}

QString QuickClient::systemClipboardText() const {
  return last_system_clipboard_text_;
}

qint64 QuickClient::systemClipboardTimestamp() const {
  return last_system_clipboard_ms_;
}

bool QuickClient::imeAvailable() {
  if (EnsureImeSession() != nullptr) {
    return true;
  }
  return !GetPinyinIndex().dict.isEmpty();
}

bool QuickClient::imeRimeAvailable() {
  return EnsureImeSession() != nullptr;
}

QVariantList QuickClient::imeCandidates(const QString& input,
                                        int maxCandidates) {
  QVariantList items;
  const QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return items;
  }
  const int limit =
      maxCandidates > 0 ? maxCandidates : kMaxPinyinCandidatesPerKey;
  QStringList list;
  void* session = EnsureImeSession();
  if (session) {
    list = ImePluginLoader::instance().queryCandidates(session, trimmed, limit);
  }
  if (list.isEmpty()) {
    list = BuildPinyinCandidates(trimmed, limit);
  }
  for (const auto& candidate : list) {
    items.push_back(candidate);
  }
  return items;
}

QString QuickClient::imePreedit() {
  if (!ime_session_) {
    return {};
  }
  return ImePluginLoader::instance().queryPreedit(ime_session_);
}

bool QuickClient::imeCommit(int index) {
  if (!ime_session_) {
    return false;
  }
  return ImePluginLoader::instance().commitCandidate(ime_session_, index);
}

void QuickClient::imeClear() {
  if (!ime_session_) {
    return;
  }
  ImePluginLoader::instance().clearComposition(ime_session_);
}

void QuickClient::imeReset() {
  if (!ime_session_) {
    return;
  }
  ImePluginLoader::instance().destroySession(ime_session_);
  ime_session_ = nullptr;
}

bool QuickClient::internalImeEnabled() const {
  return internal_ime_enabled_;
}

void QuickClient::setInternalImeEnabled(bool enabled) {
  internal_ime_enabled_ = enabled;
}

bool QuickClient::aiEnhanceGpuAvailable() const {
  return ai_gpu_available_;
}

bool QuickClient::aiEnhanceEnabled() const {
  return ai_enhance_enabled_;
}

void QuickClient::setAiEnhanceEnabled(bool enabled) {
  ai_enhance_enabled_ = enabled;
  SaveAiEnhanceSettings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

int QuickClient::aiEnhanceQualityLevel() const {
  return ai_enhance_quality_;
}

void QuickClient::setAiEnhanceQualityLevel(int level) {
  ai_enhance_quality_ = ClampEnhanceScale(level);
  SaveAiEnhanceSettings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

bool QuickClient::aiEnhanceX4Confirmed() const {
  return ai_enhance_x4_confirmed_;
}

void QuickClient::setAiEnhanceX4Confirmed(bool confirmed) {
  ai_enhance_x4_confirmed_ = confirmed;
  SaveAiEnhanceSettings(ai_enhance_enabled_, ai_enhance_quality_,
                        ai_enhance_x4_confirmed_);
}

QVariantMap QuickClient::aiEnhanceRecommendations() const {
  QVariantMap out;
  out.insert(QStringLiteral("gpuAvailable"), ai_gpu_available_);
  out.insert(QStringLiteral("gpuName"), ai_gpu_name_);
  out.insert(QStringLiteral("gpuSeries"), ai_gpu_series_);
  out.insert(QStringLiteral("perfScale"), ai_rec_perf_scale_);
  out.insert(QStringLiteral("qualityScale"), ai_rec_quality_scale_);
  return out;
}

bool QuickClient::clipboardIsolation() const {
  return clipboard_isolation_enabled_;
}

void QuickClient::setClipboardIsolation(bool enabled) {
  clipboard_isolation_enabled_ = enabled;
}

QString QuickClient::token() const {
  return token_;
}

bool QuickClient::loggedIn() const {
  return !token_.isEmpty();
}

QString QuickClient::username() const {
  return username_;
}

QString QuickClient::lastError() const {
  return last_error_;
}

QVariantList QuickClient::friends() const {
  return friends_;
}

QVariantList QuickClient::groups() const {
  return groups_;
}

QVariantList QuickClient::friendRequests() const {
  return friend_requests_;
}

QString QuickClient::deviceId() const {
  return QString::fromStdString(core_.device_id());
}

bool QuickClient::remoteOk() const {
  return core_.remote_ok();
}

QString QuickClient::remoteError() const {
  return QString::fromStdString(core_.remote_error());
}

bool QuickClient::hasPendingServerTrust() const {
  return core_.HasPendingServerTrust();
}

QString QuickClient::pendingServerFingerprint() const {
  return QString::fromStdString(core_.pending_server_fingerprint());
}

QString QuickClient::pendingServerPin() const {
  return QString::fromStdString(core_.pending_server_pin());
}

bool QuickClient::hasPendingPeerTrust() const {
  return core_.HasPendingPeerTrust();
}

QString QuickClient::pendingPeerUsername() const {
  if (!core_.HasPendingPeerTrust()) {
    return {};
  }
  return QString::fromStdString(core_.pending_peer_trust().peer_username);
}

QString QuickClient::pendingPeerFingerprint() const {
  if (!core_.HasPendingPeerTrust()) {
    return {};
  }
  return QString::fromStdString(core_.pending_peer_trust().fingerprint_hex);
}

QString QuickClient::pendingPeerPin() const {
  if (!core_.HasPendingPeerTrust()) {
    return {};
  }
  return QString::fromStdString(core_.pending_peer_trust().pin6);
}

void QuickClient::StartPolling() {
  if (!poll_timer_.isActive()) {
    last_friend_sync_ms_ = 0;
    last_request_sync_ms_ = 0;
    last_heartbeat_ms_ = 0;
    poll_timer_.start();
  }
}

void QuickClient::StopPolling() {
  if (poll_timer_.isActive()) {
    poll_timer_.stop();
  }
}

void QuickClient::PollOnce() {
  if (!loggedIn()) {
    return;
  }
  const auto poll_result = core_.PollChat();
  const QString poll_error = QString::fromStdString(core_.last_error());
  if (IsSessionInvalidError(poll_error)) {
    HandleSessionInvalid(QStringLiteral("登录已失效，请重新登录"));
    return;
  }
  HandlePollResult(poll_result);
  UpdateConnectionState(false);
  MaybeEmitTrustSignals();

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - last_friend_sync_ms_ > 2000) {
    std::vector<ClientCore::FriendEntry> out;
    bool changed = false;
    if (core_.SyncFriends(out, changed) && changed) {
      UpdateFriendList(out);
    }
    last_friend_sync_ms_ = now;
  }
  if (now - last_request_sync_ms_ > 4000) {
    UpdateFriendRequests(core_.ListFriendRequests());
    last_request_sync_ms_ = now;
  }
  if (now - last_heartbeat_ms_ > 5000) {
    core_.Heartbeat();
    last_heartbeat_ms_ = now;
  }

  if (media_session_ && !media_timer_.isActive()) {
    std::string err;
    media_session_->PollIncoming(16, 0, err);
  }
}

void QuickClient::EmitMessage(const QVariantMap& message) {
  emit messageEvent(message);
}

void QuickClient::UpdateFriendList(
    const std::vector<ClientCore::FriendEntry>& friends) {
  QVariantList updated;
  updated.reserve(static_cast<int>(friends.size()));
  for (const auto& entry : friends) {
    QVariantMap map;
    map.insert(QStringLiteral("username"),
               QString::fromStdString(entry.username));
    map.insert(QStringLiteral("remark"), QString::fromStdString(entry.remark));
    updated.push_back(map);
  }
  friends_ = updated;
  emit friendsChanged();
}

void QuickClient::UpdateFriendRequests(
    const std::vector<ClientCore::FriendRequestEntry>& requests) {
  QVariantList updated;
  updated.reserve(static_cast<int>(requests.size()));
  for (const auto& entry : requests) {
    QVariantMap map;
    map.insert(QStringLiteral("username"),
               QString::fromStdString(entry.requester_username));
    map.insert(QStringLiteral("remark"),
               QString::fromStdString(entry.requester_remark));
    updated.push_back(map);
  }
  friend_requests_ = updated;
  emit friendRequestsChanged();
}

bool QuickClient::AddGroupIfMissing(const QString& groupId) {
  for (const auto& entry : groups_) {
    const auto map = entry.toMap();
    if (map.value(QStringLiteral("id")).toString() == groupId) {
      return false;
    }
  }
  QVariantMap map;
  map.insert(QStringLiteral("id"), groupId);
  map.insert(QStringLiteral("name"), groupId);
  map.insert(QStringLiteral("unread"), 0);
  groups_.push_back(map);
  return true;
}

QVariantMap QuickClient::BuildStickerMeta(const QString& stickerId) const {
  QVariantMap meta;
  const auto* item = EmojiPackManager::Instance().Find(stickerId);
  if (!item) {
    return meta;
  }
  meta.insert(QStringLiteral("stickerId"), item->id);
  meta.insert(QStringLiteral("stickerTitle"), item->title);
  meta.insert(QStringLiteral("stickerAnimated"), item->animated);
  meta.insert(QStringLiteral("stickerUrl"),
              QUrl::fromLocalFile(item->filePath));
  return meta;
}

QVariantMap QuickClient::BuildHistoryMessage(
    const ClientCore::HistoryEntry& entry) const {
  QVariantMap msg;
  msg.insert(QStringLiteral("convId"), QString::fromStdString(entry.conv_id));
  msg.insert(QStringLiteral("sender"), QString::fromStdString(entry.sender));
  msg.insert(QStringLiteral("outgoing"), entry.outgoing);
  msg.insert(QStringLiteral("isGroup"), entry.is_group);
  const QString messageId = QString::fromStdString(entry.message_id_hex);
  msg.insert(QStringLiteral("messageId"), messageId);
  msg.insert(QStringLiteral("time"),
             QDateTime::fromSecsSinceEpoch(
                 static_cast<qint64>(entry.timestamp_sec))
                 .toString(QStringLiteral("HH:mm:ss")));
  switch (entry.status) {
    case ClientCore::HistoryStatus::kSent:
      msg.insert(QStringLiteral("status"), QStringLiteral("sent"));
      break;
    case ClientCore::HistoryStatus::kDelivered:
      msg.insert(QStringLiteral("status"), QStringLiteral("delivered"));
      break;
    case ClientCore::HistoryStatus::kRead:
      msg.insert(QStringLiteral("status"), QStringLiteral("read"));
      break;
    case ClientCore::HistoryStatus::kFailed:
      msg.insert(QStringLiteral("status"), QStringLiteral("failed"));
      break;
    default:
      msg.insert(QStringLiteral("status"), QStringLiteral("sent"));
      break;
  }

  switch (entry.kind) {
    case ClientCore::HistoryKind::kText:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      msg.insert(QStringLiteral("text"), QString::fromStdString(entry.text_utf8));
      break;
    case ClientCore::HistoryKind::kFile:
      msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
      msg.insert(QStringLiteral("fileName"),
                 QString::fromStdString(entry.file_name));
      msg.insert(QStringLiteral("fileSize"),
                 static_cast<qint64>(entry.file_size));
      msg.insert(QStringLiteral("fileId"), QString::fromStdString(entry.file_id));
      msg.insert(QStringLiteral("fileKey"), BytesToHex32(entry.file_key));
      if (!messageId.isEmpty()) {
        const QString enhancedPath = EnhancedImagePathIfExists(messageId);
        if (!enhancedPath.isEmpty()) {
          const QString ext =
              QFileInfo(QString::fromStdString(entry.file_name)).suffix();
          if (IsImageExt(ext)) {
            msg.insert(QStringLiteral("fileUrl"),
                       QUrl::fromLocalFile(enhancedPath));
            msg.insert(QStringLiteral("imageEnhanced"), true);
          }
        }
      }
      break;
    case ClientCore::HistoryKind::kSticker: {
      msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
      const QString sid = QString::fromStdString(entry.sticker_id);
      msg.insert(QStringLiteral("stickerId"), sid);
      const auto meta = BuildStickerMeta(sid);
      msg.insert(QStringLiteral("stickerUrl"),
                 meta.value(QStringLiteral("stickerUrl")));
      msg.insert(QStringLiteral("stickerAnimated"),
                 meta.value(QStringLiteral("stickerAnimated")));
      break;
    }
    case ClientCore::HistoryKind::kSystem:
      msg.insert(QStringLiteral("kind"), QStringLiteral("system"));
      msg.insert(QStringLiteral("text"), QString::fromStdString(entry.text_utf8));
      break;
    default:
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      break;
  }
  return msg;
}

void QuickClient::HandlePollResult(const ClientCore::ChatPollResult& result) {
  const QString now = NowTimeString();

  for (const auto& t : result.texts) {
    const QString text = QString::fromStdString(t.text_utf8);
    const auto invite = ParseCallInvite(text);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    if (invite.ok) {
      msg.insert(QStringLiteral("kind"), QStringLiteral("call_invite"));
      msg.insert(QStringLiteral("callId"), invite.callId);
      msg.insert(QStringLiteral("video"), invite.video);
    } else {
      msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
      msg.insert(QStringLiteral("text"), text);
    }
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_texts) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& s : result.stickers) {
    const QString sid = QString::fromStdString(s.sticker_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(s.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(s.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
    msg.insert(QStringLiteral("stickerId"), sid);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(s.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    const auto meta = BuildStickerMeta(sid);
    msg.insert(QStringLiteral("stickerUrl"),
               meta.value(QStringLiteral("stickerUrl")));
    msg.insert(QStringLiteral("stickerAnimated"),
               meta.value(QStringLiteral("stickerAnimated")));
    EmitMessage(msg);
  }

  for (const auto& s : result.outgoing_stickers) {
    const QString sid = QString::fromStdString(s.sticker_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(s.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("sticker"));
    msg.insert(QStringLiteral("stickerId"), sid);
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(s.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    const auto meta = BuildStickerMeta(sid);
    msg.insert(QStringLiteral("stickerUrl"),
               meta.value(QStringLiteral("stickerUrl")));
    msg.insert(QStringLiteral("stickerAnimated"),
               meta.value(QStringLiteral("stickerAnimated")));
    EmitMessage(msg);
  }

  for (const auto& f : result.files) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.outgoing_files) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(f.peer_username));
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), false);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& t : result.group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& t : result.outgoing_group_texts) {
    const QString group_id = QString::fromStdString(t.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("text"));
    msg.insert(QStringLiteral("text"), QString::fromStdString(t.text_utf8));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(t.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(f.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& f : result.outgoing_group_files) {
    const QString group_id = QString::fromStdString(f.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), username_);
    msg.insert(QStringLiteral("outgoing"), true);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("file"));
    msg.insert(QStringLiteral("fileName"), QString::fromStdString(f.file_name));
    msg.insert(QStringLiteral("fileSize"),
               static_cast<qint64>(f.file_size));
    msg.insert(QStringLiteral("fileId"), QString::fromStdString(f.file_id));
    msg.insert(QStringLiteral("fileKey"), BytesToHex32(f.file_key));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(f.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& inv : result.group_invites) {
    const QString group_id = QString::fromStdString(inv.group_id);
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"),
               QString::fromStdString(inv.from_username));
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("group_invite"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(inv.message_id_hex));
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& n : result.group_notices) {
    const QString group_id = QString::fromStdString(n.group_id);
    if (AddGroupIfMissing(group_id)) {
      emit groupsChanged();
    }
    const QString actor = QString::fromStdString(n.actor_username);
    const QString target = QString::fromStdString(n.target_username);
    QString text;
    switch (n.kind) {
      case 1:
        text = QStringLiteral("%1 加入群聊").arg(target);
        break;
      case 2:
        text = QStringLiteral("%1 离开群聊").arg(target);
        break;
      case 3:
        text = QStringLiteral("%1 被移出群聊").arg(target);
        break;
      case 4:
        text = QStringLiteral("%1 权限变更").arg(target);
        break;
      default:
        text = QStringLiteral("群通知更新");
        break;
    }
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"), group_id);
    msg.insert(QStringLiteral("sender"), actor);
    msg.insert(QStringLiteral("outgoing"), false);
    msg.insert(QStringLiteral("isGroup"), true);
    msg.insert(QStringLiteral("kind"), QStringLiteral("notice"));
    msg.insert(QStringLiteral("text"), text);
    msg.insert(QStringLiteral("noticeKind"), static_cast<int>(n.kind));
    msg.insert(QStringLiteral("noticeActor"), actor);
    msg.insert(QStringLiteral("noticeTarget"), target);
    msg.insert(QStringLiteral("time"), now);
    EmitMessage(msg);
  }

  for (const auto& d : result.deliveries) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(d.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("delivery"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(d.message_id_hex));
    EmitMessage(msg);
  }

  for (const auto& r : result.read_receipts) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(r.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("read"));
    msg.insert(QStringLiteral("messageId"),
               QString::fromStdString(r.message_id_hex));
    EmitMessage(msg);
  }

  for (const auto& t : result.typing_events) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(t.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("typing"));
    msg.insert(QStringLiteral("typing"), t.typing);
    EmitMessage(msg);
  }

  for (const auto& p : result.presence_events) {
    QVariantMap msg;
    msg.insert(QStringLiteral("convId"),
               QString::fromStdString(p.from_username));
    msg.insert(QStringLiteral("kind"), QStringLiteral("presence"));
    msg.insert(QStringLiteral("online"), p.online);
    EmitMessage(msg);
  }
}

void QuickClient::HandleSessionInvalid(const QString& message) {
  const QString hint = message.trimmed().isEmpty()
                           ? QStringLiteral("登录已失效，请重新登录")
                           : message.trimmed();
  const bool was_logged_in = !token_.isEmpty() || !username_.isEmpty();

  StopPolling();
  StopMedia();
  core_.Logout();
  token_.clear();
  username_.clear();
  friends_.clear();
  groups_.clear();
  friend_requests_.clear();
  active_call_id_.clear();
  active_call_peer_.clear();
  active_call_video_ = false;
  UpdateConnectionState(true);
  MaybeEmitTrustSignals();

  if (last_error_ != hint) {
    last_error_ = hint;
    emit errorChanged();
  }
  if (was_logged_in) {
    emit tokenChanged();
    emit userChanged();
    emit friendsChanged();
    emit groupsChanged();
    emit friendRequestsChanged();
    emit callStateChanged();
  }
  emit status(hint);
}

void QuickClient::UpdateLastError(const QString& message) {
  const QString trimmed = message.trimmed();
  if (IsSessionInvalidError(trimmed)) {
    HandleSessionInvalid(QStringLiteral("登录已失效，请重新登录"));
    return;
  }
  if (trimmed == last_error_) {
    return;
  }
  last_error_ = trimmed;
  emit errorChanged();
}

void QuickClient::UpdateConnectionState(bool force_emit) {
  const bool ok = core_.remote_ok();
  const QString err = QString::fromStdString(core_.remote_error());
  if (!force_emit && ok == last_remote_ok_ && err == last_remote_error_) {
    return;
  }
  last_remote_ok_ = ok;
  last_remote_error_ = err;
  emit connectionChanged();
}

void QuickClient::MaybeEmitTrustSignals() {
  bool changed = false;
  if (core_.HasPendingServerTrust()) {
    const QString fp = pendingServerFingerprint();
    if (fp != last_pending_server_fingerprint_) {
      last_pending_server_fingerprint_ = fp;
      emit serverTrustRequired(fp, pendingServerPin());
      changed = true;
    }
  } else if (!last_pending_server_fingerprint_.isEmpty()) {
    last_pending_server_fingerprint_.clear();
    changed = true;
  }

  if (core_.HasPendingPeerTrust()) {
    const QString fp = pendingPeerFingerprint();
    if (fp != last_pending_peer_fingerprint_) {
      last_pending_peer_fingerprint_ = fp;
      emit peerTrustRequired(pendingPeerUsername(), fp, pendingPeerPin());
      changed = true;
    }
  } else if (!last_pending_peer_fingerprint_.isEmpty()) {
    last_pending_peer_fingerprint_.clear();
    changed = true;
  }

  if (changed) {
    emit trustStateChanged();
  }
}

void QuickClient::EmitDownloadProgress(const QString& fileId,
                                       const QString& savePath,
                                       double progress) {
  const double base = download_progress_base_.value(fileId, 0.0);
  const double span = download_progress_span_.value(fileId, 1.0);
  double clamped = std::max(0.0, std::min(1.0, progress));
  double scaled = base + clamped * span;
  scaled = std::max(0.0, std::min(1.0, scaled));
  emit attachmentDownloadProgress(fileId, savePath, scaled);
}

bool QuickClient::InitMediaSession(const QString& peerUsername,
                                   const QString& callIdHex,
                                   bool initiator,
                                   bool video,
                                   QString& outError) {
  outError.clear();
  StopMedia();
  const QString peer = peerUsername.trimmed();
  if (peer.isEmpty() || callIdHex.trimmed().isEmpty()) {
    outError = QStringLiteral("通话参数无效");
    return false;
  }
  std::array<std::uint8_t, 16> call_id{};
  if (!HexToBytes16(callIdHex, call_id)) {
    outError = QStringLiteral("通话 ID 格式错误");
    return false;
  }
  mi::client::media::MediaSessionConfig cfg;
  cfg.peer_username = peer.toStdString();
  cfg.call_id = call_id;
  cfg.initiator = initiator;
  cfg.enable_audio = true;
  cfg.enable_video = video;

  auto session = std::make_unique<mi::client::media::MediaSession>(core_, cfg);
  std::string err;
  if (!session->Init(err)) {
    outError = err.empty() ? QStringLiteral("通话初始化失败")
                           : QString::fromStdString(err);
    return false;
  }
  media_session_ = std::move(session);
  audio_config_ = mi::client::media::AudioPipelineConfig{};
  const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
  const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
  AdjustAudioConfigForDevices(in_device, out_device, audio_config_);
  audio_pipeline_ = std::make_unique<mi::client::media::AudioPipeline>(
      *media_session_, audio_config_);
  if (!audio_pipeline_->Init(err)) {
    outError = err.empty() ? QStringLiteral("音频编码初始化失败")
                           : QString::fromStdString(err);
    StopMedia();
    return false;
  }
  if (video) {
    video_config_ = mi::client::media::VideoPipelineConfig{};
    if (!SetupVideo(outError)) {
      StopMedia();
      return false;
    }
    video_pipeline_ = std::make_unique<mi::client::media::VideoPipeline>(
        *media_session_, video_config_);
    if (!video_pipeline_->Init(err)) {
      outError = err.empty() ? QStringLiteral("视频编码初始化失败")
                             : QString::fromStdString(err);
      StopMedia();
      return false;
    }
  }
  if (!SetupAudio(outError)) {
    StopMedia();
    return false;
  }
  StartMedia();
  active_call_id_ = callIdHex.trimmed();
  active_call_peer_ = peer;
  active_call_video_ = video;
  emit callStateChanged();
  return true;
}

void QuickClient::StartMedia() {
  if (!media_timer_.isActive()) {
    media_timer_.start();
  }
  if (camera_ && !camera_->isActive()) {
    camera_->start();
  }
}

void QuickClient::StopMedia() {
  if (media_timer_.isActive()) {
    media_timer_.stop();
  }
  ShutdownAudio();
  ShutdownVideo();
  audio_pipeline_.reset();
  video_pipeline_.reset();
  media_session_.reset();
  audio_in_buffer_.clear();
  audio_out_pending_.clear();
  audio_in_offset_ = 0;
  audio_frame_tmp_.clear();
  video_send_buffer_.clear();
  if (remote_video_sink_) {
    remote_video_sink_->setVideoFrame(QVideoFrame());
  }
}

void QuickClient::PumpMedia() {
  if (!media_session_) {
    return;
  }
  std::string err;
  media_session_->PollIncoming(32, 0, err);

  if (audio_pipeline_) {
    audio_pipeline_->PumpIncoming();
    DrainAudioInput();
    mi::client::media::PcmFrame decoded;
    const int frame_samples = audio_pipeline_->frame_samples();
    const int frame_bytes = frame_samples * static_cast<int>(sizeof(std::int16_t));
    const int max_pending = frame_bytes * 10;
    while (audio_pipeline_->PopDecodedFrame(decoded)) {
      if (!decoded.samples.empty()) {
        const char* ptr =
            reinterpret_cast<const char*>(decoded.samples.data());
        const int bytes =
            static_cast<int>(decoded.samples.size() * sizeof(std::int16_t));
        if (bytes > 0) {
          audio_out_pending_.append(ptr, bytes);
          if (audio_out_pending_.size() > max_pending) {
            const int trim = audio_out_pending_.size() - max_pending;
            audio_out_pending_.remove(0, trim);
          }
        }
      }
    }
    FlushAudioOutput();
  }

  if (video_pipeline_) {
    video_pipeline_->PumpIncoming();
    mi::client::media::VideoFrameData latest;
    bool has_frame = false;
    while (video_pipeline_->PopDecodedFrame(latest)) {
      has_frame = true;
    }
    if (has_frame && remote_video_sink_ && latest.width > 0 &&
        latest.height > 0 && !latest.nv12.empty()) {
      std::uint32_t stride = latest.stride;
      if (stride == 0) {
        const std::size_t denom =
            static_cast<std::size_t>(latest.height) * 3;
        const std::size_t maybe =
            denom == 0 ? 0 : latest.nv12.size() * 2 / denom;
        stride = maybe >= latest.width
                     ? static_cast<std::uint32_t>(maybe)
                     : latest.width;
      }
      auto buffer = std::make_unique<Nv12VideoBuffer>(
          std::move(latest.nv12), latest.width, latest.height, stride);
      QVideoFrame frame(std::move(buffer));
      frame.setStartTime(static_cast<qint64>(latest.timestamp_ms));
      remote_video_sink_->setVideoFrame(frame);
    }
  }
}

void QuickClient::DrainAudioInput() {
  if (!audio_pipeline_ || !audio_in_device_) {
    return;
  }
  const int frame_samples = audio_pipeline_->frame_samples();
  if (frame_samples <= 0) {
    return;
  }
  const int frame_bytes = frame_samples * static_cast<int>(sizeof(std::int16_t));
  if (frame_bytes <= 0) {
    return;
  }
  if (audio_frame_tmp_.size() != static_cast<std::size_t>(frame_samples)) {
    audio_frame_tmp_.assign(static_cast<std::size_t>(frame_samples), 0);
  }
  while (audio_in_buffer_.size() - audio_in_offset_ >= frame_bytes) {
    const char* src = audio_in_buffer_.constData() + audio_in_offset_;
    std::memcpy(audio_frame_tmp_.data(), src,
                static_cast<std::size_t>(frame_bytes));
    audio_in_offset_ += frame_bytes;
    audio_pipeline_->SendPcmFrame(audio_frame_tmp_.data(),
                                  static_cast<std::size_t>(frame_samples));
  }
  if (audio_in_offset_ > 0 &&
      audio_in_offset_ >= audio_in_buffer_.size() / 2) {
    audio_in_buffer_.remove(0, audio_in_offset_);
    audio_in_offset_ = 0;
  }
}

void QuickClient::FlushAudioOutput() {
  if (!audio_out_device_ || audio_out_pending_.isEmpty()) {
    return;
  }
  for (;;) {
    const qint64 written = audio_out_device_->write(audio_out_pending_);
    if (written <= 0) {
      break;
    }
    audio_out_pending_.remove(0, static_cast<int>(written));
    if (audio_out_pending_.isEmpty()) {
      break;
    }
  }
}

bool QuickClient::SetupAudio(QString& outError) {
  outError.clear();
  if (!audio_pipeline_) {
    return true;
  }
  const QAudioDevice in_device = QMediaDevices::defaultAudioInput();
  const QAudioDevice out_device = QMediaDevices::defaultAudioOutput();
  const bool have_in = !in_device.isNull();
  const bool have_out = !out_device.isNull();
  if (!have_in && !have_out) {
    outError = QStringLiteral("未找到音频设备");
    return false;
  }
  QAudioFormat format;
  format.setSampleRate(audio_config_.sample_rate);
  format.setChannelCount(audio_config_.channels);
  format.setSampleFormat(QAudioFormat::Int16);
  const bool in_ok = have_in && in_device.isFormatSupported(format);
  const bool out_ok = have_out && out_device.isFormatSupported(format);
  if (!in_ok && !out_ok) {
    outError = QStringLiteral("音频格式不支持");
    return false;
  }
  if (in_ok) {
    audio_source_ = std::make_unique<QAudioSource>(in_device, format, this);
  }
  if (out_ok) {
    audio_sink_ = std::make_unique<QAudioSink>(out_device, format, this);
  }
  const int frame_bytes =
      audio_pipeline_->frame_samples() * static_cast<int>(sizeof(std::int16_t));
  if (frame_bytes > 0) {
    if (audio_source_) {
      audio_source_->setBufferSize(frame_bytes * 4);
    }
    if (audio_sink_) {
      audio_sink_->setBufferSize(frame_bytes * 8);
    }
  }
  if (audio_source_) {
    audio_in_device_ = audio_source_->start();
    if (!audio_in_device_) {
      audio_source_.reset();
    }
  }
  if (audio_sink_) {
    audio_out_device_ = audio_sink_->start();
    if (!audio_out_device_) {
      audio_sink_.reset();
    }
  }
  if (!audio_in_device_ && !audio_out_device_) {
    outError = QStringLiteral("音频设备启动失败");
    return false;
  }
  if (audio_in_device_) {
    connect(audio_in_device_, &QIODevice::readyRead, this,
            &QuickClient::HandleAudioReady);
  }
  return true;
}

bool QuickClient::SetupVideo(QString& outError) {
  outError.clear();
  const QCameraDevice device = QMediaDevices::defaultVideoInput();
  if (device.isNull()) {
    return true;
  }
  QMediaCaptureSession* session = EnsureCaptureSession();
  if (!session) {
    outError = QStringLiteral("视频模块初始化失败");
    return false;
  }
  camera_ = std::make_unique<QCamera>(device);
  session->setCamera(camera_.get());
  session->setVideoSink(local_video_sink_);
  if (local_video_sink_) {
    disconnect(local_video_sink_, nullptr, this, nullptr);
    connect(local_video_sink_, &QVideoSink::videoFrameChanged, this,
            &QuickClient::HandleLocalVideoFrame);
  }
  if (!SelectCameraFormat()) {
    const QCameraFormat fmt = camera_->cameraFormat();
    if (fmt.isNull()) {
      outError = QStringLiteral("摄像头格式不可用");
      return false;
    }
    const QSize res = fmt.resolution();
    if (res.isValid()) {
      video_config_.width = static_cast<std::uint32_t>(res.width());
      video_config_.height = static_cast<std::uint32_t>(res.height());
    }
    const float max_fps = fmt.maxFrameRate();
    if (max_fps > 1.0f) {
      video_config_.fps =
          static_cast<std::uint32_t>(std::lround(max_fps));
    }
    if (video_config_.fps == 0) {
      video_config_.fps = 24;
    }
  }
  return true;
}

void QuickClient::ShutdownAudio() {
  if (audio_source_) {
    audio_source_->stop();
  }
  if (audio_sink_) {
    audio_sink_->stop();
  }
  audio_in_device_ = nullptr;
  audio_out_device_ = nullptr;
  audio_source_.reset();
  audio_sink_.reset();
  audio_in_buffer_.clear();
  audio_out_pending_.clear();
  audio_in_offset_ = 0;
}

void QuickClient::ShutdownVideo() {
  if (camera_) {
    camera_->stop();
  }
  if (capture_session_) {
    capture_session_->setVideoSink(nullptr);
    capture_session_->setCamera(nullptr);
  }
  camera_.reset();
}

QMediaCaptureSession* QuickClient::EnsureCaptureSession() {
  if (!capture_session_) {
    capture_session_ = std::make_unique<QMediaCaptureSession>(this);
  }
  return capture_session_.get();
}

void* QuickClient::EnsureImeSession() {
  if (ime_session_) {
    return ime_session_;
  }
  ime_session_ = ImePluginLoader::instance().createSession();
  return ime_session_;
}

void QuickClient::HandleAudioReady() {
  if (!audio_in_device_) {
    return;
  }
  const QByteArray data = audio_in_device_->readAll();
  if (data.isEmpty()) {
    return;
  }
  audio_in_buffer_.append(data);
  DrainAudioInput();
}

void QuickClient::HandleLocalVideoFrame(const QVideoFrame& frame) {
  if (!video_pipeline_ || !media_session_) {
    return;
  }
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t stride = 0;
  if (!ConvertVideoFrameToNv12(frame, video_send_buffer_, width, height,
                               stride)) {
    return;
  }
  if (width == 0 || height == 0 || stride == 0) {
    return;
  }
  video_pipeline_->SendNv12Frame(video_send_buffer_.data(), stride, width,
                                 height);
}

bool QuickClient::ConvertVideoFrameToNv12(const QVideoFrame& frame,
                                          std::vector<std::uint8_t>& out,
                                          std::uint32_t& width,
                                          std::uint32_t& height,
                                          std::size_t& stride) const {
  QVideoFrame mapped(frame);
  if (!mapped.isValid()) {
    return false;
  }
  if (!mapped.map(QVideoFrame::ReadOnly)) {
    return false;
  }
  width = static_cast<std::uint32_t>(mapped.width());
  height = static_cast<std::uint32_t>(mapped.height());
  if (width == 0 || height == 0) {
    mapped.unmap();
    return false;
  }
  stride = width;
  const std::size_t y_bytes =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const std::size_t uv_bytes = y_bytes / 2;
  out.resize(y_bytes + uv_bytes);
  std::uint8_t* y_out = out.data();
  std::uint8_t* uv_out = out.data() + y_bytes;
  const auto fmt = mapped.pixelFormat();

  if (fmt == QVideoFrameFormat::Format_NV12 ||
      fmt == QVideoFrameFormat::Format_NV21) {
    const int y_stride = mapped.bytesPerLine(0);
    const int uv_stride = mapped.bytesPerLine(1);
    const std::uint8_t* y_src = mapped.bits(0);
    const std::uint8_t* uv_src = mapped.bits(1);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(y_out + row * width, y_src + row * y_stride, width);
    }
    const std::uint32_t uv_height = height / 2;
    if (fmt == QVideoFrameFormat::Format_NV12) {
      for (std::uint32_t row = 0; row < uv_height; ++row) {
        std::memcpy(uv_out + row * width, uv_src + row * uv_stride, width);
      }
    } else {
      for (std::uint32_t row = 0; row < uv_height; ++row) {
        const std::uint8_t* src = uv_src + row * uv_stride;
        std::uint8_t* dst = uv_out + row * width;
        for (std::uint32_t col = 0; col + 1 < width; col += 2) {
          dst[col] = src[col + 1];
          dst[col + 1] = src[col];
        }
      }
    }
    mapped.unmap();
    return true;
  }

  if (fmt == QVideoFrameFormat::Format_YUV420P ||
      fmt == QVideoFrameFormat::Format_YV12) {
    const int y_stride = mapped.bytesPerLine(0);
    const int u_stride = mapped.bytesPerLine(1);
    const int v_stride = mapped.bytesPerLine(2);
    const std::uint8_t* y_src = mapped.bits(0);
    const std::uint8_t* u_src = mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 1 : 2);
    const std::uint8_t* v_src = mapped.bits(fmt == QVideoFrameFormat::Format_YUV420P ? 2 : 1);
    for (std::uint32_t row = 0; row < height; ++row) {
      std::memcpy(y_out + row * width, y_src + row * y_stride, width);
    }
    const std::uint32_t uv_height = height / 2;
    for (std::uint32_t row = 0; row < uv_height; ++row) {
      const std::uint8_t* u_line = u_src + row * u_stride;
      const std::uint8_t* v_line = v_src + row * v_stride;
      std::uint8_t* dst = uv_out + row * width;
      for (std::uint32_t col = 0; col + 1 < width; col += 2) {
        dst[col] = u_line[col / 2];
        dst[col + 1] = v_line[col / 2];
      }
    }
    mapped.unmap();
    return true;
  }

  if (fmt == QVideoFrameFormat::Format_YUYV ||
      fmt == QVideoFrameFormat::Format_UYVY) {
    const int src_stride = mapped.bytesPerLine(0);
    const std::uint8_t* src = mapped.bits(0);
    const std::uint32_t width_even = width & ~1u;
    for (std::uint32_t row = 0; row < height; ++row) {
      const std::uint8_t* line = src + row * src_stride;
      for (std::uint32_t col = 0; col < width_even; col += 2) {
        std::uint8_t y0 = 0;
        std::uint8_t y1 = 0;
        std::uint8_t u = 0;
        std::uint8_t v = 0;
        if (fmt == QVideoFrameFormat::Format_YUYV) {
          y0 = line[0];
          u = line[1];
          y1 = line[2];
          v = line[3];
        } else {
          u = line[0];
          y0 = line[1];
          v = line[2];
          y1 = line[3];
        }
        y_out[row * width + col] = y0;
        if (col + 1 < width) {
          y_out[row * width + col + 1] = y1;
        }
        if ((row & 1u) == 0) {
          std::uint8_t* dst = uv_out + (row / 2) * width;
          dst[col] = u;
          if (col + 1 < width) {
            dst[col + 1] = v;
          }
        }
        line += 4;
      }
    }
    mapped.unmap();
    return true;
  }

  mapped.unmap();
  return false;
}

bool QuickClient::SelectCameraFormat() {
  if (!camera_) {
    return false;
  }
  const auto formats = camera_->cameraDevice().videoFormats();
  if (formats.isEmpty()) {
    return false;
  }
  const QSize target(static_cast<int>(video_config_.width),
                     static_cast<int>(video_config_.height));
  int best_score = std::numeric_limits<int>::max();
  QCameraFormat best;
  bool found = false;
  for (const auto& fmt : formats) {
    const auto pix = fmt.pixelFormat();
    if (pix != QVideoFrameFormat::Format_NV12 &&
        pix != QVideoFrameFormat::Format_NV21 &&
        pix != QVideoFrameFormat::Format_YUV420P &&
        pix != QVideoFrameFormat::Format_YV12 &&
        pix != QVideoFrameFormat::Format_YUYV &&
        pix != QVideoFrameFormat::Format_UYVY) {
      continue;
    }
    const QSize res = fmt.resolution();
    int score = std::abs(res.width() - target.width()) +
                std::abs(res.height() - target.height());
    if (pix != QVideoFrameFormat::Format_NV12) {
      score += 200;
    }
    const float max_fps = fmt.maxFrameRate();
    if (max_fps > 0.0f) {
      score += static_cast<int>(
          std::abs(max_fps - static_cast<float>(video_config_.fps)) * 10.0f);
    }
    if (!found || score < best_score) {
      best = fmt;
      best_score = score;
      found = true;
    }
  }
  if (!found || best.isNull()) {
    return false;
  }
  camera_->setCameraFormat(best);
  const QSize res = best.resolution();
  if (res.isValid()) {
    video_config_.width = static_cast<std::uint32_t>(res.width());
    video_config_.height = static_cast<std::uint32_t>(res.height());
  }
  const float max_fps = best.maxFrameRate();
  if (max_fps > 1.0f) {
    video_config_.fps = static_cast<std::uint32_t>(std::lround(max_fps));
  }
  if (video_config_.fps == 0) {
    video_config_.fps = 24;
  }
  return true;
}

QString QuickClient::BytesToHex(const std::array<std::uint8_t, 16>& bytes) {
  const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<int>(bytes.size()));
  return QString::fromLatin1(raw.toHex());
}

bool QuickClient::HexToBytes16(const QString& hex,
                               std::array<std::uint8_t, 16>& out) {
  const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
  if (raw.size() != static_cast<int>(out.size())) {
    return false;
  }
  std::memcpy(out.data(), raw.data(), out.size());
  return true;
}

QString QuickClient::BytesToHex32(const std::array<std::uint8_t, 32>& bytes) {
  const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<int>(bytes.size()));
  return QString::fromLatin1(raw.toHex());
}

bool QuickClient::HexToBytes32(const QString& hex,
                               std::array<std::uint8_t, 32>& out) {
  const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
  if (raw.size() != static_cast<int>(out.size())) {
    return false;
  }
  std::memcpy(out.data(), raw.data(), out.size());
  return true;
}

}  // namespace mi::client::ui
