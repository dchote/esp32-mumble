#pragma once

#include "mumble_protocol.h"
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace mumble {

// MsgVersion (type 0)
struct MsgVersion {
  uint32_t version_v1 = 0;
  uint64_t version_v2 = 0;
  std::string release;
  std::string os;
  std::string os_version;
  uint32_t crypto_modes = 0;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgAuthenticate (type 2) - encode only for client
struct MsgAuthenticate {
  std::string username;
  std::string password;
  bool opus = true;
  int32_t client_type = 0;

  void marshal(std::vector<uint8_t> &out) const;
};

// MsgCryptSetup (type 15) - encode (for nonce resync) and decode
struct MsgCryptSetup {
  std::vector<uint8_t> key;
  std::vector<uint8_t> client_nonce;
  std::vector<uint8_t> server_nonce;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgServerSync (type 5) - decode only
struct MsgServerSync {
  uint32_t session = 0;
  uint32_t max_bandwidth = 0;
  std::string welcome_text;
  uint64_t permissions = 0;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgChannelState (type 7) - decode and encode (for channel changes we don't send, but need decode)
struct MsgChannelState {
  uint32_t channel_id = 0;
  uint32_t parent = 0;
  bool has_parent = false;
  std::string name;
  std::vector<uint32_t> links;
  bool temporary = false;
  int32_t position = 0;
  uint32_t max_users = 0;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgChannelRemove (type 6) - decode only
struct MsgChannelRemove {
  uint32_t channel_id = 0;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgUserState (type 9) - decode and encode (encode for channel_id change)
struct MsgUserState {
  uint32_t session = 0;
  uint32_t actor = 0;
  std::string name;
  uint32_t user_id = 0;
  uint32_t channel_id = 0;
  bool mute = false;
  bool deaf = false;
  bool suppress = false;
  bool self_mute = false;
  bool self_deaf = false;

  bool unmarshal(const uint8_t *data, size_t len);
  // Encode UserState for channel change: session (1) + channel_id (5)
  static void marshal_channel_change(std::vector<uint8_t> &out, uint32_t session_id,
                                     uint32_t channel_id);
};

// MsgUserRemove (type 8) - decode only
struct MsgUserRemove {
  uint32_t session = 0;
  std::string reason;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgPing (type 3) - encode and decode
struct MsgPing {
  uint64_t timestamp = 0;
  uint32_t good = 0;
  uint32_t late = 0;
  uint32_t lost = 0;
  uint32_t resync = 0;
  uint32_t udp_packets = 0;
  uint32_t tcp_packets = 0;
  float udp_ping_avg = 0;
  float udp_ping_var = 0;
  float tcp_ping_avg = 0;
  float tcp_ping_var = 0;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// RejectType (for MsgReject)
enum RejectType : uint32_t {
  REJECT_NONE = 0,
  REJECT_WRONG_VERSION = 1,
  REJECT_INVALID_USERNAME = 2,
  REJECT_WRONG_USER_PW = 3,
  REJECT_WRONG_SERVER_PW = 4,
  REJECT_USERNAME_IN_USE = 5,
  REJECT_SERVER_FULL = 6,
  REJECT_NO_CERTIFICATE = 7,
  REJECT_AUTHENTICATOR_FAIL = 8,
  REJECT_NO_NEW_CONNECTIONS = 9,
};

// MsgReject (type 4) - decode only
struct MsgReject {
  RejectType type = REJECT_NONE;
  std::string reason;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgCodecVersion (type 21) - decode only
struct MsgCodecVersion {
  bool opus = false;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgServerConfig (type 24) - decode only
struct MsgServerConfig {
  uint32_t max_bandwidth = 0;
  std::string welcome_text;
  uint32_t max_users = 0;

  bool unmarshal(const uint8_t *data, size_t len);
};

}  // namespace mumble
}  // namespace esphome
