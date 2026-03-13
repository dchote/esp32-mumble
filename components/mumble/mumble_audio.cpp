#include "mumble_audio.h"
#include "esphome/core/log.h"
#include <opus.h>
#include <algorithm>
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.audio";

// --- OpusAudioDecoder ---

bool OpusAudioDecoder::init(int sample_rate, int channels) {
  destroy();
  sample_rate_ = sample_rate;
  channels_ = channels;
  int err = 0;
  OpusDecoder *dec = opus_decoder_create(sample_rate, channels, &err);
  if (dec == nullptr || err != OPUS_OK) {
    ESP_LOGE(TAG, "opus_decoder_create failed: %d", err);
    return false;
  }
  decoder_ = dec;
  ESP_LOGI(TAG, "Opus decoder init %dHz mono", sample_rate);
  return true;
}

void OpusAudioDecoder::destroy() {
  if (decoder_ != nullptr) {
    opus_decoder_destroy(static_cast<OpusDecoder *>(decoder_));
    decoder_ = nullptr;
  }
}

int OpusAudioDecoder::decode(const uint8_t *opus_data, size_t opus_len,
                             int16_t *out_pcm, int max_samples) {
  if (decoder_ == nullptr || out_pcm == nullptr) return -1;
  int n = opus_decode(static_cast<OpusDecoder *>(decoder_),
                      opus_data, static_cast<opus_int32>(opus_len),
                      out_pcm, max_samples, 0);
  return n;
}

int OpusAudioDecoder::decode_lost(int16_t *out_pcm, int max_samples) {
  if (decoder_ == nullptr || out_pcm == nullptr) return -1;
  int n = opus_decode(static_cast<OpusDecoder *>(decoder_),
                      nullptr, 0, out_pcm, max_samples, 0);
  return n;
}

// --- OpusAudioEncoder ---

bool OpusAudioEncoder::init(int sample_rate, int channels) {
  destroy();
  sample_rate_ = sample_rate;
  channels_ = channels;
  int err = 0;
  OpusEncoder *enc = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
  if (enc == nullptr || err != OPUS_OK) {
    ESP_LOGE(TAG, "opus_encoder_create failed: %d", err);
    return false;
  }
  encoder_ = enc;
  opus_encoder_ctl(static_cast<OpusEncoder *>(encoder_), OPUS_SET_BITRATE(16000));
  opus_encoder_ctl(static_cast<OpusEncoder *>(encoder_), OPUS_SET_COMPLEXITY(1));
  opus_encoder_ctl(static_cast<OpusEncoder *>(encoder_), OPUS_SET_VBR(1));
  opus_encoder_ctl(static_cast<OpusEncoder *>(encoder_), OPUS_SET_DTX(1));
  ESP_LOGI(TAG, "Opus encoder init %dHz mono", sample_rate);
  return true;
}

void OpusAudioEncoder::destroy() {
  if (encoder_ != nullptr) {
    opus_encoder_destroy(static_cast<OpusEncoder *>(encoder_));
    encoder_ = nullptr;
  }
}

int OpusAudioEncoder::encode(const int16_t *pcm, size_t samples, uint8_t *out, size_t max_out) {
  if (encoder_ == nullptr || out == nullptr || pcm == nullptr) return -1;
  if (samples != FRAME_SAMPLES || max_out < MAX_PAYLOAD_BYTES) return -1;
  int n = opus_encode(static_cast<OpusEncoder *>(encoder_),
                      pcm, static_cast<int>(samples), out, static_cast<opus_int32>(max_out));
  return n;
}

// --- JitterBuffer ---

void JitterBuffer::init(size_t capacity_frames) {
  capacity_ = capacity_frames;
  target_depth_ = std::min(capacity_ / 2, DEFAULT_TARGET_DEPTH);
  if (target_depth_ < 1) target_depth_ = 1;
  frames_.resize(capacity_);
  reset();
}

void JitterBuffer::reset() {
  for (auto &f : frames_) {
    f.valid = false;
  }
  next_playout_seq_ = 0;
  write_idx_ = 0;
  read_idx_ = 0;
  buffered_ = 0;
  playout_started_ = false;
}

void JitterBuffer::push(uint64_t sequence, const int16_t *pcm, size_t samples) {
  if (capacity_ == 0 || pcm == nullptr) return;

  // Late packet - drop
  if (playout_started_ && sequence < next_playout_seq_) {
    return;
  }

  // First packet - set base sequence
  if (!playout_started_) {
    next_playout_seq_ = sequence;
    playout_started_ = true;
  }

  size_t idx = write_idx_ % capacity_;
  Frame &f = frames_[idx];
  f.sequence = sequence;
  f.sample_count = std::min(samples, static_cast<size_t>(FRAME_SAMPLES));
  memcpy(f.pcm, pcm, f.sample_count * sizeof(int16_t));
  f.valid = true;
  write_idx_++;
  if (buffered_ < capacity_) buffered_++;
}

size_t JitterBuffer::pop(int16_t *out_pcm, size_t max_samples) {
  if (capacity_ == 0 || out_pcm == nullptr) return 0;

  // Wait until we have target_depth frames before first playout
  if (buffered_ < target_depth_) return 0;

  size_t idx = read_idx_ % capacity_;
  Frame &f = frames_[idx];
  if (!f.valid) {
    return 0;  // Underrun - caller should use PLC
  }

  size_t n = std::min(f.sample_count, max_samples);
  memcpy(out_pcm, f.pcm, n * sizeof(int16_t));
  f.valid = false;
  read_idx_++;
  buffered_--;
  next_playout_seq_++;
  return n;
}

bool JitterBuffer::has_playout_ready() const {
  return buffered_ >= target_depth_;
}

// --- EsphomeSpeakerSink ---

void EsphomeSpeakerSink::write(const int16_t *pcm, size_t samples) {
  if (speaker_ == nullptr || pcm == nullptr || samples == 0) return;
  size_t bytes = samples * sizeof(int16_t);
  speaker_->play(reinterpret_cast<const uint8_t *>(pcm), bytes);
}

void EsphomeSpeakerSink::start() {
  if (speaker_ != nullptr) {
    speaker_->start();
    speaker_->set_volume(speaker_->get_volume());
  }
}

void EsphomeSpeakerSink::stop() {
  if (speaker_ != nullptr) {
    speaker_->stop();
  }
}

}  // namespace mumble
}  // namespace esphome
