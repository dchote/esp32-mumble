#include "mumble_voice.h"
#include <cstring>

namespace esphome {
namespace mumble {

// Server-to-client legacy format:
//   Header byte: codec(3) | target(5)
//   Session (varint)
//   Sequence (varint)
//   Payload length (varint) - bit 13 = terminator
//   Audio frame data
//   Optional: positional (3x float32 LE) - we ignore
bool parse_voice_packet(const uint8_t *data, size_t len, VoicePacket *out) {
  if (data == nullptr || out == nullptr || len < 2) return false;

  out->codec = static_cast<AudioCodec>((data[0] >> 5) & 0x07);
  out->target = data[0] & 0x1F;
  out->sender_session = 0;
  out->sequence_number = 0;
  out->frame_data = nullptr;
  out->frame_length = 0;
  out->is_terminator = false;

  size_t pos = 1;
  size_t n;

  int64_t v = mumble_varint_decode(data + pos, len - pos, &n);
  if (n == 0) return false;
  pos += n;
  out->sender_session = static_cast<uint32_t>(v);

  v = mumble_varint_decode(data + pos, len - pos, &n);
  if (n == 0) return false;
  pos += n;
  out->sequence_number = static_cast<uint64_t>(v);

  v = mumble_varint_decode(data + pos, len - pos, &n);
  if (n == 0) return false;
  pos += n;
  // Mumble: bit 13 of payload length varint = terminator
  out->is_terminator = (static_cast<uint64_t>(v) & (1ULL << 13)) != 0;
  size_t payload_len = static_cast<size_t>(v) & 0x1FFF;

  if (pos + payload_len > len) return false;

  out->frame_data = data + pos;
  out->frame_length = payload_len;
  return true;
}

}  // namespace mumble
}  // namespace esphome
