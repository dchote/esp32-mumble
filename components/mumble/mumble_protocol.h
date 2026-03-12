#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace esphome {
namespace mumble {

// Mumble TCP control message types (from go-mumble-server pkg/mumble/protocol/types.go)
enum MessageType : uint16_t {
  MSG_VERSION = 0,
  MSG_UDP_TUNNEL = 1,
  MSG_AUTHENTICATE = 2,
  MSG_PING = 3,
  MSG_REJECT = 4,
  MSG_SERVER_SYNC = 5,
  MSG_CHANNEL_REMOVE = 6,
  MSG_CHANNEL_STATE = 7,
  MSG_USER_REMOVE = 8,
  MSG_USER_STATE = 9,
  MSG_BAN_LIST = 10,
  MSG_TEXT_MESSAGE = 11,
  MSG_PERMISSION_DENIED = 12,
  MSG_ACL = 13,
  MSG_QUERY_USERS = 14,
  MSG_CRYPT_SETUP = 15,
  MSG_CONTEXT_ACTION_MODIFY = 16,
  MSG_CONTEXT_ACTION = 17,
  MSG_USER_LIST = 18,
  MSG_VOICE_TARGET = 19,
  MSG_PERMISSION_QUERY = 20,
  MSG_CODEC_VERSION = 21,
  MSG_USER_STATS = 22,
  MSG_REQUEST_BLOB = 23,
  MSG_SERVER_CONFIG = 24,
  MSG_SUGGEST_CONFIG = 25,
  MSG_PLUGIN_DATA_TRANSMISSION = 26,
};

// Wire types per protobuf spec (for TCP control messages)
constexpr int WIRE_VARINT = 0;
constexpr int WIRE_FIXED64 = 1;
constexpr int WIRE_LENGTH_DELIMITED = 2;
constexpr int WIRE_FIXED32 = 5;

// TCP framing: 6-byte header (type u16be + length u32be) + payload
constexpr size_t TCP_HEADER_SIZE = 6;
constexpr size_t MAX_PAYLOAD_SIZE = 8 * 1024 * 1024;  // 8 MiB

// Read TCP packet header. Returns type and payload length. Payload length 0 on error.
bool read_tcp_header(const uint8_t *buf, size_t len, uint16_t *out_type,
                     uint32_t *out_payload_len);

// Write TCP packet header into buf. Returns bytes written (always 6).
size_t write_tcp_header(uint8_t *buf, uint16_t type, uint32_t payload_len);

// --- Protobuf wire encoder (append to vector) ---
void proto_append_tag(std::vector<uint8_t> &out, int field_num, int wire_type);
void proto_append_varint(std::vector<uint8_t> &out, uint64_t value);
void proto_append_fixed32(std::vector<uint8_t> &out, uint32_t value);
void proto_append_string(std::vector<uint8_t> &out, int field_num, const std::string &s);
void proto_append_bytes(std::vector<uint8_t> &out, int field_num, const uint8_t *data, size_t len);
void proto_append_bool(std::vector<uint8_t> &out, int field_num, bool value);

// --- Protobuf wire decoder (read from buffer) ---
// Returns bytes consumed, or 0 on error/buffer too short.
int proto_read_varint(const uint8_t *buf, size_t len, uint64_t *out_value);
int proto_read_tag(const uint8_t *buf, size_t len, int *out_field_num, int *out_wire_type);
int proto_read_length_delimited(const uint8_t *buf, size_t len, const uint8_t **out_data,
                                size_t *out_data_len);
uint32_t proto_read_fixed32(const uint8_t *buf);  // caller must ensure len >= 4
int proto_skip_field(const uint8_t *buf, size_t len, int wire_type);

}  // namespace mumble
}  // namespace esphome
