#include "mumble_udp.h"
#include "mumble_diag.h"
#include "mumble_socket.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.udp";

// Header byte: codec (3 bits) | target (5 bits). Codec 1 = Ping.
static constexpr uint8_t HEADER_PING = 0x20;

bool MumbleUdp::start(const std::string &server, uint16_t port, uint32_t peer_ip) {
  if (server.empty() || port == 0) {
    ESP_LOGW(TAG, "start: server or port invalid");
    return false;
  }
  stop();
  server_ = server;
  port_ = port;
  if (peer_ip != 0) {
    cached_ip_ = peer_ip;
  } else {
    cached_ip_ = mumble_dns_lookup(server.c_str());
    if (cached_ip_ == 0) {
      ESP_LOGW(TAG, "UDP DNS lookup failed for %s", server.c_str());
      return false;
    }
  }
  if (udp_ == nullptr) udp_ = mumble_create_udp_socket();
  if (!udp_->begin(0)) {
    ESP_LOGE(TAG, "UDP begin failed");
    return false;
  }
  if (udp_->connect_remote(cached_ip_, port_)) {
    ESP_LOGD(TAG, "UDP connected (send instead of sendto)");
  }
  started_ = true;
  udp_active_ = false;
  last_ping_sent_ms_ = 0;
  last_ping_timestamp_ = 0;
  last_ping_timeout_log_ms_ = 0;
  initial_pings_sent_ = 0;
  ESP_LOGI(TAG, "UDP started for %s:%u", server_.c_str(), port_);
  return true;
}

void MumbleUdp::stop() {
  if (!started_) return;
  if (udp_ != nullptr) udp_->stop();
  started_ = false;
  udp_active_ = false;
  ESP_LOGD(TAG, "UDP stopped");
}

void MumbleUdp::loop() {
  if (!started_) return;

  uint32_t now = mumble_millis();

  if (!udp_active_) {
    if (last_ping_sent_ms_ > 0 && (now - last_ping_sent_ms_) > UDP_PING_TIMEOUT_MS) {
      // Rate-limit: log at most once per 60s. UDP echo often blocked by NAT; voice uses TCP tunnel.
      if (now - last_ping_timeout_log_ms_ >= 60000) {
        ESP_LOGW(TAG, "UDP ping echo not received (NAT/firewall?); voice via TCP tunnel");
        last_ping_timeout_log_ms_ = now;
      }
      last_ping_sent_ms_ = 0;
    } else {
      uint32_t interval =
          (initial_pings_sent_ < UDP_PING_INITIAL_COUNT) ? UDP_PING_INITIAL_INTERVAL_MS : UDP_PING_INTERVAL_MS;
      if (last_ping_sent_ms_ == 0 || (now - last_ping_sent_ms_) >= interval) {
        send_ping();
      }
    }
  }

  int pkt_size = udp_->parse_packet();
  if (pkt_size <= 0) return;
  if (pkt_size > (int) (MAX_PACKET_SIZE + MAX_CRYPTO_OVERHEAD)) {
    udp_->flush();
    ESP_LOGW(TAG, "UDP packet too large: %d", pkt_size);
    return;
  }

  int n = udp_->read(recv_buf_, sizeof(recv_buf_));
  udp_->flush();
  if (n <= 0) return;

  udp_packets_recv_++;
  if (udp_packets_recv_ <= 5) {
    mumble_log_hex(TAG, "UDP RX", recv_buf_, static_cast<size_t>(n), 32);
  }
  process_packet(recv_buf_, static_cast<size_t>(n));
}

void MumbleUdp::process_packet(const uint8_t *data, size_t len) {
  if (len < 1) return;

  const uint8_t *plain = data;
  size_t plain_len = len;

  // Decrypt if crypto is active
  if (crypt_state_ != nullptr && crypt_state_->is_valid()) {
    size_t oh = crypt_state_->overhead();
    if (len < oh) return;
    if (!crypt_state_->decrypt(data, crypt_buf_, len)) {
      ESP_LOGD(TAG, "Decrypt failed len=%u", (unsigned) len);
      return;
    }
    plain = crypt_buf_;
    plain_len = len - oh;
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
    size_t enc_len = len + crypt_state_->overhead();
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
  uint64_t ts = static_cast<uint64_t>(mumble_millis());
  size_t n = mumble_varint_encode(buf + pos, static_cast<int64_t>(ts));
  pos += n;

  if (send_encrypted(buf, pos)) {
    last_ping_sent_ms_ = mumble_millis();
    last_ping_timestamp_ = ts;
    if (initial_pings_sent_ < UDP_PING_INITIAL_COUNT) {
      initial_pings_sent_++;
    }
    ESP_LOGD(TAG, "UDP ping sent%s", (crypt_state_ && crypt_state_->is_valid()) ? " (encrypted)" : "");
  }
}

void MumbleUdp::send_audio(const uint8_t *data, size_t len) {
  if (!started_ || len > MAX_PACKET_SIZE) return;
  send_encrypted(data, len);
}

bool MumbleUdp::send_raw(const uint8_t *data, size_t len) {
  if (cached_ip_ == 0) return false;
  if (!udp_->begin_packet(cached_ip_, port_)) return false;
  size_t wrote = udp_->write(data, len);
  bool ok = udp_->end_packet() && wrote == len;
  if (ok) {
    udp_packets_sent_++;
    if (udp_packets_sent_ <= 3) {
      mumble_delay_ms(25);
    }
  }
  if (!ok && udp_packets_sent_ <= 15) {
    ESP_LOGW(TAG, "UDP send FAIL to %u.%u.%u.%u:%u len=%u",
             (unsigned)(cached_ip_ >> 24) & 0xFF, (unsigned)(cached_ip_ >> 16) & 0xFF,
             (unsigned)(cached_ip_ >> 8) & 0xFF, (unsigned) cached_ip_ & 0xFF,
             port_, (unsigned) len);
  }
  return ok;
}

}  // namespace mumble
}  // namespace esphome
