#include "display_contract.h"

#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace mi::client::ui::display_contract {

namespace {

bool UseZhCnStrings() {
  QSettings settings;
  settings.beginGroup(QStringLiteral("ui_i18n"));
  QString mode = settings.value(QStringLiteral("localeMode"),
                                QStringLiteral("system")).toString().trimmed();
  settings.endGroup();
  if (mode != QStringLiteral("zh-CN") && mode != QStringLiteral("en-US")) {
    const QString localeName = QLocale::system().name().replace(QLatin1Char('_'),
                                                                QLatin1Char('-'));
    mode = localeName.startsWith(QStringLiteral("zh"))
               ? QStringLiteral("zh-CN")
               : QStringLiteral("en-US");
  }
  return mode == QStringLiteral("zh-CN");
}

QString NormalizedHost(const QString& input) {
  QString host = input.trimmed();
  if (host.isEmpty()) {
    return QStringLiteral("localhost");
  }
  host.replace(QLatin1Char('\\'), QLatin1Char(' '));
  host.replace(QLatin1Char('/'), QLatin1Char(' '));
  host = host.simplified();
  return host.isEmpty() ? QStringLiteral("localhost") : host;
}

bool IsLocalHost(const QString& host) {
  const QString lowered = host.trimmed().toLower();
  return lowered.isEmpty() ||
         lowered == QStringLiteral("localhost") ||
         lowered == QStringLiteral("127.0.0.1") ||
         lowered == QStringLiteral("::1");
}

QString GatewayStateFor(const QString& host,
                        bool requirePinnedFingerprint,
                        const QString& pinnedFingerprint,
                        const QString& trustStore) {
  if (IsLocalHost(host)) {
    return QStringLiteral("本地配置");
  }
  if (requirePinnedFingerprint ||
      !pinnedFingerprint.trimmed().isEmpty() ||
      !trustStore.trimmed().isEmpty()) {
    return QStringLiteral("已固定");
  }
  return QStringLiteral("远程接入");
}

QString SanitizedGatewayDetail(const QString& host, int port) {
  QString detail = NormalizedHost(host);
  if (port > 0) {
    detail += QStringLiteral(":%1").arg(port);
  }
  detail.replace(QStringLiteral("config:"), QString());
  detail.replace(QLatin1Char('\\'), QLatin1Char(' '));
  detail.replace(QLatin1Char('/'), QLatin1Char(' '));
  return detail.simplified();
}

QString FormatDuration(int lastSeenSec) {
  if (lastSeenSec <= 0) {
    return UseZhCnStrings() ? QStringLiteral("在线") : QStringLiteral("Online");
  }
  if (lastSeenSec < 60) {
    return UseZhCnStrings()
               ? QStringLiteral("最近在线：%1 秒").arg(lastSeenSec)
               : QStringLiteral("Last seen: %1s").arg(lastSeenSec);
  }
  if (lastSeenSec < 3600) {
    const int minutes = lastSeenSec / 60;
    return UseZhCnStrings()
               ? QStringLiteral("最近在线：%1 分钟").arg(minutes)
               : QStringLiteral("Last seen: %1m").arg(minutes);
  }
  const int hours = lastSeenSec / 3600;
  return UseZhCnStrings()
             ? QStringLiteral("最近在线：%1 小时").arg(hours)
             : QStringLiteral("Last seen: %1h").arg(hours);
}

}  // namespace

GatewayDisplayInfo BuildGatewayDisplayInfo(const QString& configPath) {
  GatewayDisplayInfo out;
  QSettings config(configPath, QSettings::IniFormat);
  const QString host =
      config.value(QStringLiteral("client/server_ip"),
                   config.value(QStringLiteral("server_ip"),
                                QStringLiteral("localhost")))
          .toString();
  const int port =
      config.value(QStringLiteral("client/server_port"),
                   config.value(QStringLiteral("server_port"), 0))
          .toInt();
  const bool requirePinnedFingerprint =
      config.value(QStringLiteral("client/require_pinned_fingerprint"),
                   config.value(QStringLiteral("require_pinned_fingerprint"), 0))
          .toInt() != 0;
  const QString pinnedFingerprint =
      config.value(QStringLiteral("client/pinned_fingerprint"),
                   config.value(QStringLiteral("pinned_fingerprint")))
          .toString();
  const QString trustStore =
      config.value(QStringLiteral("client/trust_store"),
                   config.value(QStringLiteral("trust_store")))
          .toString();

  out.state =
      GatewayStateFor(host, requirePinnedFingerprint, pinnedFingerprint, trustStore);
  out.detail = SanitizedGatewayDetail(host, port);
  return out;
}

QString MaskedDeviceId(const QString& rawId) {
  const QString trimmed = rawId.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }
  if (trimmed.size() <= 8) {
    return trimmed;
  }
  return trimmed.left(4) + QStringLiteral("…") + trimmed.right(4);
}

QString LocalizedLastSeen(int lastSeenSec) { return FormatDuration(lastSeenSec); }

QVariantList BuildDeviceDisplayList(const QVariantList& rawDevices,
                                    const QString& currentDeviceId,
                                    const QString& currentDeviceDisplayId) {
  QVariantList out;
  out.reserve(rawDevices.size());
  for (const QVariant& item : rawDevices) {
    const QVariantMap raw = item.toMap();
    const QString deviceId = raw.value(QStringLiteral("deviceId")).toString();
    const QString deviceDisplayId =
        raw.value(QStringLiteral("deviceDisplayId")).toString();
    const int lastSeenSec = raw.value(QStringLiteral("lastSeenSec")).toInt();
    const bool isCurrent = !currentDeviceId.isEmpty() && deviceId == currentDeviceId;
    const QString copyValue =
        !deviceDisplayId.isEmpty()
            ? deviceDisplayId
            : (isCurrent && !currentDeviceDisplayId.isEmpty() ? currentDeviceDisplayId
                                                              : deviceId);
    const QString maskedDisplay = MaskedDeviceId(copyValue);

    QVariantMap mapped;
    mapped.insert(QStringLiteral("deviceId"), deviceId);
    mapped.insert(QStringLiteral("copyValue"), copyValue);
    mapped.insert(QStringLiteral("displayId"), maskedDisplay);
    mapped.insert(QStringLiteral("maskedDeviceId"), maskedDisplay);
    mapped.insert(QStringLiteral("maskedDeviceDisplayId"), maskedDisplay);
    mapped.insert(QStringLiteral("lastSeenSec"), lastSeenSec);
    mapped.insert(QStringLiteral("lastSeenDisplay"), LocalizedLastSeen(lastSeenSec));
    mapped.insert(QStringLiteral("isCurrent"), isCurrent);
    out.push_back(mapped);
  }
  return out;
}

}  // namespace mi::client::ui::display_contract
