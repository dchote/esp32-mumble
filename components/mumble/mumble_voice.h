#pragma once

#include "mumble_varint.h"
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace mumble {

// Legacy binary voice packet codec IDs (from voice-data.md)
enum class AudioCodec : uint8_t {
  CELT_ALPHA = 0,
  PING = 1,
  SPEEX = 2,
  CELT_BETA = 3,
  OPUS = 4,
};

struct VoicePacket {
  AudioCodec codec;
  uint8_t target;
  uint32_t sender_session;
  uint64_t sequence_number;
  const uint8_t *frame_data;
  size_t frame_length;
  bool is_terminator;
};

// Parse a server-to-client voice packet (legacy binary format).
// Returns true on success. out->frame_data points into the input buffer.
bool parse_voice_packet(const uint8_t *data, size_t len, VoicePacket *out);

}  // namespace mumble
}  // namespace esphome
