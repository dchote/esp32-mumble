#include "mumble_client.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.client";

// Crypto mode bits (Version message CryptoModes field)
static constexpr uint32_t CRYPTO_LITE = 0x01;
static constexpr uint32_t CRYPTO_LEGACY = 0x02;

bool MumbleClient::connect() {
  if (server_.empty() || username_.empty()) {
    ESP_LOGW(TAG, "connect: server or username empty");
    return false;
  }
  state_ = ConnectionState::CONNECTING;
  if (tls_client_ == nullptr) {
    tls_client_ = mumble_create_tls_client();
  }
  if (!ca_cert_.empty()) {
    tls_client_->set_ca_cert(ca_cert_.c_str());
  } else {
    tls_client_->set_insecure();
  }
  // connect() spawns a background task (ESP-IDF) or blocks (Arduino).
  // esp_tls handles DNS internally so no manual fallback needed.
  bool ok = tls_client_->connect(server_.c_str(), port_, CONNECT_TIMEOUT_MS);
  if (ok) {
    recv_buf_.clear();
    recv_buf_.reserve(1024);
    channel_join_sent_ = false;
    unknown_crypt_logged_ = false;
    state_ = ConnectionState::AUTHENTICATING;
    ESP_LOGI(TAG, "Connected to %s:%u, starting handshake", server_.c_str(), port_);
    return true;
  }
  if (tls_client_->connect_in_progress()) {
    ESP_LOGD(TAG, "TLS connect to %s:%u in progress (background task)", server_.c_str(), port_);
    return false;  // Stay in CONNECTING; loop() will call connect_poll()
  }
  char errbuf[128];
  int err = tls_client_->last_error(errbuf, sizeof(errbuf));
  ESP_LOGE(TAG, "TLS connect failed to %s:%u: last_error=%d (%s)", server_.c_str(), port_, err,
           errbuf[0] ? errbuf : "(no message)");
  tls_client_->stop();
  state_ = ConnectionState::DISCONNECTED;
  return false;
}

void MumbleClient::disconnect() {
  if (tls_client_ != nullptr) tls_client_->stop();
  state_ = ConnectionState::DISCONNECTED;
  recv_buf_.clear();
  channels_.clear();
  users_.clear();
  session_id_ = 0;
  channel_join_sent_ = false;
  crypt_setup_received_ = false;
  crypt_negotiated_mode_ = 0;
  ESP_LOGD(TAG, "disconnect");
}

void MumbleClient::loop() {
  uint32_t now = mumble_millis();

  switch (state_) {
    case ConnectionState::DISCONNECTED: {
      if (server_.empty() || username_.empty()) return;
      if (!network::is_connected()) {
        network_connected_since_ms_ = 0;
        reject_count_ = 0;  // Reset on network loss; will retry after reconnect
        return;
      }
      if (network_connected_since_ms_ == 0) {
        network_connected_since_ms_ = now;
      }
      if ((now - network_connected_since_ms_) < WIFI_STABILITY_DELAY_MS) {
        return;  // Wait for server to clean up previous session
      }
      if (reject_count_ >= MAX_REJECT_ATTEMPTS) {
        return;  // Stop after 5 rejections; reset on network reconnect or reboot
      }
      if (now - last_connect_attempt_ms_ < reconnect_delay_ms_) return;
      last_connect_attempt_ms_ = now;
      if (connect()) {
        reconnect_delay_ms_ = 1000;
      } else if (tls_client_ == nullptr || !tls_client_->connect_in_progress()) {
        if (reconnect_delay_ms_ < RECONNECT_DELAY_MAX_MS) {
          reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, RECONNECT_DELAY_MAX_MS);
        }
      }
      return;
    }
    case ConnectionState::CONNECTING: {
      int poll = tls_client_ != nullptr ? tls_client_->connect_poll() : -1;
      if (poll == 1) {
        recv_buf_.clear();
        recv_buf_.reserve(1024);
        channel_join_sent_ = false;
        unknown_crypt_logged_ = false;
        state_ = ConnectionState::AUTHENTICATING;
        ESP_LOGI(TAG, "Connected to %s:%u, starting handshake", server_.c_str(), port_);
      } else if (poll == -1) {
        tls_client_->stop();
        state_ = ConnectionState::DISCONNECTED;
        if (reconnect_delay_ms_ < RECONNECT_DELAY_MAX_MS) {
          reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, RECONNECT_DELAY_MAX_MS);
        }
      }
      return;
    }
    case ConnectionState::AUTHENTICATING:
    case ConnectionState::CONNECTED:
      break;
  }

  if (tls_client_ == nullptr || !tls_client_->connected()) {
    ESP_LOGW(TAG, "Connection lost");
    disconnect();
    return;
  }

  if (state_ == ConnectionState::CONNECTED) {
    if (last_ping_ms_ > 0 && (now - last_ping_ms_) > PING_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Ping timeout");
      disconnect();
      return;
    }
    if ((now - last_ping_ms_) >= PING_INTERVAL_MS) {
      send_ping();
    }
  }

  // With non-blocking TLS, esp_tls_get_bytes_avail only reports bytes already
  // decrypted by mbedTLS. We must attempt a read() to process incoming TLS
  // records from the socket, even when available() returns 0.
  int max_messages = 8;
  while (max_messages-- > 0) {
    size_t space = RECV_BUF_MAX - recv_buf_.size();
    if (space == 0) break;
    size_t to_read = std::min(space, static_cast<size_t>(1024));

    size_t old_size = recv_buf_.size();
    recv_buf_.resize(old_size + to_read);
    int n = tls_client_->read(&recv_buf_[old_size], to_read);
    if (n <= 0) {
      recv_buf_.resize(old_size);
      if (n < 0) {
        // Arduino WiFiClientSecure returns -1 when no data available (not just on error).
        // Only disconnect if the connection is actually dead.
        if (!tls_client_->connected()) {
          ESP_LOGW(TAG, "TLS read error, disconnecting");
          disconnect();
          return;
        }
      }
      break;
    }
    recv_buf_.resize(old_size + static_cast<size_t>(n));

    size_t consumed = 0;
    while (recv_buf_.size() - consumed >= TCP_HEADER_SIZE) {
      uint16_t type;
      uint32_t payload_len;
      if (!read_tcp_header(recv_buf_.data() + consumed, recv_buf_.size() - consumed, &type, &payload_len)) {
        break;
      }
      if (payload_len > MAX_PAYLOAD_PRACTICAL) {
        ESP_LOGW(TAG, "Payload too large: %u", (unsigned) payload_len);
        disconnect();
        return;
      }
      size_t total = TCP_HEADER_SIZE + payload_len;
      if (recv_buf_.size() - consumed < total) break;

      handle_message(type, recv_buf_.data() + consumed + TCP_HEADER_SIZE, payload_len);
      consumed += total;
      if (state_ == ConnectionState::DISCONNECTED) {
        break;
      }
    }
    if (consumed > 0) {
      recv_buf_.erase(recv_buf_.begin(), recv_buf_.begin() + static_cast<ptrdiff_t>(consumed));
    }
    if (state_ == ConnectionState::DISCONNECTED) {
      break;
    }
  }
}

bool MumbleClient::write_all(const uint8_t *buf, size_t len) {
  size_t sent = 0;
  int retries = 0;
  while (sent < len) {
    size_t n = tls_client_->write(buf + sent, len - sent);
    if (n > 0) {
      sent += n;
      retries = 0;
    } else if (++retries > 200) {
      ESP_LOGW(TAG, "write_all failed after retries (sent %u/%u)", (unsigned) sent, (unsigned) len);
      return false;
    } else {
      vTaskDelay(1);  // yield to let TLS/TCP drain
    }
  }
  return true;
}

bool MumbleClient::send_message(uint16_t type, const uint8_t *payload, size_t payload_len) {
  uint8_t header[TCP_HEADER_SIZE];
  write_tcp_header(header, type, static_cast<uint32_t>(payload_len));
  if (!write_all(header, TCP_HEADER_SIZE)) return false;
  if (payload_len > 0 && payload != nullptr) {
    if (!write_all(payload, payload_len)) return false;
  }
  return true;
}

bool MumbleClient::send_udp_tunnel(const uint8_t *data, size_t len) {
  if (tls_client_ == nullptr || !tls_client_->connected() || data == nullptr) return false;
  return send_message(MSG_UDP_TUNNEL, data, len);
}

void MumbleClient::handle_message(uint16_t type, const uint8_t *payload, size_t len) {
  switch (type) {
    case MSG_VERSION:
      on_version(payload, len);
      break;
    case MSG_CRYPT_SETUP:
      on_crypt_setup(payload, len);
      break;
    case MSG_CHANNEL_STATE:
      on_channel_state(payload, len);
      break;
    case MSG_CHANNEL_REMOVE:
      on_channel_remove(payload, len);
      break;
    case MSG_USER_STATE:
      on_user_state(payload, len);
      break;
    case MSG_USER_REMOVE:
      on_user_remove(payload, len);
      break;
    case MSG_SERVER_SYNC:
      on_server_sync(payload, len);
      break;
    case MSG_PING:
      on_ping(payload, len);
      break;
    case MSG_REJECT:
      on_reject(payload, len);
      break;
    case MSG_CODEC_VERSION:
      on_codec_version(payload, len);
      break;
    case MSG_SERVER_CONFIG:
      on_server_config(payload, len);
      break;
    case MSG_UDP_TUNNEL:
      on_udp_tunnel(payload, len);
      break;
    case MSG_PERMISSION_QUERY:
      ESP_LOGD(TAG, "Ignoring PermissionQuery (type 20)");
      break;
    default:
      ESP_LOGD(TAG, "Unhandled message type %u", (unsigned) type);
      break;
  }
}

void MumbleClient::send_version() {
  MsgVersion v;
  v.version_v1 = 0x00010204;
  v.release = "esp32-mumble";
  v.os = "ESP32";
  v.os_version = "1.0";
  // Advertise supported modes: 0x03 = Lite | Legacy so server can pick. Secure (0x04) when we have TLS 1.3 + client cert.
  v.crypto_modes = (crypto_mode_ == 0) ? CRYPTO_LITE : (CRYPTO_LITE | CRYPTO_LEGACY);

  std::vector<uint8_t> buf;
  v.marshal(buf);
  if (!send_message(MSG_VERSION, buf.data(), buf.size())) {
    ESP_LOGW(TAG, "Failed to send Version");
  } else {
    ESP_LOGD(TAG, "Sent Version (crypto=0x%x)", (unsigned) v.crypto_modes);
  }
}

void MumbleClient::send_authenticate() {
  MsgAuthenticate a;
  a.username = username_;
  a.password = password_;
  a.opus = true;
  a.client_type = 0;

  std::vector<uint8_t> buf;
  a.marshal(buf);
  if (!send_message(MSG_AUTHENTICATE, buf.data(), buf.size())) {
    ESP_LOGW(TAG, "Failed to send Authenticate");
  } else {
    ESP_LOGD(TAG, "Sent Authenticate (user=%s)", username_.c_str());
  }
}

void MumbleClient::send_ping() {
  uint64_t ts = static_cast<uint64_t>(mumble_millis());
  MsgPing p;
  p.timestamp = ts;
  p.good = ping_udp_good_;
  p.late = ping_udp_late_;
  p.lost = ping_udp_lost_;
  p.resync = ping_udp_resync_;
  p.udp_packets = ping_udp_packets_;
  p.tcp_packets = 1;

  std::vector<uint8_t> buf;
  p.marshal(buf);
  if (send_message(MSG_PING, buf.data(), buf.size())) {
    last_ping_ms_ = mumble_millis();
    last_ping_timestamp_ = ts;
  }
}

void MumbleClient::join_channel(const std::string &name) {
  if (channel_join_sent_) return;
  uint32_t ch_id = find_channel_by_name(name);
  if (ch_id == 0 && !name.empty()) {
    ESP_LOGW(TAG, "Channel not found: %s", name.c_str());
    return;
  }
  join_channel_by_id(ch_id);
  channel_join_sent_ = true;
}

void MumbleClient::join_channel_by_id(uint32_t channel_id) {
  std::vector<uint8_t> buf;
  MsgUserState::marshal_channel_change(buf, session_id_, channel_id);
  if (send_message(MSG_USER_STATE, buf.data(), buf.size())) {
    ESP_LOGI(TAG, "Joined channel id=%u", (unsigned) channel_id);
  }
}

void MumbleClient::on_version(const uint8_t *payload, size_t len) {
  MsgVersion v;
  if (!v.unmarshal(payload, len)) return;
  ESP_LOGD(TAG, "Server Version: %s, crypto=0x%x", v.release.c_str(), (unsigned) v.crypto_modes);
  send_version();
  send_authenticate();
}

void MumbleClient::on_crypt_setup(const uint8_t *payload, size_t len) {
  MsgCryptSetup m;
  if (!m.unmarshal(payload, len)) return;

  if (!m.key.empty() && m.key.size() == 16 &&
      m.client_nonce.size() == 16 && m.server_nonce.size() == 16) {
    crypt_key_ = std::move(m.key);
    crypt_client_nonce_ = std::move(m.client_nonce);
    crypt_server_nonce_ = std::move(m.server_nonce);
    crypt_negotiated_mode_ = 1;  // Legacy
    crypt_setup_received_ = true;
    ESP_LOGI(TAG, "CryptSetup: Legacy (OCB2-AES128) key received");
  } else if (!m.key.empty() && m.key.size() == 32 &&
             m.client_nonce.size() == 12 && m.server_nonce.size() == 12) {
    crypt_key_ = std::move(m.key);
    crypt_client_nonce_ = std::move(m.client_nonce);
    crypt_server_nonce_ = std::move(m.server_nonce);
    crypt_negotiated_mode_ = 2;  // Secure
    crypt_setup_received_ = true;
    ESP_LOGI(TAG, "CryptSetup: Secure (AES-256-GCM) key received");
  } else if (m.key.empty() && !m.server_nonce.empty()) {
    // Nonce resync: server updated its encrypt nonce (our decrypt IV) — Legacy only
    crypt_server_nonce_ = std::move(m.server_nonce);
    crypt_setup_received_ = true;
    ESP_LOGI(TAG, "CryptSetup: server nonce resync (%u bytes)",
             (unsigned) crypt_server_nonce_.size());
  } else if (m.key.empty()) {
    crypt_negotiated_mode_ = 0;  // Lite
    crypt_setup_received_ = true;
    ESP_LOGD(TAG, "CryptSetup: Lite mode (no UDP encryption)");
  } else {
    if (!unknown_crypt_logged_) {
      ESP_LOGD(TAG, "CryptSetup: unsupported key=%u cn=%u sn=%u, ignoring",
               (unsigned) m.key.size(), (unsigned) m.client_nonce.size(),
               (unsigned) m.server_nonce.size());
      unknown_crypt_logged_ = true;
    }
  }
}

void MumbleClient::on_channel_state(const uint8_t *payload, size_t len) {
  MsgChannelState m;
  if (!m.unmarshal(payload, len)) return;
  update_channel(m);
}

void MumbleClient::on_channel_remove(const uint8_t *payload, size_t len) {
  MsgChannelRemove m;
  if (!m.unmarshal(payload, len)) return;
  remove_channel(m.channel_id);
}

void MumbleClient::on_user_state(const uint8_t *payload, size_t len) {
  MsgUserState m;
  if (!m.unmarshal(payload, len)) return;
  update_user(m);
}

void MumbleClient::on_user_remove(const uint8_t *payload, size_t len) {
  MsgUserRemove m;
  if (!m.unmarshal(payload, len)) return;
  remove_user(m.session);
}

void MumbleClient::on_server_sync(const uint8_t *payload, size_t len) {
  MsgServerSync m;
  if (!m.unmarshal(payload, len)) return;
  session_id_ = m.session;
  max_bandwidth_ = m.max_bandwidth;
  welcome_text_ = m.welcome_text;
  state_ = ConnectionState::CONNECTED;
  last_ping_ms_ = 0;  // Reset so (now - last_ping_ms_) doesn't immediately exceed PING_TIMEOUT_MS on reconnect
  reconnect_delay_ms_ = 1000;
  reject_count_ = 0;  // Successful auth resets reject counter
  ESP_LOGI(TAG, "ServerSync: session=%u, welcome=%s", (unsigned) session_id_,
           welcome_text_.empty() ? "(none)" : welcome_text_.c_str());
  join_channel(channel_);
}

void MumbleClient::on_ping(const uint8_t *payload, size_t len) {
  MsgPing m;
  if (!m.unmarshal(payload, len)) return;
  if (m.timestamp == last_ping_timestamp_ && last_ping_ms_ > 0) {
    ping_rtt_ms_ = static_cast<float>(mumble_millis() - last_ping_ms_);
  }
}

void MumbleClient::on_reject(const uint8_t *payload, size_t len) {
  MsgReject m;
  if (!m.unmarshal(payload, len)) return;
  reject_count_++;
  if (m.type == REJECT_NO_CERTIFICATE) {
    ESP_LOGE(TAG, "Rejected: server requires a client certificate (Secure tier). Configure a client cert to use Secure crypto.");
  }
  ESP_LOGE(TAG, "Rejected (%u/%u): type=%u, reason=%s",
           (unsigned) reject_count_, (unsigned) MAX_REJECT_ATTEMPTS,
           (unsigned) m.type, m.reason.c_str());
  last_connect_attempt_ms_ = mumble_millis();  // Prevent immediate retry
  reconnect_delay_ms_ = std::max(reconnect_delay_ms_, static_cast<uint32_t>(30000));  // 30s backoff on reject
  disconnect();
}

void MumbleClient::send_crypt_resync(const uint8_t *encrypt_iv, size_t len) {
  MsgCryptSetup cs;
  cs.client_nonce.assign(encrypt_iv, encrypt_iv + len);
  std::vector<uint8_t> buf;
  cs.marshal(buf);
  if (send_message(MSG_CRYPT_SETUP, buf.data(), buf.size())) {
    ESP_LOGI(TAG, "Sent CryptSetup resync (client_nonce)");
  }
}

void MumbleClient::on_codec_version(const uint8_t *payload, size_t len) {
  MsgCodecVersion m;
  if (!m.unmarshal(payload, len)) return;
  if (m.opus) {
    ESP_LOGD(TAG, "Server supports Opus");
  }
}

void MumbleClient::on_server_config(const uint8_t *payload, size_t len) {
  MsgServerConfig m;
  if (!m.unmarshal(payload, len)) return;
  ESP_LOGD(TAG, "ServerConfig: max_bandwidth=%u, max_users=%u", (unsigned) m.max_bandwidth,
           (unsigned) m.max_users);
}

void MumbleClient::on_udp_tunnel(const uint8_t *payload, size_t len) {
  if (voice_packet_callback_) {
    voice_packet_callback_(payload, len);
  }
}

void MumbleClient::update_channel(const MsgChannelState &m) {
  for (auto &c : channels_) {
    if (c.id == m.channel_id) {
      c.parent_id = m.parent;
      c.name = m.name;
      c.temporary = m.temporary;
      c.position = m.position;
      ESP_LOGD(TAG, "Channel update: id=%u parent=%u name='%s'",
               (unsigned) m.channel_id, (unsigned) m.parent, m.name.c_str());
      return;
    }
  }
  ChannelInfo c;
  c.id = m.channel_id;
  c.parent_id = m.parent;
  c.name = m.name;
  c.temporary = m.temporary;
  c.position = m.position;
  channels_.push_back(c);
  ESP_LOGI(TAG, "Channel add: id=%u parent=%u name='%s' (total=%zu)",
           (unsigned) m.channel_id, (unsigned) m.parent,
           m.name.empty() ? "(unnamed)" : m.name.c_str(), channels_.size());
}

void MumbleClient::remove_channel(uint32_t id) {
  channels_.erase(
      std::remove_if(channels_.begin(), channels_.end(), [id](const ChannelInfo &c) {
        return c.id == id;
      }),
      channels_.end());
}

void MumbleClient::update_user(const MsgUserState &m) {
  for (auto &u : users_) {
    if (u.session == m.session) {
      u.name = m.name;
      u.channel_id = m.channel_id;
      u.mute = m.mute;
      u.deaf = m.deaf;
      u.self_mute = m.self_mute;
      u.self_deaf = m.self_deaf;
      return;
    }
  }
  UserInfo u;
  u.session = m.session;
  u.name = m.name;
  u.channel_id = m.channel_id;
  u.mute = m.mute;
  u.deaf = m.deaf;
  u.self_mute = m.self_mute;
  u.self_deaf = m.self_deaf;
  users_.push_back(u);
}

void MumbleClient::remove_user(uint32_t session) {
  users_.erase(
      std::remove_if(users_.begin(), users_.end(),
                    [session](const UserInfo &u) { return u.session == session; }),
      users_.end());
}

static bool str_iequals(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    char ca = a[i], cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb) return false;
  }
  return true;
}

uint32_t MumbleClient::find_channel_by_name(const std::string &name) const {
  if (name.empty()) return 0;
  for (const auto &c : channels_) {
    if (str_iequals(c.name, name)) return c.id;
  }
  return 0;
}

uint32_t MumbleClient::get_current_channel_id() const {
  if (session_id_ == 0) return 0;
  for (const auto &u : users_) {
    if (u.session == session_id_) return u.channel_id;
  }
  return 0;
}

static constexpr size_t MAX_CHANNEL_OPTIONS = 64;

void MumbleClient::build_channel_tree_options(std::vector<std::string> &out_strings,
                                              std::vector<uint32_t> &out_ids) const {
  out_strings.clear();
  out_ids.clear();
  std::vector<const ChannelInfo *> sorted;
  sorted.reserve(channels_.size());
  for (const auto &c : channels_) {
    sorted.push_back(&c);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const ChannelInfo *a, const ChannelInfo *b) {
              if (a->parent_id != b->parent_id) return a->parent_id < b->parent_id;
              return a->position < b->position;
            });
  for (const auto *c : sorted) {
    if (out_strings.size() >= MAX_CHANNEL_OPTIONS) break;
    std::string name = c->name.empty() ? "(unnamed)" : c->name;
    if (name.size() > 48) name.resize(48);
    std::string opt = std::to_string(c->id) + ":" + name;
    out_strings.push_back(opt);
    out_ids.push_back(c->id);
  }
  if (out_strings.empty()) {
    out_strings.push_back("0:Root");
    out_ids.push_back(0);
  }
}

}  // namespace mumble
}  // namespace esphome
