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

}  // namespace mumble
}  // namespace esphome
