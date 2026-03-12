#include "mumble_component.h"
#include "mumble_channel_select.h"
#include "esphome/core/log.h"
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include "esp_mac.h"

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble";

std::string MumbleComponent::get_server() const {
  if (server_text_ != nullptr && !server_text_->state.empty()) {
    return server_text_->state;
  }
  return server_;
}

uint16_t MumbleComponent::get_port() const {
  if (port_text_ != nullptr && !port_text_->state.empty()) {
    int v = std::atoi(port_text_->state.c_str());
    if (v < 1) return 1;
    if (v > 65535) return 65535;
    return static_cast<uint16_t>(v);
  }
  return port_;
}

std::string MumbleComponent::get_mac_based_username() const {
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    return "";
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "esp32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  return std::string(buf);
}

std::string MumbleComponent::get_username() const {
  if (username_text_ != nullptr && !username_text_->state.empty()) {
    return username_text_->state;
  }
  if (!username_.empty()) {
    return username_;
  }
  // Default: esp32-<MAC>; user can overwrite via username text entity
  return get_mac_based_username();
}

void MumbleComponent::seed_username_default_if_empty() {
  if (username_text_ == nullptr) return;
  if (!username_text_->state.empty()) return;
  std::string def = get_mac_based_username();
  if (def.empty()) return;
  auto call = username_text_->make_call();
  call.set_value(def);
  call.perform();
}

void MumbleComponent::publish_empty_text_defaults() {
  // ESPHome's TemplateText skips publish_state for empty initial values,
  // leaving HA showing "unknown". Force-publish empty string so HA shows "".
  if (password_text_ != nullptr && !password_text_->has_state()) {
    password_text_->make_call().set_value("").perform();
  }
  if (channel_text_ != nullptr && !channel_text_->has_state()) {
    channel_text_->make_call().set_value("").perform();
  }
}

std::string MumbleComponent::get_password() const {
  if (password_text_ != nullptr) {
    return password_text_->state;
  }
  return password_;
}

std::string MumbleComponent::get_channel() const {
  if (channel_text_ != nullptr && !channel_text_->state.empty()) {
    return channel_text_->state;
  }
  return channel_;
}

uint8_t MumbleComponent::get_mode() const {
  if (mode_select_ != nullptr && !mode_select_->current_option().empty()) {
    const std::string &s = mode_select_->current_option();
    if (s == "push_to_talk") return 1;
    if (s == "always_on") return 0;
  }
  return mode_;
}

uint8_t MumbleComponent::get_crypto() const {
  if (crypto_select_ != nullptr && !crypto_select_->current_option().empty()) {
    const std::string &s = crypto_select_->current_option();
    if (s == "legacy") return 1;
    if (s == "lite") return 0;
  }
  return crypto_;
}

void MumbleComponent::set_microphone_enabled(bool enabled) {
  microphone_enabled_ = enabled;
  ESP_LOGD(TAG, "Microphone %s", microphone_enabled_ ? "enabled" : "disabled");
  if (microphone_switch_ != nullptr) {
    microphone_switch_->publish_state(microphone_enabled_);
  }
  // Stub: actual transmit enable when audio pipeline is implemented
}

void MumbleComponent::trigger_ptt() {
  // PTT: press-and-hold to talk (future). For now, toggle mic when PTT pin triggers.
  set_microphone_enabled(!microphone_enabled_);
}

void MumbleComponent::join_channel_by_id(uint32_t channel_id) {
  client_.join_channel_by_id(channel_id);
}

void MumbleComponent::on_shutdown() {
  // Proactive disconnect before OTA/flash/reboot so the server can clean up the session
  if (client_.is_connected() || udp_.is_started()) {
    ESP_LOGI(TAG, "Shutdown: disconnecting from Mumble server");
    client_.disconnect();
    if (udp_.is_started()) {
      udp_.stop();
      udp_.set_crypt_state(nullptr);
    }
  }
}

void MumbleComponent::reset_config() {
  ESP_LOGI(TAG, "Resetting all configuration to defaults");
  // Space updates to give NVS time between writes (avoids bogus "too long" warnings)
  if (server_text_ != nullptr) {
    server_text_->make_call().set_value("192.168.1.100").perform();
    delay(25);
  }
  if (port_text_ != nullptr) {
    port_text_->make_call().set_value("64738").perform();
    delay(25);
  }
  if (username_text_ != nullptr) {
    std::string def = get_mac_based_username();
    if (!def.empty()) {
      username_text_->make_call().set_value(def).perform();
      delay(25);
    }
  }
  if (password_text_ != nullptr) {
    password_text_->make_call().set_value("").perform();
    delay(25);
  }
  if (channel_text_ != nullptr) {
    channel_text_->make_call().set_value("").perform();
    delay(25);
  }
  if (mode_select_ != nullptr) {
    mode_select_->make_call().set_option("always_on").perform();
  }
  if (crypto_select_ != nullptr) {
    crypto_select_->make_call().set_option("legacy").perform();
  }
  set_microphone_enabled(false);
}

void MumbleComponent::log_connection_config() const {
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", get_server().c_str(), get_port());
  ESP_LOGCONFIG(TAG, "  Username: %s", get_username().c_str());
  ESP_LOGCONFIG(TAG, "  Channel: %s",
                get_channel().empty() ? "(root)" : get_channel().c_str());
}

void MumbleComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Mumble client");
  seed_username_default_if_empty();  // Expose esp32-<MAC> as default; user can overwrite
  publish_empty_text_defaults();     // Prevent HA showing "unknown" for empty fields
  log_connection_config();
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                get_mode() == 0 ? "always_on" : "push_to_talk");
  ESP_LOGCONFIG(TAG, "  Crypto: %s", get_crypto() == 0 ? "lite" : "legacy");

  client_.set_server(get_server());
  client_.set_port(get_port());
  client_.set_username(get_username());
  client_.set_password(get_password());
  client_.set_channel(get_channel());
  client_.set_crypto(get_crypto());

  if (speaker_ != nullptr) {
    opus_decoder_.init(16000, 1);
    jitter_buffer_.init(JitterBuffer::DEFAULT_CAPACITY);
    speaker_sink_.set_speaker(speaker_);
    // Don't start speaker here; start it when voice actually arrives
    client_.set_voice_packet_callback([this](const uint8_t *data, size_t len) {
      on_voice_packet(data, len);
    });
    udp_.set_audio_callback([this](const uint8_t *data, size_t len) {
      on_voice_packet(data, len);
    });
  }
}

void MumbleComponent::loop() {
  std::string server = get_server();
  uint16_t port = get_port();
  std::string username = get_username();
  std::string password = get_password();
  std::string channel = get_channel();
  uint8_t crypto = get_crypto();

  if (config_tracked_) {
    if (server != last_server_ || port != last_port_ || username != last_username_ ||
        password != last_password_ || channel != last_channel_ || crypto != last_crypto_) {
      ESP_LOGI(TAG, "Server/username/password/channel/crypto changed, reconnecting");
      client_.disconnect();
    }
  }
  config_tracked_ = true;
  last_server_ = server;
  last_port_ = port;
  last_username_ = username;
  last_password_ = password;
  last_channel_ = channel;
  last_crypto_ = crypto;

  client_.set_server(server);
  client_.set_port(port);
  client_.set_username(username);
  client_.set_password(password);
  client_.set_channel(channel);
  client_.set_crypto(crypto);
  client_.set_ca_cert(ca_cert_);
  // Feed UDP/crypto stats into client for Ping message
  MumbleCryptStateBase *cs = udp_.get_crypt_state();
  if (cs != nullptr) {
    client_.set_udp_stats(cs->good(), cs->late(), cs->lost(), cs->resync(), udp_.get_udp_packets_sent());
    // Proactive Legacy nonce resync before wrap (~256 packets)
    if (cs == &crypt_state_) {
      const uint8_t *iv = crypt_state_.get_encrypt_iv();
      if (iv != nullptr && iv[0] >= 250 && !legacy_resync_sent_) {
        client_.send_crypt_resync(iv, 16);
        legacy_resync_sent_ = true;
      } else if (iv != nullptr && iv[0] < 10) {
        legacy_resync_sent_ = false;  // wrapped, allow next resync cycle
      }
    }
  }
  client_.loop();
  connected_ = client_.is_connected();

  // Initialize crypto from CryptSetup once available
  if (connected_ && client_.has_crypt_setup()) {
    uint8_t mode = client_.get_crypt_negotiated_mode();
    const auto &key = client_.get_crypt_key();
    const auto &cn = client_.get_crypt_client_nonce();
    const auto &sn = client_.get_crypt_server_nonce();
    if (key.empty() && !sn.empty()) {
      // Nonce resync (Legacy only): server sent server_nonce only
      crypt_state_.set_decrypt_iv(sn.data(), sn.size());
      legacy_resync_sent_ = false;  // allow next proactive resync
    } else if (!crypt_initialized_) {
      if (mode == 0) {
        udp_.set_crypt_state(nullptr);
        crypt_initialized_ = true;
      } else if (mode == 1 && key.size() == 16 && cn.size() == 16 && sn.size() == 16) {
        if (crypt_state_.set_key(key.data(), key.size(), cn.data(), cn.size(), sn.data(), sn.size())) {
          udp_.set_crypt_state(&crypt_state_);
          crypt_initialized_ = true;
        }
      } else if (mode == 2 && key.size() == 32) {
        if (crypt_state_gcm_.set_key(key.data(), key.size(), cn.data(), cn.size(), sn.data(), sn.size())) {
          udp_.set_crypt_state(&crypt_state_gcm_);
          crypt_initialized_ = true;
        }
      }
    }
    client_.clear_crypt_setup();
  }

  if (connected_ && speaker_ != nullptr) {
    if (!udp_.is_started()) {
      udp_.start(server, port);
    }
    udp_.loop();
    audio_playout();
  } else if (!connected_ && udp_.is_started()) {
    udp_.stop();
    udp_.set_crypt_state(nullptr);
    crypt_initialized_ = false;
    jitter_buffer_.reset();
    voice_active_ = false;
    speaker_sink_.stop();
  }

  float rtt = client_.get_ping_rtt_ms();
  ping_ms_ = (rtt > 0) ? rtt : NAN;
  if (channel_select_ != nullptr) {
    update_channel_select();
  }
}

void MumbleComponent::on_voice_packet(const uint8_t *data, size_t len) {
  voice_recv_count_++;

  VoicePacket pkt;
  if (!parse_voice_packet(data, len, &pkt)) {
    return;
  }

  switch (pkt.codec) {
    case AudioCodec::OPUS: {
      if (!opus_decoder_.is_initialized() || pkt.frame_length == 0) {
        return;
      }
      int n = opus_decoder_.decode(pkt.frame_data, pkt.frame_length, pcm_buf_,
                                   JitterBuffer::FRAME_SAMPLES);
      if (n > 0) {
        if (!voice_active_) {
          voice_active_ = true;
          speaker_sink_.start();
          ESP_LOGI(TAG, "Voice started");
        }
        last_voice_push_ms_ = millis();
        jitter_buffer_.push(voice_frame_id_++, pcm_buf_, static_cast<size_t>(n));
      }
      break;
    }
    case AudioCodec::CELT_ALPHA:
    case AudioCodec::CELT_BETA:
    case AudioCodec::SPEEX:
      break;
    case AudioCodec::PING:
      break;  // Handled by UDP layer
  }
}

void MumbleComponent::audio_playout() {
  if (!speaker_sink_.has_speaker() || !voice_active_) return;

  uint32_t now = millis();

  // Flush all available jitter buffer frames to the speaker's ring buffer.
  // The I2S DMA drains the speaker buffer at a constant rate, so feeding it
  // in bursts is fine — the speaker handles the real-time timing.
  int frames_written = 0;
  while (frames_written < 8) {
    size_t n = jitter_buffer_.pop(pcm_buf_, JitterBuffer::FRAME_SAMPLES);
    if (n == 0) break;
    speaker_sink_.write(pcm_buf_, n);
    frames_written++;
  }

  if (frames_written > 0) return;

  // Jitter buffer empty: check if voice session has gone idle
  if ((now - last_voice_push_ms_) > VOICE_IDLE_TIMEOUT_MS) {
    voice_active_ = false;
    jitter_buffer_.reset();
    speaker_sink_.stop();
    ESP_LOGI(TAG, "Voice stopped");
    return;
  }
}

void MumbleComponent::update_channel_select() {
  if (!connected_) {
    if (channel_option_strings_.empty() || channel_option_strings_[0] != "0:(connecting...)") {
      channel_option_strings_.clear();
      channel_option_strings_.push_back("0:(connecting...)");
      channel_option_ids_.clear();
      channel_option_ids_.push_back(0);
      channel_option_ptrs_.release();
      channel_option_ptrs_.init(1);
      channel_option_ptrs_.push_back(channel_option_strings_[0].c_str());
      channel_select_->traits.set_options(channel_option_ptrs_);
      channel_select_->publish_state("0:(connecting...)");
    }
    return;
  }
  const auto &channels = client_.get_channels();
  size_t n = channels.size();
  uint32_t current_id = client_.get_current_channel_id();
  if (n == last_channel_count_ && current_id == last_current_channel_id_) {
    return;  // No change
  }
  last_channel_count_ = n;
  last_current_channel_id_ = current_id;

  client_.build_channel_tree_options(channel_option_strings_, channel_option_ids_);
  if (channel_option_strings_.empty()) {
    channel_option_strings_.push_back("0:Root");
    channel_option_ids_.push_back(0);
  }
  channel_option_ptrs_.release();
  channel_option_ptrs_.init(channel_option_strings_.size());
  for (const auto &s : channel_option_strings_) {
    channel_option_ptrs_.push_back(s.c_str());
  }
  channel_select_->traits.set_options(channel_option_ptrs_);

  ESP_LOGI(TAG, "Channel select: %zu channels from server -> %zu options",
           n, channel_option_strings_.size());
  for (size_t i = 0; i < channel_option_strings_.size() && i < 8; ++i) {
    ESP_LOGD(TAG, "  option[%zu]: %s", i, channel_option_strings_[i].c_str());
  }
  if (channel_option_strings_.size() > 8) {
    ESP_LOGD(TAG, "  ... and %zu more", channel_option_strings_.size() - 8);
  }

  // Find option string for current channel and publish
  std::string current_option;
  for (size_t i = 0; i < channel_option_ids_.size(); ++i) {
    if (channel_option_ids_[i] == current_id) {
      current_option = channel_option_strings_[i];
      break;
    }
  }
  if (current_option.empty()) current_option = channel_option_strings_[0];
  channel_select_->publish_state(current_option);
}

void MumbleComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Mumble client:");
  log_connection_config();
}

}  // namespace mumble
}  // namespace esphome
