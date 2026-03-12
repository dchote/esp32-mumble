#pragma once

#include "mumble_voice.h"
#include "esphome/components/speaker/speaker.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace mumble {

// Opus decoder wrapper (16 kHz mono)
class OpusAudioDecoder {
 public:
  OpusAudioDecoder() = default;
  ~OpusAudioDecoder() { destroy(); }

  bool init(int sample_rate = 16000, int channels = 1);
  void destroy();

  // Decode Opus frame to PCM. Returns samples decoded, or negative on error.
  int decode(const uint8_t *opus_data, size_t opus_len, int16_t *out_pcm, int max_samples);

  // Packet loss concealment - generates one 20ms frame
  int decode_lost(int16_t *out_pcm, int max_samples);

  bool is_initialized() const { return decoder_ != nullptr; }

 private:
  void *decoder_{nullptr};  // OpusDecoder* (opaque to avoid opus.h in header)
  int sample_rate_{16000};
  int channels_{1};
};

// Simple jitter buffer for decoded PCM frames
class JitterBuffer {
 public:
  static constexpr size_t FRAME_SAMPLES = 320;  // 20ms at 16kHz
  static constexpr size_t DEFAULT_CAPACITY = 16;   // 320ms buffer (tolerates TCP burst gaps)
  static constexpr size_t DEFAULT_TARGET_DEPTH = 2;  // 40ms prebuffer; speaker ring buffer handles timing

  void init(size_t capacity_frames = DEFAULT_CAPACITY);
  void reset();

  // Push decoded PCM. Drops late packets (sequence < next_playout).
  void push(uint64_t sequence, const int16_t *pcm, size_t samples);

  // Pop next frame for playback. Returns samples, 0 if underrun.
  size_t pop(int16_t *out_pcm, size_t max_samples);

  bool has_playout_ready() const;

 private:
  struct Frame {
    uint64_t sequence;
    int16_t pcm[FRAME_SAMPLES];
    size_t sample_count;
    bool valid;
  };
  std::vector<Frame> frames_;
  uint64_t next_playout_seq_{0};
  size_t write_idx_{0};
  size_t read_idx_{0};
  size_t buffered_{0};
  size_t capacity_{0};
  size_t target_depth_{0};
  bool playout_started_{false};
};

// Simple PCM mixer (for multi-user - single-user fast path skips mixing)
class AudioMixer {
 public:
  static constexpr size_t MAX_SAMPLES = 640;

  void reset();
  void mix_in(const int16_t *src, size_t samples);
  void get_mixed(int16_t *out, size_t samples);

 private:
  int32_t acc_[MAX_SAMPLES];
  size_t max_used_{0};
};

// Speaker sink - writes PCM to ESPHome speaker
class EsphomeSpeakerSink {
 public:
  void set_speaker(speaker::Speaker *spk) { speaker_ = spk; }
  void write(const int16_t *pcm, size_t samples);
  void start();
  void stop();

  bool has_speaker() const { return speaker_ != nullptr; }

 private:
  speaker::Speaker *speaker_{nullptr};
};

}  // namespace mumble
}  // namespace esphome
