#ifndef MI_E2EE_CLIENT_SYNC_SERVICE_H
#define MI_E2EE_CLIENT_SYNC_SERVICE_H

#include "client_core.h"

namespace mi::client {

class SyncService {
 public:
  bool LoadDeviceSyncKey(ClientCore& core) const;
  bool StoreDeviceSyncKey(ClientCore& core, const std::array<std::uint8_t, 32>& key) const;
  bool EncryptDeviceSync(ClientCore& core, const std::vector<std::uint8_t>& plaintext, std::vector<std::uint8_t>& out_cipher) const;
  bool DecryptDeviceSync(ClientCore& core, const std::vector<std::uint8_t>& cipher, std::vector<std::uint8_t>& out_plaintext) const;
  bool PushDeviceSyncCiphertext(ClientCore& core, const std::vector<std::uint8_t>& cipher) const;
  std::vector<std::vector<std::uint8_t>> PullDeviceSyncCiphertexts(ClientCore& core) const;
  bool BeginDevicePairingPrimary(ClientCore& core, std::string& out_pairing_code) const;
  std::vector<ClientCore::DevicePairingRequest> PollDevicePairingRequests(ClientCore& core) const;
  bool ApproveDevicePairingRequest(ClientCore& core, const ClientCore::DevicePairingRequest& request) const;
  bool BeginDevicePairingLinked(ClientCore& core, const std::string& pairing_code) const;
  bool PollDevicePairingLinked(ClientCore& core, bool& out_completed) const;
  void CancelDevicePairing(ClientCore& core) const;

 private:
  void ApplyDeviceSyncState(ClientCore& core,
                            const std::array<std::uint8_t, 32>& key,
                            std::uint64_t send_counter,
                            std::uint64_t recv_counter) const;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_SYNC_SERVICE_H
