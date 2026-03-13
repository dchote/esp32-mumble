#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "mumble_audio.h"
#include "mumble_client.h"
#include "mumble_gcm.h"
#include "mumble_ocb2.h"
#include "mumble_udp.h"
#include "mumble_voice.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/select/select.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/text/text.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mumble {

class MumbleChannelSelect;

class MumbleComponent : public Component {
 public:
  void set_server(const std::string &server) { server_ = server; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void set_channel(const std::string &channel) { channel_ = channel; }
  void set_mode(uint8_t mode) { mode_ = mode; }
  void set_crypto(uint8_t crypto) { crypto_ = crypto; }
  /** Optional: PEM of CA cert for server verification. When set, TLS verification is enabled. */
  void set_ca_cert(const std::string &pem) { ca_cert_ = pem; }
  void set_ptt_pin(GPIOPin *pin) { ptt_pin_ = pin; }
  void set_mute_pin(GPIOPin *pin) { mute_pin_ = pin; }

  void set_server_text(text::Text *t) { server_text_ = t; }
  void set_port_text(text::Text *t) { port_text_ = t; }
  void set_username_text(text::Text *t) { username_text_ = t; }
  void set_password_text(text::Text *t) { password_text_ = t; }
  void set_channel_text(text::Text *t) { channel_text_ = t; }
  void set_channel_select(MumbleChannelSelect *s) { channel_select_ = s; }
  void set_mode_select(select::Select *s) { mode_select_ = s; }
  void set_crypto_select(select::Select *s) { crypto_select_ = s; }
  void set_microphone(microphone::Microphone *m) { microphone_ = m; }
  void set_speaker(speaker::Speaker *s) { speaker_ = s; }

  bool get_microphone_enabled() const { return microphone_enabled_; }
  void set_microphone_enabled(bool enabled);
  bool is_connected() const { return connected_; }
  float get_ping_ms() const { return ping_ms_; }
  bool get_voice_active() const { return voice_active_; }
  bool get_voice_sending() const { return voice_sending_; }
  /** True if voice uses UDP; false if TCP tunnel. For Transport diagnostic. */
  bool get_voice_transport_udp() const { return udp_.is_active(); }
  std::string get_server() const;
  uint16_t get_port() const;
  std::string get_username() const;
  std::string get_password() const;
  std::string get_channel() const;
  uint8_t get_mode() const;
  uint8_t get_crypto() const;

  void trigger_ptt();  // mumble.ptt_press action: toggle mic (or future hold-to-talk via ptt_pin)
  void join_channel_by_id(uint32_t channel_id);
  void reset_config();  // Reset all config entities to defaults (diagnostic)

  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  void log_connection_config() const;
  float get_setup_priority() const override {
    return setup_priority::AFTER_CONNECTION;
  }

 private:
  std::string server_;
  uint16_t port_{64738};
  std::string username_;
  std::string password_;
  std::string channel_;
  uint8_t mode_{0};
  uint8_t crypto_{0};
  std::string ca_cert_;
  GPIOPin *ptt_pin_{nullptr};
  GPIOPin *mute_pin_{nullptr};

  text::Text *server_text_{nullptr};
  text::Text *port_text_{nullptr};
  text::Text *username_text_{nullptr};
  text::Text *password_text_{nullptr};
  text::Text *channel_text_{nullptr};
  MumbleChannelSelect *channel_select_{nullptr};
  select::Select *mode_select_{nullptr};
  select::Select *crypto_select_{nullptr};
  speaker::Speaker *speaker_{nullptr};

  bool microphone_enabled_{false};  // explicit on/off; distinct from PTT (press-and-hold)
  bool connected_{false};
  float ping_ms_{NAN};

  MumbleClient client_;
  std::string last_server_;
  uint16_t last_port_{0};
  std::string last_username_;
  std::string last_password_;
  std::string last_channel_;
  uint8_t last_crypto_{0xff};  // 0xff = not yet tracked
  bool config_tracked_{false};

  MumbleUdp udp_;
  MumbleCryptState crypt_state_;
  MumbleCryptStateGcm crypt_state_gcm_;
  bool crypt_initialized_{false};
  bool legacy_resync_sent_{false};  // throttle proactive Legacy nonce resync
  OpusAudioDecoder opus_decoder_;
  JitterBuffer jitter_buffer_;
  EsphomeSpeakerSink speaker_sink_;
  int16_t pcm_buf_[JitterBuffer::FRAME_SAMPLES];
  uint64_t voice_frame_id_{0};
  bool voice_active_{false};             // true while someone is talking (within idle timeout)
  uint32_t last_voice_push_ms_{0};       // timestamp of last decoded frame pushed to jitter buffer
  uint32_t voice_recv_count_{0};         // total voice packets received
  static constexpr uint32_t VOICE_IDLE_TIMEOUT_MS = 500;
  std::vector<std::string> channel_option_strings_;
  std::vector<uint32_t> channel_option_ids_;
  FixedVector<const char *> channel_option_ptrs_;
  size_t last_channel_count_{0};
  uint32_t last_current_channel_id_{0};

  std::string get_mac_based_username() const;
  static int32_t frame_rms(const int16_t *pcm, size_t samples);
  void send_voice_packet(const uint8_t *opus_data, size_t opus_len, bool is_terminator);
  void seed_username_default_if_empty();
  void publish_empty_text_defaults();
  void update_channel_select();
  void on_voice_packet(const uint8_t *data, size_t len);
  void audio_playout();
  void manage_i2s_bus();
  void on_microphone_data(const std::vector<uint8_t> &data);
  bool should_transmit() const;
  void audio_capture();

  enum class TxState { IDLE, CAPTURING, TRANSMITTING, TAIL };
  static constexpr size_t CAPTURE_BUF_FRAMES = 8;
  static constexpr size_t CAPTURE_BUF_SAMPLES = CAPTURE_BUF_FRAMES * OpusAudioEncoder::FRAME_SAMPLES;
  static constexpr int VAD_ATTACK_FRAMES = 3;
  static constexpr int VAD_HANGOVER_FRAMES = 15;
  static constexpr uint32_t ECHO_SUPPRESS_TAIL_MS = 100;
  static constexpr size_t TX_PACKET_BUF_SIZE = 1024;

  enum class BusOwner : uint8_t { NONE, MIC, SPEAKER };
  BusOwner bus_owner_{BusOwner::NONE};
  bool bus_releasing_{false};
  uint32_t mic_warmup_until_ms_{0};  // Delay before first mic start (fixes toggle-to-start)

  uint8_t opus_payload_buf_[OpusAudioEncoder::MAX_PAYLOAD_BYTES];
  microphone::Microphone *microphone_{nullptr};
  TxState tx_state_{TxState::IDLE};
  OpusAudioEncoder opus_encoder_;
  int16_t capture_buf_[CAPTURE_BUF_SAMPLES];
  size_t capture_write_{0};
  size_t capture_read_{0};
  size_t capture_used_{0};
  int vad_voice_frames_{0};
  int vad_silence_frames_{0};
  uint64_t tx_sequence_{0};
  bool voice_sending_{false};
  uint32_t last_voice_active_ms_{0};
  uint8_t tx_packet_buf_[TX_PACKET_BUF_SIZE];
};

template<typename... Ts>
class MumbleMicrophoneEnableAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumbleMicrophoneEnableAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(const Ts &...x) override { this->parent_->set_microphone_enabled(true); }
};

template<typename... Ts>
class MumbleMicrophoneDisableAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumbleMicrophoneDisableAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(const Ts &...x) override { this->parent_->set_microphone_enabled(false); }
};

template<typename... Ts>
class MumblePttPressAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumblePttPressAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(Ts... x) override { this->parent_->trigger_ptt(); }  // press-and-hold PTT
};

template<typename... Ts>
class MumbleResetConfigAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumbleResetConfigAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(Ts... x) override { this->parent_->reset_config(); }
};

}  // namespace mumble
}  // namespace esphome
