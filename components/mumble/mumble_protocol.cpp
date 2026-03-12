#include "mumble_protocol.h"
#include <cstring>

namespace esphome {
namespace mumble {

static uint16_t be16(const uint8_t *p) {
  return (uint16_t) p[0] << 8 | p[1];
}
static uint32_t be32(const uint8_t *p) {
  return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 |
         (uint32_t) p[2] << 8 | p[3];
}

bool read_tcp_header(const uint8_t *buf, size_t len, uint16_t *out_type,
                     uint32_t *out_payload_len) {
  if (len < TCP_HEADER_SIZE) {
    return false;
  }
  *out_type = be16(buf);
  *out_payload_len = be32(buf + 2);
  if (*out_payload_len > MAX_PAYLOAD_SIZE) {
    return false;
  }
  return true;
}

static void put_be16(uint8_t *p, uint16_t v) {
  p[0] = (v >> 8) & 0xFF;
  p[1] = v & 0xFF;
}
static void put_be32(uint8_t *p, uint32_t v) {
  p[0] = (v >> 24) & 0xFF;
  p[1] = (v >> 16) & 0xFF;
  p[2] = (v >> 8) & 0xFF;
  p[3] = v & 0xFF;
}

size_t write_tcp_header(uint8_t *buf, uint16_t type, uint32_t payload_len) {
  put_be16(buf, type);
  put_be32(buf + 2, payload_len);
  return TCP_HEADER_SIZE;
}

// --- Protobuf wire encoder ---
void proto_append_tag(std::vector<uint8_t> &out, int field_num, int wire_type) {
  proto_append_varint(out, (static_cast<uint64_t>(field_num) << 3) | static_cast<uint64_t>(wire_type));
}

void proto_append_varint(std::vector<uint8_t> &out, uint64_t v) {
  while (v >= 0x80) {
    out.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  out.push_back(static_cast<uint8_t>(v));
}

void proto_append_fixed32(std::vector<uint8_t> &out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void proto_append_string(std::vector<uint8_t> &out, int field_num, const std::string &s) {
  proto_append_tag(out, field_num, WIRE_LENGTH_DELIMITED);
  proto_append_varint(out, s.size());
  for (char c : s) {
    out.push_back(static_cast<uint8_t>(c));
  }
}

void proto_append_bytes(std::vector<uint8_t> &out, int field_num, const uint8_t *data, size_t len) {
  proto_append_tag(out, field_num, WIRE_LENGTH_DELIMITED);
  proto_append_varint(out, len);
  for (size_t i = 0; i < len; i++) {
    out.push_back(data[i]);
  }
}

void proto_append_bool(std::vector<uint8_t> &out, int field_num, bool value) {
  proto_append_tag(out, field_num, WIRE_VARINT);
  proto_append_varint(out, value ? 1 : 0);
}

// --- Protobuf wire decoder ---
int proto_read_varint(const uint8_t *buf, size_t len, uint64_t *out_value) {
  uint64_t x = 0;
  unsigned int s = 0;
  for (size_t i = 0; i < len && i < 10; i++) {
    uint8_t c = buf[i];
    x |= static_cast<uint64_t>(c & 0x7F) << s;
    if (c < 0x80) {
      *out_value = x;
      return static_cast<int>(i + 1);
    }
    s += 7;
  }
  return 0;
}

int proto_read_tag(const uint8_t *buf, size_t len, int *out_field_num, int *out_wire_type) {
  uint64_t v;
  int n = proto_read_varint(buf, len, &v);
  if (n == 0) return 0;
  *out_field_num = static_cast<int>(v >> 3);
  *out_wire_type = static_cast<int>(v & 7);
  return n;
}

int proto_read_length_delimited(const uint8_t *buf, size_t len, const uint8_t **out_data,
                                size_t *out_data_len) {
  uint64_t l;
  int n = proto_read_varint(buf, len, &l);
  if (n == 0) return 0;
  if (static_cast<size_t>(n) + static_cast<size_t>(l) > len) return 0;
  *out_data = buf + n;
  *out_data_len = static_cast<size_t>(l);
  return n + static_cast<int>(l);
}

uint32_t proto_read_fixed32(const uint8_t *buf) {
  return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
         (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

int proto_skip_field(const uint8_t *buf, size_t len, int wire_type) {
  switch (wire_type) {
    case WIRE_VARINT: {
      uint64_t v;
      return proto_read_varint(buf, len, &v);
    }
    case WIRE_FIXED64:
      return (len >= 8) ? 8 : 0;
    case WIRE_LENGTH_DELIMITED: {
      uint64_t l;
      int n = proto_read_varint(buf, len, &l);
      if (n == 0) return 0;
      if (static_cast<size_t>(n) + static_cast<size_t>(l) > len) return 0;
      return n + static_cast<int>(l);
    }
    case WIRE_FIXED32:
      return (len >= 4) ? 4 : 0;
    default:
      return 0;
  }
}

}  // namespace mumble
}  // namespace esphome
