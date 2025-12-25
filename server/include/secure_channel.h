#ifndef MI_E2EE_SERVER_SECURE_CHANNEL_H
#define MI_E2EE_SERVER_SECURE_CHANNEL_H

#include <array>
#include <cstdint>
#include <vector>

#include "frame.h"
#include "pake.h"

namespace mi::server {

enum class SecureChannelRole : std::uint8_t { kClient = 0, kServer = 1 };

class SecureChannel {
 public:
  SecureChannel() = default;

  explicit SecureChannel(const DerivedKeys& keys, SecureChannelRole role);

  bool Encrypt(std::uint64_t seq,
               FrameType frame_type,
               const std::vector<std::uint8_t>& plaintext,
               std::vector<std::uint8_t>& out);

  bool Decrypt(const std::vector<std::uint8_t>& input,
               FrameType frame_type,
               std::vector<std::uint8_t>& out_plain);
  bool Decrypt(const std::uint8_t* input, std::size_t len,
               FrameType frame_type, std::vector<std::uint8_t>& out_plain);

 private:
  bool CanAcceptSeq(std::uint64_t seq) const;
  void MarkSeqReceived(std::uint64_t seq);

  std::array<std::uint8_t, 32> tx_key_{};
  std::array<std::uint8_t, 32> rx_key_{};

  bool recv_inited_{false};
  std::uint64_t recv_max_seq_{0};
  std::uint64_t recv_window_{0};
};

}  // namespace mi::server

#endif  // MI_E2EE_SERVER_SECURE_CHANNEL_H
