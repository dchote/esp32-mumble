#include "mumble_varint.h"

namespace esphome {
namespace mumble {

// Port of research/go-mumble-server/pkg/mumble/audio/varint.go
static uint32_t read_be32(const uint8_t *b) {
  return (uint32_t) b[0] << 24 | (uint32_t) b[1] << 16 |
         (uint32_t) b[2] << 8 | (uint32_t) b[3];
}

static uint64_t read_be64(const uint8_t *b) {
  return (uint64_t) read_be32(b) << 32 | read_be32(b + 4);
}

static void write_be32(uint8_t *b, uint32_t v) {
  b[0] = (v >> 24) & 0xFF;
  b[1] = (v >> 16) & 0xFF;
  b[2] = (v >> 8) & 0xFF;
  b[3] = v & 0xFF;
}

static void write_be64(uint8_t *b, uint64_t v) {
  write_be32(b, (uint32_t)(v >> 32));
  write_be32(b + 4, (uint32_t) v);
}

int64_t mumble_varint_decode(const uint8_t *buf, size_t len, size_t *bytes_read) {
  *bytes_read = 0;
  if (len == 0) return 0;

  if ((buf[0] & 0x80) == 0) {
    *bytes_read = 1;
    return (int64_t) buf[0];
  }
  if ((buf[0] & 0xC0) == 0x80 && len >= 2) {
    *bytes_read = 2;
    return (int64_t)((buf[0] & 0x3F) << 8 | buf[1]);
  }
  if ((buf[0] & 0xE0) == 0xC0 && len >= 3) {
    *bytes_read = 3;
    return (int64_t)((buf[0] & 0x1F) << 16 | (uint32_t) buf[1] << 8 | buf[2]);
  }
  if ((buf[0] & 0xF0) == 0xE0 && len >= 4) {
    *bytes_read = 4;
    return (int64_t)((buf[0] & 0x0F) << 24 | (uint32_t) buf[1] << 16 |
                     (uint32_t) buf[2] << 8 | buf[3]);
  }
  if ((buf[0] & 0xFC) == 0xF0 && len >= 5) {
    *bytes_read = 5;
    return (int64_t) read_be32(buf + 1);
  }
  if ((buf[0] & 0xFC) == 0xF4 && len >= 9) {
    *bytes_read = 9;
    return (int64_t) read_be64(buf + 1);
  }
  if ((buf[0] & 0xFC) == 0xF8) {
    size_t n;
    int64_t v = mumble_varint_decode(buf + 1, len - 1, &n);
    if (n > 0) {
      *bytes_read = 1 + n;
      return -v;
    }
    return 0;
  }
  if ((buf[0] & 0xFC) == 0xFC) {
    *bytes_read = 1;
    return ~(int64_t)(buf[0] & 0x03);
  }
  return 0;
}

size_t mumble_varint_encode(uint8_t *buf, int64_t value) {
  if (value >= -4 && value <= -1) {
    buf[0] = 0xFC | (uint8_t)(~value & 0xFF);
    return 1;
  }
  if (value < 0) {
    buf[0] = 0xF8;
    return 1 + mumble_varint_encode(buf + 1, -value);
  }
  if (value <= 0x7F) {
    buf[0] = (uint8_t) value;
    return 1;
  }
  if (value <= 0x3FFF) {
    buf[0] = (uint8_t)(((value >> 8) & 0x3F) | 0x80);
    buf[1] = (uint8_t)(value & 0xFF);
    return 2;
  }
  if (value <= 0x1FFFFF) {
    buf[0] = (uint8_t)((value >> 16) & 0x1F | 0xC0);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    buf[2] = (uint8_t)(value & 0xFF);
    return 3;
  }
  if (value <= 0x0FFFFFFF) {
    buf[0] = (uint8_t)((value >> 24) & 0x0F | 0xE0);
    buf[1] = (uint8_t)((value >> 16) & 0xFF);
    buf[2] = (uint8_t)((value >> 8) & 0xFF);
    buf[3] = (uint8_t)(value & 0xFF);
    return 4;
  }
  if (value <= 2147483647) {  // INT32_MAX
    buf[0] = 0xF0;
    write_be32(buf + 1, (uint32_t) value);
    return 5;
  }
  buf[0] = 0xF4;
  write_be64(buf + 1, (uint64_t) value);
  return 9;
}

}  // namespace mumble
}  // namespace esphome
