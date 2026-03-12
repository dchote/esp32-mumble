#pragma once

#include "mumble_messages.h"
#include "mumble_protocol.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace mumble {

struct ChannelInfo {
  uint32_t id = 0;
  uint32_t parent_id = 0;
  std::string name;
  bool temporary = false;
  int32_t position = 0;
};

struct UserInfo {
  uint32_t session = 0;
  std::string name;
  uint32_t channel_id = 0;
  bool mute = false;
  bool deaf = false;
  bool self_mute = false;
  bool self_deaf = false;
};

enum class ConnectionState {
  DISCONNECTED,
  CONNECTING,
  AUTHENTICATING,
  CONNECTED,
};

class MumbleClient {
 public:
  MumbleClient() = default;
  ~MumbleClient() = default;

  void set_server(const std::string &server) { server_ = server; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void set_channel(const std::string &channel) { channel_ = channel; }
  void set_crypto(uint8_t crypto) { crypto_mode_ = crypto; }

  bool connect();
  void disconnect();
  bool is_connected() const { return state_ == ConnectionState::CONNECTED; }
  void loop();

  float get_ping_rtt_ms() const { return ping_rtt_ms_; }
  uint32_t get_session_id() const { return session_id_; }
  const std::vector<ChannelInfo> &get_channels() const { return channels_; }
  const std::vector<UserInfo> &get_users() const { return users_; }
  uint32_t find_channel_by_name(const std::string &name) const;
  void join_channel_by_id(uint32_t channel_id);
  uint32_t get_current_channel_id() const;  // Our user's channel, or 0 if unknown
  // Build channel options for select: "id:Name" (flat list). Fills out_strings and out_ids.
  void build_channel_tree_options(std::vector<std::string> &out_strings,
                                  std::vector<uint32_t> &out_ids) const;

 private:
  bool send_message(uint16_t type, const uint8_t *payload, size_t payload_len);
  void handle_message(uint16_t type, const uint8_t *payload, size_t len);
  void send_version();
  void send_authenticate();
  void send_ping();
  void join_channel(const std::string &name);

  void on_version(const uint8_t *payload, size_t len);
  void on_crypt_setup(const uint8_t *payload, size_t len);
  void on_channel_state(const uint8_t *payload, size_t len);
  void on_channel_remove(const uint8_t *payload, size_t len);
  void on_user_state(const uint8_t *payload, size_t len);
  void on_user_remove(const uint8_t *payload, size_t len);
  void on_server_sync(const uint8_t *payload, size_t len);
  void on_ping(const uint8_t *payload, size_t len);
  void on_reject(const uint8_t *payload, size_t len);
  void on_codec_version(const uint8_t *payload, size_t len);
  void on_server_config(const uint8_t *payload, size_t len);

  void update_channel(const MsgChannelState &m);
  void remove_channel(uint32_t id);
  void update_user(const MsgUserState &m);
  void remove_user(uint32_t session);

  std::string server_;
  uint16_t port_{64738};
  std::string username_;
  std::string password_;
  std::string channel_;
  uint8_t crypto_mode_{0};

  WiFiClientSecure tls_client_;
  std::vector<uint8_t> recv_buf_;
  static constexpr size_t RECV_BUF_MAX = 16384;
  static constexpr size_t MAX_PAYLOAD_PRACTICAL = 16384;

  ConnectionState state_{ConnectionState::DISCONNECTED};
  uint32_t session_id_{0};
  uint32_t max_bandwidth_{0};
  std::string welcome_text_;
  std::vector<ChannelInfo> channels_;
  std::vector<UserInfo> users_;

  uint32_t last_ping_ms_{0};
  uint64_t last_ping_timestamp_{0};
  float ping_rtt_ms_{0};
  static constexpr uint32_t PING_INTERVAL_MS = 15000;
  static constexpr uint32_t PING_TIMEOUT_MS = 45000;

  uint32_t reconnect_delay_ms_{1000};
  uint32_t last_connect_attempt_ms_{0};
  uint32_t network_connected_since_ms_{0};  // 0 = disconnected or not yet tracked
  uint32_t reject_count_{0};                // Consecutive rejections; stop after max
  static constexpr uint32_t RECONNECT_DELAY_MAX_MS = 30000;
  static constexpr uint32_t MAX_REJECT_ATTEMPTS = 5;
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
  static constexpr uint32_t WIFI_STABILITY_DELAY_MS = 5000;  // Wait after WiFi before Mumble connect
  bool channel_join_sent_{false};
};

}  // namespace mumble
}  // namespace esphome
