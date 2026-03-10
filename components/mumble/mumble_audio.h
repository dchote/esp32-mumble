#pragma once

namespace esphome {
namespace mumble {

// Audio pipeline interfaces - stubs for future I2S/Opus integration.
// MicrophoneSource: provides mono 16 kHz PCM frames.
// SpeakerSink: accepts mono 16 kHz PCM frames.
// See docs/technical-overview.md for full pipeline design.

class MicrophoneSource {
 public:
  virtual ~MicrophoneSource() = default;
  // Stub: no implementation yet
};

class SpeakerSink {
 public:
  virtual ~SpeakerSink() = default;
  // Stub: no implementation yet
};

}  // namespace mumble
}  // namespace esphome
