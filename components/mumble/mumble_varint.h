#pragma once

#include <cstdint>
#include <cstddef>

namespace esphome {
namespace mumble {

// Mumble custom varint encoding (UDP voice packets).
// NOT protobuf LEB128 - see research/go-mumble-server/pkg/mumble/audio/varint.go

constexpr size_t MUMBLE_VARINT_MAX_LEN = 10;

// Decode Mumble varint from buf. Returns value and bytes consumed. 0 bytes on error.
int64_t mumble_varint_decode(const uint8_t *buf, size_t len, size_t *bytes_read);

// Encode value as Mumble varint into buf. Returns bytes written. Caller ensures buf
// has at least MUMBLE_VARINT_MAX_LEN bytes.
size_t mumble_varint_encode(uint8_t *buf, int64_t value);

}  // namespace mumble
}  // namespace esphome
