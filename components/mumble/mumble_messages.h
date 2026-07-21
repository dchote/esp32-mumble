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

// MsgChannelState (type 7) - decode and encode
struct MsgChannelState {
  uint32_t channel_id = 0;
  uint32_t parent = 0;
  bool has_parent = false;
  std::string name;
  std::vector<uint32_t> links;
  bool temporary = false;
  int32_t position = 0;
  uint32_t max_users = 0;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgChannelRemove (type 6)
struct MsgChannelRemove {
  uint32_t channel_id = 0;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgUserState (type 9) - decode and encode
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
  std::string comment;

  bool unmarshal(const uint8_t *data, size_t len);
  /** Full marshal: writes all non-default fields. Use for self_mute, self_deaf, comment, etc. */
  void marshal(std::vector<uint8_t> &out) const;
  // Encode UserState for channel change: session (1) + channel_id (5)
  static void marshal_channel_change(std::vector<uint8_t> &out, uint32_t session_id, uint32_t channel_id);
};

// MsgUserRemove (type 8)
struct MsgUserRemove {
  uint32_t session = 0;
  uint32_t actor = 0;
  bool ban = false;
  std::string reason;

  void marshal(std::vector<uint8_t> &out) const;
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

// MsgTextMessage (type 11)
struct MsgTextMessage {
  uint32_t actor = 0;
  std::vector<uint32_t> session;
  std::vector<uint32_t> channel_id;
  std::vector<uint32_t> tree_id;
  std::string message;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// DenyType (for MsgPermissionDenied)
enum DenyType : uint32_t {
  DENY_TEXT = 0,
  DENY_PERMISSION = 1,
  DENY_SUPER_USER = 2,
  DENY_CHANNEL_NAME = 3,
  DENY_TEXT_TOO_LONG = 4,
  DENY_H9K = 5,
  DENY_TEMPORARY_CHANNEL = 6,
  DENY_MISSING_CERTIFICATE = 7,
  DENY_USER_NAME = 8,
  DENY_CHANNEL_FULL = 9,
  DENY_NESTING_LIMIT = 10,
  DENY_CHANNEL_COUNT_LIMIT = 11,
  DENY_CHANNEL_LISTENER_LIMIT = 12,
  DENY_USER_LISTENER_LIMIT = 13,
};

// MsgPermissionDenied (type 12)
struct MsgPermissionDenied {
  uint32_t permission = 0;
  uint32_t channel_id = 0;
  uint32_t session = 0;
  std::string reason;
  DenyType type = DENY_TEXT;
  std::string name;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgACL (type 13) - minimal: channel_id, inherit_acls, query
struct MsgACL {
  uint32_t channel_id = 0;
  bool inherit_acls = true;
  bool query = false;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgQueryUsers (type 14)
struct MsgQueryUsers {
  std::vector<uint32_t> ids;
  std::vector<std::string> names;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// BanEntry (embedded in MsgBanList)
struct MsgBanEntry {
  std::vector<uint8_t> address;
  uint32_t mask = 0;
  std::string name;
  std::string hash;
  std::string reason;
  std::string start;
  uint32_t duration = 0;
};

// MsgBanList (type 10)
struct MsgBanList {
  std::vector<MsgBanEntry> bans;
  bool query = false;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgContextActionModify (type 16)
struct MsgContextActionModify {
  std::string action;
  std::string text;
  uint32_t context = 0;
  uint32_t operation = 0;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgContextAction (type 17)
struct MsgContextAction {
  uint32_t session = 0;
  uint32_t channel_id = 0;
  std::string action;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// UserListUser (embedded in MsgUserList)
struct MsgUserListUser {
  uint32_t user_id = 0;
  std::string name;
  std::string last_seen;
  int32_t last_channel = 0;
};

// MsgUserList (type 18)
struct MsgUserList {
  std::vector<MsgUserListUser> users;

  bool unmarshal(const uint8_t *data, size_t len);
};

// VoiceTargetTarget (embedded in MsgVoiceTarget)
struct MsgVoiceTargetTarget {
  std::vector<uint32_t> session;
  uint32_t channel_id = 0;
  std::string group;
  bool links = false;
  bool children = false;
};

// MsgVoiceTarget (type 19)
struct MsgVoiceTarget {
  uint32_t id = 0;
  std::vector<MsgVoiceTargetTarget> targets;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgUserStats (type 22)
struct MsgUserStats {
  uint32_t session = 0;
  bool stats_only = false;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgRequestBlob (type 23)
struct MsgRequestBlob {
  std::vector<uint32_t> session_texture;
  std::vector<uint32_t> session_comment;
  std::vector<uint32_t> channel_description;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgSuggestConfig (type 25)
struct MsgSuggestConfig {
  uint32_t version_v1 = 0;
  uint64_t version_v2 = 0;
  bool positional = false;
  bool push_to_talk = false;

  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgPluginDataTransmission (type 26)
struct MsgPluginDataTransmission {
  uint32_t sender_session = 0;
  std::vector<uint32_t> receiver_sessions;
  std::vector<uint8_t> data;
  std::string data_id;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

// MsgPermissionQuery (type 20)
struct MsgPermissionQuery {
  uint32_t channel_id = 0;
  uint32_t permissions = 0;
  bool flush = false;

  void marshal(std::vector<uint8_t> &out) const;
  bool unmarshal(const uint8_t *data, size_t len);
};

} // namespace mumble
} // namespace esphome
