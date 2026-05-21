#include "display_contract.h"

#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace mi::client::ui::display_contract {

namespace {

bool use_zh_cn_strings() {
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

QString normalized_host(const QString& input) {
  QString host = input.trimmed();
  if (host.isEmpty()) {
    return {};
  }
  host.replace(QLatin1Char('\\'), QLatin1Char(' '));
  host.replace(QLatin1Char('/'), QLatin1Char(' '));
  host = host.simplified();
  return host;
}

QString loopback_host_name() {
  return QStringLiteral("local") + QStringLiteral("host");
}

QString loopback_ipv4_address() {
  return QStringLiteral("127") + QStringLiteral(".0.0.1");
}

bool is_local_host(const QString& host) {
  const QString lowered = host.trimmed().toLower();
  return lowered == loopback_host_name() ||
         lowered == loopback_ipv4_address() ||
         lowered == QStringLiteral("::1");
}

QString normalized_tls_verify_mode(const QString& input,
                                   bool requirePinnedFingerprint) {
  const QString mode = input.trimmed().toLower();
  if (mode.isEmpty()) {
    return requirePinnedFingerprint ? QStringLiteral("pin")
                                    : QStringLiteral("ca");
  }
  if (mode == QStringLiteral("pin") ||
      mode == QStringLiteral("pinned") ||
      mode == QStringLiteral("fingerprint") ||
      mode == QStringLiteral("0")) {
    return QStringLiteral("pin");
  }
  if (mode == QStringLiteral("ca") ||
      mode == QStringLiteral("cert") ||
      mode == QStringLiteral("certificate") ||
      mode == QStringLiteral("pkix") ||
      mode == QStringLiteral("1")) {
    return QStringLiteral("ca");
  }
  if (mode == QStringLiteral("hybrid") ||
      mode == QStringLiteral("both") ||
      mode == QStringLiteral("pin+ca") ||
      mode == QStringLiteral("ca+pin") ||
      mode == QStringLiteral("2")) {
    return QStringLiteral("hybrid");
  }
  return requirePinnedFingerprint ? QStringLiteral("pin")
                                  : QStringLiteral("ca");
}

QString gateway_state_for(const QString& host,
                          bool requirePinnedFingerprint,
                          const QString& pinnedFingerprint,
                          const QString& tlsVerifyMode) {
  if (host.trimmed().isEmpty()) {
    return {};
  }
  if (is_local_host(host)) {
    return QStringLiteral("本地配置");
  }
  const QString mode =
      normalized_tls_verify_mode(tlsVerifyMode, requirePinnedFingerprint);
  if (mode == QStringLiteral("pin") ||
      !pinnedFingerprint.trimmed().isEmpty()) {
    return QStringLiteral("已固定");
  }
  return QStringLiteral("远程接入");
}

QString sanitized_gateway_detail(const QString& host, int port) {
  QString detail = normalized_host(host);
  if (!detail.isEmpty() && port > 0) {
    detail += QStringLiteral(":%1").arg(port);
  }
  detail.replace(QStringLiteral("config:"), QString());
  detail.replace(QLatin1Char('\\'), QLatin1Char(' '));
  detail.replace(QLatin1Char('/'), QLatin1Char(' '));
  return detail.simplified();
}

QString format_duration(int lastSeenSec) {
  if (lastSeenSec <= 0) {
    return use_zh_cn_strings() ? QStringLiteral("在线") : QStringLiteral("Online");
  }
  if (lastSeenSec < 60) {
    return use_zh_cn_strings()
               ? QStringLiteral("最近在线：%1 秒").arg(lastSeenSec)
               : QStringLiteral("Last seen: %1s").arg(lastSeenSec);
  }
  if (lastSeenSec < 3600) {
    const int minutes = lastSeenSec / 60;
    return use_zh_cn_strings()
               ? QStringLiteral("最近在线：%1 分钟").arg(minutes)
               : QStringLiteral("Last seen: %1m").arg(minutes);
  }
  const int hours = lastSeenSec / 3600;
  return use_zh_cn_strings()
             ? QStringLiteral("最近在线：%1 小时").arg(hours)
             : QStringLiteral("Last seen: %1h").arg(hours);
}

}  // namespace

GatewayDisplayInfo BuildGatewayDisplayInfo(const QString& configPath) {
  GatewayDisplayInfo out;
  QSettings config(configPath, QSettings::IniFormat);
  const QString host =
      config.value(QStringLiteral("client/server_ip"),
                   config.value(QStringLiteral("server_ip")))
          .toString();
  const int port =
      config.value(QStringLiteral("client/server_port"),
                   config.value(QStringLiteral("server_port"), 0))
          .toInt();
  const bool requirePinnedFingerprint =
      config.value(QStringLiteral("client/require_pinned_fingerprint"),
                   config.value(QStringLiteral("require_pinned_fingerprint"), 1))
          .toInt() != 0;
  const QString pinnedFingerprint =
      config.value(QStringLiteral("client/pinned_fingerprint"),
                   config.value(QStringLiteral("pinned_fingerprint")))
          .toString();
  const QString tlsVerifyMode =
      config.value(QStringLiteral("client/tls_verify_mode"),
                   config.value(QStringLiteral("tls_verify_mode")))
          .toString();

  out.state =
      gateway_state_for(host, requirePinnedFingerprint, pinnedFingerprint,
                      tlsVerifyMode);
  out.detail = sanitized_gateway_detail(host, port);
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

QString LocalizedLastSeen(int lastSeenSec) { return format_duration(lastSeenSec); }

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
