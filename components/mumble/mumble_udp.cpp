#include "mumble_udp.h"
#include "esphome/core/log.h"
#include <WiFi.h>
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.udp";

// Header byte: codec (3 bits) | target (5 bits). Codec 1 = Ping.
static constexpr uint8_t HEADER_PING = 0x20;

bool MumbleUdp::start(const std::string &server, uint16_t port) {
  if (server.empty() || port == 0) {
    ESP_LOGW(TAG, "start: server or port invalid");
    return false;
  }
  stop();
  server_ = server;
  port_ = port;

  if (!udp_.begin(0)) {
    ESP_LOGE(TAG, "UDP begin failed");
    return false;
  }
  started_ = true;
  udp_active_ = false;
  last_ping_sent_ms_ = 0;
  last_ping_timestamp_ = 0;
  last_ping_timeout_log_ms_ = 0;
  ESP_LOGI(TAG, "UDP started for %s:%u", server_.c_str(), port_);
  return true;
}

void MumbleUdp::stop() {
  if (!started_) return;
  udp_.stop();
  started_ = false;
  udp_active_ = false;
  ESP_LOGD(TAG, "UDP stopped");
}

void MumbleUdp::loop() {
  if (!started_) return;

  uint32_t now = millis();

  if (!udp_active_) {
    if (last_ping_sent_ms_ > 0 && (now - last_ping_sent_ms_) > UDP_PING_TIMEOUT_MS) {
      // Rate-limit: log at most once per 60s. UDP echo often blocked by NAT; voice uses TCP tunnel.
      if (now - last_ping_timeout_log_ms_ >= 60000) {
        ESP_LOGW(TAG, "UDP ping echo not received (NAT/firewall?); voice via TCP tunnel");
        last_ping_timeout_log_ms_ = now;
      }
      last_ping_sent_ms_ = 0;
    } else if (last_ping_sent_ms_ == 0 || (now - last_ping_sent_ms_) >= UDP_PING_INTERVAL_MS) {
      send_ping();
    }
  }

  int pkt_size = udp_.parsePacket();
  if (pkt_size <= 0) return;
  if (pkt_size > (int) MAX_PACKET_SIZE + (int) OCB2_OVERHEAD) {
    while (udp_.available()) udp_.read();
    ESP_LOGW(TAG, "UDP packet too large: %d", pkt_size);
    return;
  }

  int n = udp_.read(recv_buf_, sizeof(recv_buf_));
  while (udp_.available()) udp_.read();
  if (n <= 0) return;

  process_packet(recv_buf_, static_cast<size_t>(n));
}

void MumbleUdp::process_packet(const uint8_t *data, size_t len) {
  if (len < 1) return;

  const uint8_t *plain = data;
  size_t plain_len = len;

  // Decrypt if crypto is active
  if (crypt_state_ != nullptr && crypt_state_->is_valid()) {
    if (len < OCB2_OVERHEAD) return;
    if (!crypt_state_->decrypt(data, crypt_buf_, len)) {
      ESP_LOGD(TAG, "Decrypt failed len=%u", (unsigned) len);
      return;
    }
    plain = crypt_buf_;
    plain_len = len - OCB2_OVERHEAD;
  }

  if (plain_len < 1) return;

  uint8_t header = plain[0];
  uint8_t codec = (header >> 5) & 0x07;

  if (codec == 1) {
    if (!udp_active_) {
      udp_active_ = true;
      ESP_LOGI(TAG, "UDP confirmed via ping echo");
    }
    return;
  }

  if (audio_callback_) {
    audio_callback_(plain, plain_len);
  }
}

bool MumbleUdp::send_encrypted(const uint8_t *plain, size_t len) {
  if (crypt_state_ != nullptr && crypt_state_->is_valid()) {
    size_t enc_len = len + OCB2_OVERHEAD;
    if (enc_len > sizeof(crypt_buf_)) return false;
    if (!crypt_state_->encrypt(plain, crypt_buf_, len)) return false;
    return send_raw(crypt_buf_, enc_len);
  }
  return send_raw(plain, len);
}

void MumbleUdp::send_ping() {
  uint8_t buf[16];
  size_t pos = 0;
  buf[pos++] = HEADER_PING;
  uint64_t ts = static_cast<uint64_t>(millis());
  size_t n = mumble_varint_encode(buf + pos, static_cast<int64_t>(ts));
  pos += n;

  if (send_encrypted(buf, pos)) {
    last_ping_sent_ms_ = millis();
    last_ping_timestamp_ = ts;
    ESP_LOGD(TAG, "UDP ping sent%s", (crypt_state_ && crypt_state_->is_valid()) ? " (encrypted)" : "");
  }
}

void MumbleUdp::send_audio(const uint8_t *data, size_t len) {
  if (!started_ || len > MAX_PACKET_SIZE) return;
  send_encrypted(data, len);
}

bool MumbleUdp::send_raw(const uint8_t *data, size_t len) {
  IPAddress ip;
  if (!WiFi.hostByName(server_.c_str(), ip)) {
    return false;
  }
  if (!udp_.beginPacket(ip, port_)) {
    return false;
  }
  size_t wrote = udp_.write(data, len);
  if (!udp_.endPacket() || wrote != len) {
    return false;
  }
  return true;
}

}  // namespace mumble
}  // namespace esphome
