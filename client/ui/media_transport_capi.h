#ifndef MI_E2EE_CLIENT_UI_MEDIA_TRANSPORT_CAPI_H
#define MI_E2EE_CLIENT_UI_MEDIA_TRANSPORT_CAPI_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "c_api_client.h"
#include "media_transport.h"

namespace mi::client::ui {

class CapiMediaTransport : public mi::client::media::MediaTransport {
 public:
  explicit CapiMediaTransport(mi_client_handle* handle) : handle_(handle) {}

  bool DeriveMediaRoot(const std::string& peer_username,
                       const std::array<std::uint8_t, 16>& call_id,
                       std::array<std::uint8_t, 32>& out_media_root,
                       std::string& out_error) override {
    out_error.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (!mi_client_derive_media_root(handle_, peer_username.c_str(),
                                     call_id.data(),
                                     static_cast<std::uint32_t>(call_id.size()),
                                     out_media_root.data(),
                                     static_cast<std::uint32_t>(
                                         out_media_root.size()))) {
      const char* err = mi_client_last_error(handle_);
      out_error = (err && *err != '\0') ? err : "media root derive failed";
      return false;
    }
    return true;
  }

  bool PushMedia(const std::string& peer_username,
                 const std::array<std::uint8_t, 16>& call_id,
                 const std::vector<std::uint8_t>& packet,
                 std::string& out_error) override {
    out_error.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (!mi_client_push_media(handle_, peer_username.c_str(), call_id.data(),
                              static_cast<std::uint32_t>(call_id.size()),
                              packet.data(),
                              static_cast<std::uint32_t>(packet.size()))) {
      const char* err = mi_client_last_error(handle_);
      out_error = (err && *err != '\0') ? err : "media push failed";
      return false;
    }
    return true;
  }

  bool PullMedia(const std::array<std::uint8_t, 16>& call_id,
                 std::uint32_t max_packets,
                 std::uint32_t wait_ms,
                 std::vector<mi::client::media::MediaRelayPacket>& out_packets,
                 std::string& out_error) override {
    out_error.clear();
    out_packets.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (max_packets == 0) {
      return true;
    }
    if (pull_buffer_.size() < max_packets) {
      pull_buffer_.resize(max_packets);
    }
    const std::uint32_t count =
        mi_client_pull_media(handle_, call_id.data(),
                             static_cast<std::uint32_t>(call_id.size()),
                             max_packets, wait_ms, pull_buffer_.data());
    if (count == 0) {
      const char* err = mi_client_last_error(handle_);
      if (err && *err != '\0') {
        out_error = err;
        return false;
      }
      return true;
    }
    out_packets.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      mi::client::media::MediaRelayPacket packet;
      const auto& entry = pull_buffer_[i];
      if (entry.sender) {
        packet.sender = entry.sender;
      }
      if (entry.payload && entry.payload_len > 0) {
        packet.payload.assign(entry.payload,
                              entry.payload + entry.payload_len);
      }
      out_packets.push_back(std::move(packet));
    }
    return true;
  }

  bool PushGroupMedia(const std::string& group_id,
                      const std::array<std::uint8_t, 16>& call_id,
                      const std::vector<std::uint8_t>& packet,
                      std::string& out_error) override {
    out_error.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (!mi_client_push_group_media(
            handle_, group_id.c_str(), call_id.data(),
            static_cast<std::uint32_t>(call_id.size()), packet.data(),
            static_cast<std::uint32_t>(packet.size()))) {
      const char* err = mi_client_last_error(handle_);
      out_error = (err && *err != '\0') ? err : "group media push failed";
      return false;
    }
    return true;
  }

  bool PullGroupMedia(
      const std::array<std::uint8_t, 16>& call_id,
      std::uint32_t max_packets,
      std::uint32_t wait_ms,
      std::vector<mi::client::media::MediaRelayPacket>& out_packets,
      std::string& out_error) override {
    out_error.clear();
    out_packets.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (max_packets == 0) {
      return true;
    }
    if (group_pull_buffer_.size() < max_packets) {
      group_pull_buffer_.resize(max_packets);
    }
    const std::uint32_t count =
        mi_client_pull_group_media(handle_, call_id.data(),
                                   static_cast<std::uint32_t>(call_id.size()),
                                   max_packets, wait_ms, group_pull_buffer_.data());
    if (count == 0) {
      const char* err = mi_client_last_error(handle_);
      if (err && *err != '\0') {
        out_error = err;
        return false;
      }
      return true;
    }
    out_packets.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      mi::client::media::MediaRelayPacket packet;
      const auto& entry = group_pull_buffer_[i];
      if (entry.sender) {
        packet.sender = entry.sender;
      }
      if (entry.payload && entry.payload_len > 0) {
        packet.payload.assign(entry.payload,
                              entry.payload + entry.payload_len);
      }
      out_packets.push_back(std::move(packet));
    }
    return true;
  }

  bool GetGroupCallKey(const std::string& group_id,
                       const std::array<std::uint8_t, 16>& call_id,
                       std::uint32_t key_id,
                       std::array<std::uint8_t, 32>& out_key,
                       std::string& out_error) override {
    out_error.clear();
    if (!handle_) {
      out_error = "invalid handle";
      return false;
    }
    if (!mi_client_get_group_call_key(
            handle_, group_id.c_str(), call_id.data(),
            static_cast<std::uint32_t>(call_id.size()), key_id, out_key.data(),
            static_cast<std::uint32_t>(out_key.size()))) {
      const char* err = mi_client_last_error(handle_);
      out_error = (err && *err != '\0') ? err : "call key missing";
      return false;
    }
    return true;
  }

 private:
  mi_client_handle* handle_{nullptr};
  std::vector<mi_media_packet_t> pull_buffer_{};
  std::vector<mi_media_packet_t> group_pull_buffer_{};
};

}  // namespace mi::client::ui

#endif  // MI_E2EE_CLIENT_UI_MEDIA_TRANSPORT_CAPI_H
