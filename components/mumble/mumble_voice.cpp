#include "mumble_voice.h"
#include "mumble_varint.h"
#include <cstring>

namespace esphome {
namespace mumble {

// Client-to-server: header (codec|target), sequence, payload_len (bit13=terminator), opus
static constexpr uint8_t HEADER_OPUS_NORMAL = (static_cast<uint8_t>(AudioCodec::OPUS) << 5) | 0;  // target 0 = normal

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

size_t build_voice_packet(uint8_t *out_buf, size_t max_len, uint64_t sequence,
                          const uint8_t *opus_data, size_t opus_len, bool is_terminator) {
  if (out_buf == nullptr || max_len < 1 + MUMBLE_VARINT_MAX_LEN * 2 + opus_len ||
      (opus_data == nullptr && opus_len > 0))
    return 0;
  size_t pos = 0;
  out_buf[pos++] = HEADER_OPUS_NORMAL;
  size_t n = mumble_varint_encode(out_buf + pos, static_cast<int64_t>(sequence));
  pos += n;
  uint64_t len_val = opus_len;
  if (is_terminator) len_val |= (1ULL << 13);
  n = mumble_varint_encode(out_buf + pos, static_cast<int64_t>(len_val));
  pos += n;
  if (pos + opus_len > max_len) return 0;
  if (opus_len > 0 && opus_data != nullptr) {
    memcpy(out_buf + pos, opus_data, opus_len);
    pos += opus_len;
  }
  return pos;
}

}  // namespace mumble
}  // namespace esphome
