#pragma once

#include "mumble_crypt.h"
#include "mumble_gcm.h"
#include "mumble_varint.h"
#include <WiFiUdp.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace esphome {
namespace mumble {

// Max crypto overhead (Secure GCM) for receive buffer
static constexpr size_t MAX_CRYPTO_OVERHEAD = GCM_OVERHEAD;

class MumbleUdp {
 public:
  static constexpr size_t MAX_PACKET_SIZE = 1024;
  static constexpr uint32_t UDP_PING_INTERVAL_MS = 5000;
  static constexpr uint32_t UDP_PING_TIMEOUT_MS = 5000;

  MumbleUdp() = default;
  ~MumbleUdp() { stop(); }

  bool start(const std::string &server, uint16_t port);
  void stop();

  void loop();

  bool is_active() const { return udp_active_; }
  bool is_started() const { return started_; }

  void send_ping();
  void send_audio(const uint8_t *data, size_t len);

  void set_crypt_state(MumbleCryptStateBase *cs) { crypt_state_ = cs; }
  MumbleCryptStateBase *get_crypt_state() { return crypt_state_; }

  uint32_t get_udp_packets_sent() const { return udp_packets_sent_; }

  using AudioCallback = std::function<void(const uint8_t *data, size_t len)>;
  void set_audio_callback(AudioCallback cb) { audio_callback_ = std::move(cb); }

 private:
  void process_packet(const uint8_t *data, size_t len);
  bool send_encrypted(const uint8_t *plain, size_t len);
  bool send_raw(const uint8_t *data, size_t len);

  WiFiUDP udp_;
  std::string server_;
  uint16_t port_{0};
  bool started_{false};
  bool udp_active_{false};
  uint32_t last_ping_sent_ms_{0};
  uint64_t last_ping_timestamp_{0};
  uint32_t last_ping_timeout_log_ms_{0};  // rate-limit timeout warning
  uint8_t recv_buf_[MAX_PACKET_SIZE + MAX_CRYPTO_OVERHEAD];
  uint8_t crypt_buf_[MAX_PACKET_SIZE + MAX_CRYPTO_OVERHEAD];
  MumbleCryptStateBase *crypt_state_{nullptr};
  uint32_t udp_packets_sent_{0};
  AudioCallback audio_callback_;
};

}  // namespace mumble
}  // namespace esphome
