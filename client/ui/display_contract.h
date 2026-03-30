#ifndef MI_E2EE_CLIENT_UI_DISPLAY_CONTRACT_H
#define MI_E2EE_CLIENT_UI_DISPLAY_CONTRACT_H

#include <QVariantList>
#include <QString>

namespace mi::client::ui::display_contract {

struct GatewayDisplayInfo {
  QString state;
  QString detail;
};

GatewayDisplayInfo BuildGatewayDisplayInfo(const QString& configPath);
QString MaskedDeviceId(const QString& rawId);
QString LocalizedLastSeen(int lastSeenSec);

QVariantList BuildDeviceDisplayList(const QVariantList& rawDevices,
                                    const QString& currentDeviceId,
                                    const QString& currentDeviceDisplayId);

}  // namespace mi::client::ui::display_contract

#endif  // MI_E2EE_CLIENT_UI_DISPLAY_CONTRACT_H
