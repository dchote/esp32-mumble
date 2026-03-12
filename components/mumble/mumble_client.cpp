#include "mumble_client.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <WiFi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.client";

// Crypto mode bits
static constexpr uint32_t CRYPTO_LITE = 0x01;
static constexpr uint32_t CRYPTO_LEGACY = 0x02;

bool MumbleClient::connect() {
  if (server_.empty() || username_.empty()) {
    ESP_LOGW(TAG, "connect: server or username empty");
    return false;
  }
  state_ = ConnectionState::CONNECTING;
  // go-mumble-server requires TLS 1.2 (MinVersion: tls.VersionTLS12). ESP32 mbedTLS
  // uses TLS 1.2 by default. setInsecure() skips cert verification for trusted LAN.
  tls_client_.setInsecure();
  tls_client_.setTimeout(CONNECT_TIMEOUT_MS / 1000);
  tls_client_.setHandshakeTimeout(CONNECT_TIMEOUT_MS / 1000);

  // Connect by hostname so SNI is sent (some servers require it). Fallback to IP if
  // hostname fails (e.g. local IP like 172.20.13.17).
  bool ok = tls_client_.connect(server_.c_str(), port_, CONNECT_TIMEOUT_MS / 1000);
  if (!ok) {
    IPAddress ip;
    if (WiFi.hostByName(server_.c_str(), ip)) {
      ok = tls_client_.connect(ip, port_, CONNECT_TIMEOUT_MS / 1000);
    }
  }
  if (!ok) {
    char errbuf[128];
    int err = tls_client_.lastError(errbuf, sizeof(errbuf));
    ESP_LOGE(TAG, "TLS connect failed to %s:%u: last_error=%d (%s)", server_.c_str(), port_, err,
             errbuf[0] ? errbuf : "(no message)");
    tls_client_.stop();
    state_ = ConnectionState::DISCONNECTED;
    return false;
  }

  recv_buf_.clear();
  recv_buf_.reserve(1024);
  channel_join_sent_ = false;
  state_ = ConnectionState::AUTHENTICATING;
  ESP_LOGI(TAG, "Connected to %s:%u, starting handshake", server_.c_str(), port_);
  return true;
}

void MumbleClient::disconnect() {
  tls_client_.stop();
  state_ = ConnectionState::DISCONNECTED;
  recv_buf_.clear();
  channels_.clear();
  users_.clear();
  session_id_ = 0;
  channel_join_sent_ = false;
  crypt_setup_received_ = false;
  ESP_LOGD(TAG, "disconnect");
}

void MumbleClient::loop() {
  uint32_t now = millis();

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
      } else {
        if (reconnect_delay_ms_ < RECONNECT_DELAY_MAX_MS) {
          reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, RECONNECT_DELAY_MAX_MS);
        }
      }
      return;
    }
    case ConnectionState::CONNECTING:
      return;
    case ConnectionState::AUTHENTICATING:
    case ConnectionState::CONNECTED:
      break;
  }

  if (!tls_client_.connected()) {
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

  int max_messages = 8;
  while (tls_client_.available() > 0 && max_messages-- > 0) {
    size_t avail = tls_client_.available();
    size_t to_read = std::min(avail, RECV_BUF_MAX - recv_buf_.size());
    if (to_read == 0) break;

    size_t old_size = recv_buf_.size();
    recv_buf_.resize(old_size + to_read);
    int n = tls_client_.read(&recv_buf_[old_size], to_read);
    if (n <= 0) break;
    recv_buf_.resize(old_size + static_cast<size_t>(n));

    while (recv_buf_.size() >= TCP_HEADER_SIZE) {
      uint16_t type;
      uint32_t payload_len;
      if (!read_tcp_header(recv_buf_.data(), recv_buf_.size(), &type, &payload_len)) {
        break;
      }
      if (payload_len > MAX_PAYLOAD_PRACTICAL) {
        ESP_LOGW(TAG, "Payload too large: %u", (unsigned) payload_len);
        disconnect();
        return;
      }
      size_t total = TCP_HEADER_SIZE + payload_len;
      if (recv_buf_.size() < total) break;

      handle_message(type, recv_buf_.data() + TCP_HEADER_SIZE, payload_len);

      recv_buf_.erase(recv_buf_.begin(), recv_buf_.begin() + static_cast<ptrdiff_t>(total));
      if (state_ == ConnectionState::DISCONNECTED) {
        break;  // Exit message loop to avoid spin (e.g. on Reject)
      }
    }
    if (state_ == ConnectionState::DISCONNECTED) {
      break;
    }
  }
}

bool MumbleClient::send_message(uint16_t type, const uint8_t *payload, size_t payload_len) {
  uint8_t header[TCP_HEADER_SIZE];
  write_tcp_header(header, type, static_cast<uint32_t>(payload_len));
  size_t wrote = tls_client_.write(header, TCP_HEADER_SIZE);
  if (wrote != TCP_HEADER_SIZE) return false;
  if (payload_len > 0 && payload != nullptr) {
    wrote = tls_client_.write(payload, payload_len);
    if (wrote != payload_len) return false;
  }
  return true;
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
  v.crypto_modes = (crypto_mode_ == 0) ? CRYPTO_LITE : CRYPTO_LEGACY;

  std::vector<uint8_t> buf;
  v.marshal(buf);
  if (!send_message(MSG_VERSION, buf.data(), buf.size())) {
    ESP_LOGW(TAG, "Failed to send Version");
  } else {
    ESP_LOGD(TAG, "Sent Version (crypto=%u)", (unsigned) v.crypto_modes);
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
  uint64_t ts = static_cast<uint64_t>(millis());
  MsgPing p;
  p.timestamp = ts;
  p.tcp_packets = 1;

  std::vector<uint8_t> buf;
  p.marshal(buf);
  if (send_message(MSG_PING, buf.data(), buf.size())) {
    last_ping_ms_ = millis();
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
    crypt_setup_received_ = true;
    ESP_LOGI(TAG, "CryptSetup: Legacy (OCB2-AES128) key received");
  } else if (m.key.empty() && !m.server_nonce.empty()) {
    // Nonce resync: server updated its encrypt nonce (our decrypt IV)
    crypt_server_nonce_ = std::move(m.server_nonce);
    crypt_setup_received_ = true;
    ESP_LOGI(TAG, "CryptSetup: server nonce resync (%u bytes)",
             (unsigned) crypt_server_nonce_.size());
  } else if (m.key.empty()) {
    ESP_LOGD(TAG, "CryptSetup: Lite mode (no UDP encryption)");
  } else {
    ESP_LOGW(TAG, "CryptSetup: unexpected key=%u cn=%u sn=%u",
             (unsigned) m.key.size(), (unsigned) m.client_nonce.size(),
             (unsigned) m.server_nonce.size());
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
    ping_rtt_ms_ = static_cast<float>(millis() - last_ping_ms_);
  }
}

void MumbleClient::on_reject(const uint8_t *payload, size_t len) {
  MsgReject m;
  if (!m.unmarshal(payload, len)) return;
  reject_count_++;
  ESP_LOGE(TAG, "Rejected (%u/%u): type=%u, reason=%s",
           (unsigned) reject_count_, (unsigned) MAX_REJECT_ATTEMPTS,
           (unsigned) m.type, m.reason.c_str());
  last_connect_attempt_ms_ = millis();  // Prevent immediate retry
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
