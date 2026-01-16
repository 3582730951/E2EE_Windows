#include "client_core.h"

#include "sync_service.h"

namespace mi::client {

bool ClientCore::LoadDeviceSyncKey() {
  return SyncService().LoadDeviceSyncKey(*this);
}

bool ClientCore::StoreDeviceSyncKey(const std::array<std::uint8_t, 32>& key) {
  return SyncService().StoreDeviceSyncKey(*this, key);
}

bool ClientCore::EncryptDeviceSync(const std::vector<std::uint8_t>& plaintext,
                                   std::vector<std::uint8_t>& out_cipher) {
  return SyncService().EncryptDeviceSync(*this, plaintext, out_cipher);
}

bool ClientCore::DecryptDeviceSync(const std::vector<std::uint8_t>& cipher,
                                   std::vector<std::uint8_t>& out_plaintext) {
  return SyncService().DecryptDeviceSync(*this, cipher, out_plaintext);
}

bool ClientCore::PushDeviceSyncCiphertext(const std::vector<std::uint8_t>& cipher) {
  return SyncService().PushDeviceSyncCiphertext(*this, cipher);
}

std::vector<std::vector<std::uint8_t>> ClientCore::PullDeviceSyncCiphertexts() {
  return SyncService().PullDeviceSyncCiphertexts(*this);
}

bool ClientCore::BeginDevicePairingPrimary(std::string& out_pairing_code) {
  return SyncService().BeginDevicePairingPrimary(*this, out_pairing_code);
}

std::vector<ClientCore::DevicePairingRequest> ClientCore::PollDevicePairingRequests() {
  return SyncService().PollDevicePairingRequests(*this);
}

bool ClientCore::ApproveDevicePairingRequest(const DevicePairingRequest& request) {
  return SyncService().ApproveDevicePairingRequest(*this, request);
}

bool ClientCore::BeginDevicePairingLinked(const std::string& pairing_code) {
  return SyncService().BeginDevicePairingLinked(*this, pairing_code);
}

bool ClientCore::PollDevicePairingLinked(bool& out_completed) {
  return SyncService().PollDevicePairingLinked(*this, out_completed);
}

void ClientCore::CancelDevicePairing() {
  SyncService().CancelDevicePairing(*this);
}

}  // namespace mi::client
