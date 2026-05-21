namespace {
constexpr char kCallVoicePrefix[] = "[call]voice:";
constexpr char kCallVideoPrefix[] = "[call]video:";
constexpr char kCallEndPrefix[] = "[call]end:";
constexpr char kRecallPrefix[] = "[recall]:";
constexpr std::uint8_t kGroupCallOpCreate = 1;
constexpr std::uint8_t kGroupCallOpJoin = 2;
constexpr std::uint8_t kGroupCallOpLeave = 3;
constexpr std::uint8_t kGroupCallOpEnd = 4;
constexpr std::uint8_t kGroupCallOpUpdate = 5;
constexpr std::uint8_t kGroupCallOpPing = 6;
constexpr std::uint8_t kGroupCallMediaAudio = 0x01;
constexpr std::uint8_t kGroupCallMediaVideo = 0x02;
constexpr int kMaxPinyinCandidatesPerKey = 5;
constexpr int kMaxAbbrInputLength = 10;
constexpr const char kPinyinDictResourcePath[] = ":/mi/e2ee/ui/ime/pinyin.dat";
constexpr const char kPinyinAbbrDictResourcePath[] =
    ":/mi/e2ee/ui/ime/pinyin_short.dat";
constexpr std::uint32_t kMaxFriendEntries = 512;
constexpr std::uint32_t kMaxFriendRequestEntries = 256;
constexpr std::uint32_t kMaxGroupMemberEntries = 256;
constexpr std::uint32_t kMaxGroupCallMembers = 256;
constexpr std::uint32_t kMaxHistoryEntries = 200;
constexpr auto kUiPlaintextLease = std::chrono::milliseconds(120);

QString to_ui_qstring(std::string_view view) {
  if (view.empty()) {
    return QString();
  }
  return QString::fromUtf8(view.data(), static_cast<int>(view.size()));
}

template <typename Fn>
bool with_protected_ui_text(const UiProtectedText& protected_text, Fn&& fn) {
  return protected_text.WithPlaintext(
      kUiPlaintextLease,
      [&](std::string_view view) { return static_cast<bool>(fn(view)); });
}

std::vector<mi::sdk::FriendEntry> read_friend_entries(
    const mi_friend_entry_t* entries,
    std::uint32_t count) {
  std::vector<mi::sdk::FriendEntry> out;
  if (!entries || count == 0) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    mi::sdk::FriendEntry e;
    if (entries[i].username) {
      e.username = entries[i].username;
    }
    if (entries[i].remark) {
      e.remark = entries[i].remark;
    }
    out.push_back(std::move(e));
  }
  return out;
}

std::vector<mi::sdk::FriendRequestEntry> read_friend_request_entries(
    const mi_friend_request_entry_t* entries,
    std::uint32_t count) {
  std::vector<mi::sdk::FriendRequestEntry> out;
  if (!entries || count == 0) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    mi::sdk::FriendRequestEntry e;
    if (entries[i].requester_username) {
      e.requester_username = entries[i].requester_username;
    }
    if (entries[i].requester_remark) {
      e.requester_remark = entries[i].requester_remark;
    }
    out.push_back(std::move(e));
  }
  return out;
}

std::vector<std::string> read_group_call_members(
    const mi_group_call_member_t* entries,
    std::uint32_t count) {
  std::vector<std::string> out;
  if (!entries || count == 0) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    if (entries[i].username) {
      out.emplace_back(entries[i].username);
    }
  }
  return out;
}

QString resolve_local_file_path(const QString& urlOrPath) {
  const QString trimmed = urlOrPath.trimmed();
  if (trimmed.startsWith(QStringLiteral("file:"))) {
    return QUrl(trimmed).toLocalFile();
  }
  return trimmed;
}

QString resolve_ui_data_dir() {
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

QString sanitize_file_stem(QString name) {
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

int clamp_enhance_scale(int scale) {
  return scale == kAiEnhanceScaleX4 ? kAiEnhanceScaleX4 : kAiEnhanceScaleX2;
}

int resolve_enhance_scale(int requestedScale, bool x4Confirmed) {
  const int clamped = clamp_enhance_scale(requestedScale);
  if (clamped == kAiEnhanceScaleX4 && !x4Confirmed) {
    return kAiEnhanceScaleX2;
  }
  return clamped;
}

QString ai_settings_path() {
  const QString dataDir = resolve_ui_data_dir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("ai_settings.ini"));
}

QString privacy_settings_path() {
  const QString dataDir = resolve_ui_data_dir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("privacy_settings.ini"));
}

QString chat_backgrounds_path() {
  const QString dataDir = resolve_ui_data_dir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("chat_backgrounds.ini"));
}

QString chat_backgrounds_dir() {
  const QString dataDir = resolve_ui_data_dir();
  if (dataDir.isEmpty()) {
    return {};
  }
  return QDir(dataDir).filePath(QStringLiteral("chat_backgrounds"));
}

struct AiEnhanceRecommendation {
  int perf_scale{kAiEnhanceScaleX2};
  int quality_scale{kAiEnhanceScaleX2};
};

AiEnhanceRecommendation build_ai_enhance_recommendation(int gpuSeries,
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

QString run_command_output(const QString& program,
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

std::uint64_t now_monotonic_ms() {
  return mi::platform::NowSteadyMs();
}

QStringList parse_gpu_names(const QString& output) {
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

QString pick_preferred_gpu_name(const QStringList& names) {
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

QString query_gpu_name() {
#ifdef _WIN32
  const QString wmicOutput = run_command_output(
      QStringLiteral("wmic"),
      {QStringLiteral("path"),
       QStringLiteral("win32_VideoController"),
       QStringLiteral("get"),
       QStringLiteral("Name")},
      2000);
  QStringList names = parse_gpu_names(wmicOutput);
  if (names.isEmpty()) {
    const QString psOutput = run_command_output(
        QStringLiteral("powershell"),
        {QStringLiteral("-NoProfile"),
         QStringLiteral("-Command"),
         QStringLiteral("Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name")},
        2500);
    names = parse_gpu_names(psOutput);
  }
  return pick_preferred_gpu_name(names);
#else
  return {};
#endif
}

int parse_nvidia_series(const QString& gpuName) {
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

QString find_real_esrgan_path(bool* supportsGpu) {
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

bool detect_ai_enhance_gpu_available() {
  bool gpuSupported = false;
  const QString exe = find_real_esrgan_path(&gpuSupported);
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

int run_process_quietly(const QString& program, const QStringList& args) {
  QProcess process;
  process.setProgram(program);
  process.setArguments(args);
  process.setProcessChannelMode(QProcess::SeparateChannels);
  process.start();
  if (!process.waitForStarted()) {
    return -1;
  }
  while (!process.waitForFinished(250)) {
    process.readAllStandardOutput();
    process.readAllStandardError();
  }
  process.readAllStandardOutput();
  process.readAllStandardError();
  return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

int run_real_esrgan_quietly(const QString& exe, const QStringList& args) {
  return run_process_quietly(exe, args);
}

QString find_real_esrgan_model_dir(const QString& exePath,
                               const QString& modelName) {
  const QString trimmedModel = modelName.trimmed();
  if (trimmedModel.isEmpty()) {
    return {};
  }
  const QString dataDir = resolve_ui_data_dir();
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

QString select_real_esrgan_model_name(int scale, bool anime) {
  const int clamped = clamp_enhance_scale(scale);
  if (clamped == kAiEnhanceScaleX2) {
    return QStringLiteral("realesrgan-x2plus");
  }
  if (anime) {
    return QStringLiteral("realesrgan-x4plus-anime");
  }
  return QStringLiteral("realesrgan-x4plus");
}

bool load_ai_enhance_settings(bool gpuAvailable,
                           const AiEnhanceRecommendation& rec,
                           bool& enabled,
                           int& quality,
                           bool& x4Confirmed) {
  const QString path = ai_settings_path();
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
  quality = clamp_enhance_scale(quality);
  if (!hasConfirm && quality == kAiEnhanceScaleX4) {
    x4Confirmed = true;
  }
  return true;
}

void save_ai_enhance_settings(bool enabled, int quality, bool x4Confirmed) {
  const QString path = ai_settings_path();
  if (path.isEmpty()) {
    return;
  }
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(QStringLiteral("ai/enabled"), enabled);
  settings.setValue(QStringLiteral("ai/quality"), clamp_enhance_scale(quality));
  settings.setValue(QStringLiteral("ai/x4_confirmed"), x4Confirmed);
  settings.sync();
}

bool load_privacy_settings(bool& historySaveEnabled) {
  const QString path = privacy_settings_path();
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    historySaveEnabled = false;
    return false;
  }
  QSettings settings(path, QSettings::IniFormat);
  historySaveEnabled =
      settings.value(QStringLiteral("privacy/save_history"), false).toBool();
  return true;
}

void save_privacy_settings(bool historySaveEnabled) {
  const QString path = privacy_settings_path();
  if (path.isEmpty()) {
    return;
  }
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(QStringLiteral("privacy/save_history"), historySaveEnabled);
  settings.sync();
}

void load_chat_backgrounds(QHash<QString, QString>& out) {
  out.clear();
  const QString path = chat_backgrounds_path();
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return;
  }
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("backgrounds"));
  const QStringList keys = settings.childKeys();
  for (const auto& key : keys) {
    const QString value = settings.value(key).toString();
    if (!value.isEmpty()) {
      out.insert(key, value);
    }
  }
  settings.endGroup();
}

void save_chat_backgrounds(const QHash<QString, QString>& in) {
  const QString path = chat_backgrounds_path();
  if (path.isEmpty()) {
    return;
  }
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("backgrounds"));
  settings.remove(QStringLiteral(""));
  for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
    settings.setValue(it.key(), it.value());
  }
  settings.endGroup();
  settings.sync();
}

QString chat_background_key(const QString& username, const QString& chatId) {
  const QString trimmed = chatId.trimmed();
  if (username.isEmpty()) {
    return trimmed;
  }
  return username + QStringLiteral(":") + trimmed;
}

bool is_session_invalid_error(const QString& message) {
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

bool load_pinyin_dict_from_resource(const char* resourcePath,
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

PinyinIndex build_pinyin_index() {
  PinyinIndex index;
  load_pinyin_dict_from_resource(kPinyinDictResourcePath, index.dict,
                             &index.maxKeyLength);
  load_pinyin_dict_from_resource(kPinyinAbbrDictResourcePath, index.abbrDict,
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

const PinyinIndex& get_pinyin_index() {
  static PinyinIndex index = build_pinyin_index();
  return index;
}

void append_candidate(QStringList& list, const QString& candidate, int limit) {
  if (candidate.isEmpty() || list.contains(candidate)) {
    return;
  }
  list.push_back(candidate);
  if (limit > 0 && list.size() > limit) {
    list.removeLast();
  }
}

bool is_audio_format_supported(const QAudioDevice& device,
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

bool pick_preferred_audio_format(const QAudioDevice& device,
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

bool find_candidate_audio_format(const QAudioDevice& inDevice,
                              const QAudioDevice& outDevice,
                              bool checkIn,
                              bool checkOut,
                              int& sampleRate,
                              int& channels) {
  const std::array<int, 5> rates = {48000, 44100, 32000, 24000, 16000};
  const std::array<int, 2> chans = {1, 2};
  for (const int rate : rates) {
    for (const int ch : chans) {
      if (checkIn && !is_audio_format_supported(inDevice, rate, ch)) {
        continue;
      }
      if (checkOut && !is_audio_format_supported(outDevice, rate, ch)) {
        continue;
      }
      sampleRate = rate;
      channels = ch;
      return true;
    }
  }
  return false;
}

void adjust_audio_config_for_devices(
    const QAudioDevice& inDevice,
    const QAudioDevice& outDevice,
    mi::client::media::AudioPipelineConfig& config) {
  const bool haveIn = !inDevice.isNull();
  const bool haveOut = !outDevice.isNull();
  if (!haveIn && !haveOut) {
    return;
  }
  const bool inOk =
      !haveIn || is_audio_format_supported(inDevice, config.sample_rate,
                                        config.channels);
  const bool outOk =
      !haveOut || is_audio_format_supported(outDevice, config.sample_rate,
                                         config.channels);
  if (inOk && outOk) {
    return;
  }

  int rate = config.sample_rate;
  int ch = config.channels;
  if (haveIn && haveOut) {
    if (find_candidate_audio_format(inDevice, outDevice, true, true, rate, ch)) {
      config.sample_rate = rate;
      config.channels = ch;
      return;
    }
    int prefRate = 0;
    int prefCh = 0;
    if (pick_preferred_audio_format(inDevice, prefRate, prefCh) &&
        is_audio_format_supported(outDevice, prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
    if (pick_preferred_audio_format(outDevice, prefRate, prefCh) &&
        is_audio_format_supported(inDevice, prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
  if (haveIn) {
    int prefRate = 0;
    int prefCh = 0;
    if (pick_preferred_audio_format(inDevice, prefRate, prefCh) ||
        find_candidate_audio_format(inDevice, outDevice, true, false,
                                 prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
  if (haveOut) {
    int prefRate = 0;
    int prefCh = 0;
    if (pick_preferred_audio_format(outDevice, prefRate, prefCh) ||
        find_candidate_audio_format(inDevice, outDevice, false, true,
                                 prefRate, prefCh)) {
      config.sample_rate = prefRate;
      config.channels = prefCh;
      return;
    }
  }
}

QString segment_fallback(const QString& pinyin) {
  const auto& index = get_pinyin_index();
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

QStringList build_pinyin_candidates(const QString& pinyin, int limit) {
  const auto& index = get_pinyin_index();
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
        append_candidate(list, cand, limit);
        if (limit > 0 && list.size() >= limit) {
          break;
        }
      }
    }
  }
  const QString fallback = segment_fallback(pinyin);
  if (!fallback.isEmpty()) {
    append_candidate(list, fallback, limit);
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
      append_candidate(list, hit.value().front(), limit);
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
      append_candidate(list, hit.value().front(), limit);
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

QString find_config_file(const QString& name) {
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

QString now_time_string() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

struct CallInvite {
  bool ok{false};
  bool video{false};
  QString callId;
};

CallInvite parse_call_invite(const QString& text) {
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

QString parse_call_end_id(const QString& text) {
  if (!text.startsWith(QString::fromLatin1(kCallEndPrefix))) {
    return {};
  }
  return text.mid(static_cast<int>(std::strlen(kCallEndPrefix))).trimmed();
}

QString parse_recall_target_id(const QString& text) {
  if (!text.startsWith(QString::fromLatin1(kRecallPrefix))) {
    return {};
  }
  return text.mid(static_cast<int>(std::strlen(kRecallPrefix))).trimmed();
}

QString format_coord_e7(std::int32_t v_e7) {
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

QString format_location_text(double lat, double lon, const QString& label) {
  const auto lat_e7 = static_cast<std::int32_t>(std::llround(lat * 10000000.0));
  const auto lon_e7 = static_cast<std::int32_t>(std::llround(lon * 10000000.0));
  const QString safeLabel =
      label.trimmed().isEmpty() ? QStringLiteral("（未命名）") : label.trimmed();
  return QStringLiteral("【位置】%1\nlat:%2, lon:%3")
      .arg(safeLabel, format_coord_e7(lat_e7), format_coord_e7(lon_e7));
}

QString sanitize_file_id(const QString& fileId) {
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

QString sanitize_download_file_name(const QString& fileName,
                                    const QString& fallbackName) {
  QString name = QFileInfo(fileName.trimmed()).fileName().trimmed();
  if (name.isEmpty() || name == QStringLiteral(".") ||
      name == QStringLiteral("..")) {
    name = QFileInfo(fallbackName.trimmed()).fileName().trimmed();
  }
  if (name.isEmpty() || name == QStringLiteral(".") ||
      name == QStringLiteral("..")) {
    name = QStringLiteral("download.bin");
  }
  QString out;
  out.reserve(name.size());
  for (const QChar ch : name) {
    const ushort u = ch.unicode();
    if (u < 0x20 || ch == QLatin1Char('<') || ch == QLatin1Char('>') ||
        ch == QLatin1Char(':') || ch == QLatin1Char('"') ||
        ch == QLatin1Char('/') || ch == QLatin1Char('\\') ||
        ch == QLatin1Char('|') || ch == QLatin1Char('?') ||
        ch == QLatin1Char('*')) {
      out.append(QLatin1Char('_'));
    } else {
      out.append(ch);
    }
  }
  out = out.trimmed();
  if (out.isEmpty() || out == QStringLiteral(".") ||
      out == QStringLiteral("..")) {
    out = QStringLiteral("download.bin");
  }
  if (out.size() > 128) {
    const QString suffix = QFileInfo(out).suffix();
    const int suffixExtra = suffix.isEmpty() ? 0 : suffix.size() + 1;
    out = out.left(std::max(1, 128 - suffixExtra));
    if (!suffix.isEmpty()) {
      out += QStringLiteral(".") + suffix.left(32);
    }
  }
  return out;
}

QString resolve_ai_upscale_dir() {
  const QString tempRoot =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  if (tempRoot.isEmpty()) {
    return {};
  }
  const QString scoped =
      QStringLiteral("mi_e2ee_ai_upscale/%1")
          .arg(QString::number(QCoreApplication::applicationPid()));
  return QDir(tempRoot).filePath(scoped);
}

bool ensure_ai_upscale_dir(QDir& outDir, QString& error) {
  const QString dirPath = resolve_ai_upscale_dir();
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

QString build_enhanced_image_path(const QString& messageId,
                               int scale,
                               QString& error) {
  const QString trimmed = messageId.trimmed();
  if (trimmed.isEmpty()) {
    error = QStringLiteral("图片标识无效");
    return {};
  }
  QDir outDir;
  if (!ensure_ai_upscale_dir(outDir, error)) {
    return {};
  }
  const QString token = sanitize_file_id(trimmed);
  if (token.isEmpty()) {
    error = QStringLiteral("图片标识无效");
    return {};
  }
  const int clamped = clamp_enhance_scale(scale);
  return outDir.filePath(
      QStringLiteral("msg_%1_x%2.png").arg(token).arg(clamped));
}

QString enhanced_image_path_if_exists(const QString& messageId) {
  QString error;
  const std::array<int, 2> scales = {kAiEnhanceScaleX4, kAiEnhanceScaleX2};
  for (const int scale : scales) {
    const QString path = build_enhanced_image_path(messageId, scale, error);
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

ImageQualityMetrics analyze_image_quality(const QString& path) {
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

bool should_auto_enhance_image(const QString& path) {
  const ImageQualityMetrics metrics = analyze_image_quality(path);
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

bool is_video_ext(const QString& ext) {
  const QString e = ext.toLower();
  return e == QStringLiteral("mp4") || e == QStringLiteral("mov") ||
         e == QStringLiteral("mkv") || e == QStringLiteral("webm") ||
         e == QStringLiteral("avi");
}

bool is_image_ext(const QString& ext) {
  const QString e = ext.toLower();
  return e == QStringLiteral("png") || e == QStringLiteral("jpg") ||
         e == QStringLiteral("jpeg") || e == QStringLiteral("webp") ||
         e == QStringLiteral("bmp");
}

bool is_gif_ext(const QString& ext) {
  return ext.toLower() == QStringLiteral("gif");
}

bool is_already_compressed_ext(const QString& ext) {
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

CachePolicy select_cache_policy(quint64 fileSize) {
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

QString cache_chunk_name(int index) {
  return QStringLiteral("chunk_%1.bin").arg(index, 8, 10, QLatin1Char('0'));
}

QString cache_index_path(const QDir& dir) {
  return dir.filePath(QStringLiteral("cache.idx"));
}

bool read_cache_index(const QString& path, CacheIndex& out, QString& error) {
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

bool write_cache_index(const QString& path, const CacheIndex& index, QString& error) {
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

bool cache_chunks_ready(const QDir& dir, const CacheIndex& index) {
  if (index.chunkCount == 0) {
    return index.fileSize == 0;
  }
  for (quint32 i = 0; i < index.chunkCount; ++i) {
    if (!QFileInfo::exists(dir.filePath(cache_chunk_name(static_cast<int>(i))))) {
      return false;
    }
  }
  return true;
}

bool write_chunk_file(const QString& path,
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

bool compress_chunk(const QByteArray& input,
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

bool build_chunked_cache(const QString& sourcePath,
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
    if (!compress_chunk(chunk, policy, payload, method)) {
      error = QStringLiteral("cache compress failed");
      return false;
    }
    const QString chunkPath =
        dir.filePath(cache_chunk_name(static_cast<int>(outChunkCount)));
    if (!write_chunk_file(chunkPath,
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

bool ensure_cache_root_dir(QDir& out, QString& error) {
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

bool copy_file_to_path(const QString& src,
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

bool read_chunk_file(const QString& path,
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

bool decompress_chunk(CacheChunkMethod method,
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

bool restore_chunked_cache(const QDir& dir,
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
    const QString chunkPath = dir.filePath(cache_chunk_name(static_cast<int>(i)));
    CacheChunkMethod method = CacheChunkMethod::kRaw;
    quint32 rawSize = 0;
    QByteArray payload;
    if (!read_chunk_file(chunkPath, method, rawSize, payload, error)) {
      out.close();
      QFile::remove(destPath);
      return false;
    }
    QByteArray plain;
    if (!decompress_chunk(method, payload, rawSize, plain, error)) {
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

QString find_ffmpeg_path();

struct ProgressAdapter {
  std::function<void(double)> onProgress;
};

void progress_thunk(std::uint64_t done,
                   std::uint64_t total,
                   void* user_data) {
  if (!user_data || total == 0) {
    return;
  }
  auto* adapter = static_cast<ProgressAdapter*>(user_data);
  if (!adapter->onProgress) {
    return;
  }
  adapter->onProgress(static_cast<double>(done) /
                      static_cast<double>(total));
}

CacheTaskResult build_attachment_cache(
    mi_client_handle* c_api,
    const QString& fileId,
    const std::array<std::uint8_t, 32>& fileKey,
    const QString& fileName,
    qint64 fileSize,
    const std::function<void(double)>& onProgress) {
  CacheTaskResult result;
  QDir cacheRoot;
  QString error;
  if (!ensure_cache_root_dir(cacheRoot, error)) {
    result.error = error;
    return result;
  }

  const QString safeId = sanitize_file_id(fileId);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = is_image_ext(ext) || is_gif_ext(ext) || is_video_ext(ext);
  auto downloadToPath = [&](const QString& path) -> bool {
    if (!c_api) {
      result.error = QStringLiteral("未初始化");
      return false;
    }
    const QByteArray pathUtf8 = path.toUtf8();
    const QByteArray nameUtf8 = fileName.toUtf8();
    const char* namePtr = nameUtf8.isEmpty() ? nullptr : nameUtf8.constData();
    ProgressAdapter adapter{onProgress};
    const mi_progress_callback_t cb = onProgress ? progress_thunk : nullptr;
    void* userData = onProgress ? &adapter : nullptr;
    const std::uint64_t sizeValue =
        fileSize > 0 ? static_cast<std::uint64_t>(fileSize) : 0;
    const bool ok = mi_client_download_chat_file_to_path(
                        c_api, fileId.toStdString().c_str(), fileKey.data(),
                        static_cast<std::uint32_t>(fileKey.size()), namePtr,
                        sizeValue, pathUtf8.constData(), 1, cb, userData) != 0;
    if (!ok) {
      const char* apiErr = mi_client_last_error(c_api);
      if (apiErr && *apiErr) {
        result.error = QString::fromUtf8(apiErr);
      }
    }
    return ok;
  };

  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    const QString previewPath =
        cacheRoot.filePath(safeId + QStringLiteral(".preview.jpg"));
    if (!QFileInfo::exists(filePath)) {
      if (!downloadToPath(filePath)) {
        return result;
      }
    }
    result.fileUrl = filePath;
    if (is_video_ext(ext)) {
      if (!QFileInfo::exists(previewPath)) {
        const QString ffmpeg = find_ffmpeg_path();
        if (!ffmpeg.isEmpty()) {
          QStringList args;
          args << QStringLiteral("-y")
               << QStringLiteral("-ss") << QStringLiteral("0.2")
               << QStringLiteral("-i") << filePath
               << QStringLiteral("-frames:v") << QStringLiteral("1")
               << QStringLiteral("-vf") << QStringLiteral("scale=480:-1")
               << previewPath;
          (void)run_process_quietly(ffmpeg, args);
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

  const QString indexPath = cache_index_path(fileDir);
  CacheIndex existing;
  if (QFileInfo::exists(indexPath)) {
    if (read_cache_index(indexPath, existing, error) &&
        cache_chunks_ready(fileDir, existing)) {
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

  if (!downloadToPath(tempPath)) {
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
  CachePolicy policy = select_cache_policy(tempSize);
  if (is_already_compressed_ext(ext)) {
    policy.forceRaw = true;
  }
  if (!build_chunked_cache(tempPath, policy, fileDir, actualSize, chunkCount,
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
  if (!write_cache_index(indexPath, index, error)) {
    result.error = error;
    return result;
  }

  result.ok = true;
  return result;
}

bool restore_attachment_from_cache(const QString& fileId,
                                const QString& fileName,
                                const QString& savePath,
                                const std::function<void(double)>& onProgress,
                                QString& error) {
  QDir cacheRoot;
  if (!ensure_cache_root_dir(cacheRoot, error)) {
    return false;
  }
  const QString safeId = sanitize_file_id(fileId);
  QString ext = QFileInfo(fileName).suffix().toLower();
  if (ext.isEmpty()) {
    ext = QStringLiteral("bin");
  }
  const bool isMedia = is_image_ext(ext) || is_gif_ext(ext) || is_video_ext(ext);
  if (isMedia) {
    const QString filePath =
        cacheRoot.filePath(safeId + QStringLiteral(".") + ext);
    if (!QFileInfo::exists(filePath)) {
      error = QStringLiteral("cache missing");
      return false;
    }
    if (!copy_file_to_path(filePath, savePath, error)) {
      return false;
    }
    if (onProgress) {
      onProgress(1.0);
    }
    return true;
  }

  const QDir fileDir(cacheRoot.filePath(safeId));
  const QString indexPath = cache_index_path(fileDir);
  CacheIndex index;
  if (!QFileInfo::exists(indexPath) ||
      !read_cache_index(indexPath, index, error)) {
    error = QStringLiteral("cache missing");
    return false;
  }
  if ((index.flags & kCacheFlagKeepRaw) && !index.rawName.isEmpty()) {
    const QString rawPath = fileDir.filePath(index.rawName);
    if (QFileInfo::exists(rawPath)) {
      if (!copy_file_to_path(rawPath, savePath, error)) {
        return false;
      }
      if (onProgress) {
        onProgress(1.0);
      }
      return true;
    }
  }
  if (!cache_chunks_ready(fileDir, index)) {
    error = QStringLiteral("cache missing");
    return false;
  }
  return restore_chunked_cache(fileDir, index, savePath, onProgress, error);
}

QString find_ffmpeg_path() {
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

QVideoFrame make_nv12_frame(std::vector<std::uint8_t>&& data,
                          std::uint32_t width,
                          std::uint32_t height,
                          std::uint32_t stride,
                          std::uint64_t timestamp_ms) {
  if (data.empty() || width == 0 || height == 0 || stride < width) {
    return {};
  }
  const auto source_stride = static_cast<std::size_t>(stride);
  const auto source_height = static_cast<std::size_t>(height);
  const std::size_t y_bytes = source_stride * source_height;
  if (data.size() < y_bytes) {
    return {};
  }
  QVideoFrameFormat format(QSize(static_cast<int>(width),
                                 static_cast<int>(height)),
                           QVideoFrameFormat::Format_NV12);
  QVideoFrame frame(format);
  if (!frame.map(QVideoFrame::WriteOnly)) {
    return {};
  }
  if (frame.planeCount() < 2 || !frame.bits(0) || !frame.bits(1)) {
    frame.unmap();
    return {};
  }
  const int y_line = frame.bytesPerLine(0);
  const int uv_line = frame.bytesPerLine(1);
  if (y_line <= 0 || uv_line <= 0) {
    frame.unmap();
    return {};
  }
  const auto copy_width =
      static_cast<std::size_t>(std::min<std::uint32_t>(width, stride));
  auto* y_dst = frame.bits(0);
  auto* uv_dst = frame.bits(1);
  const auto* y_src = data.data();
  const auto* uv_src = data.data() + y_bytes;
  for (std::size_t row = 0; row < source_height; ++row) {
    std::memcpy(y_dst + row * static_cast<std::size_t>(y_line),
                y_src + row * source_stride,
                std::min(copy_width, static_cast<std::size_t>(y_line)));
  }
  const std::size_t uv_rows = source_height / 2;
  const std::size_t available_uv_rows = (data.size() - y_bytes) / source_stride;
  const std::size_t rows = std::min(uv_rows, available_uv_rows);
  for (std::size_t row = 0; row < rows; ++row) {
    std::memcpy(uv_dst + row * static_cast<std::size_t>(uv_line),
                uv_src + row * source_stride,
                std::min(copy_width, static_cast<std::size_t>(uv_line)));
  }
  frame.unmap();
  frame.setStartTime(static_cast<qint64>(timestamp_ms));
  return frame;
}

}  // namespace
