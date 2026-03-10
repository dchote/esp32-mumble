#include "mumble_component.h"
#include <cmath>
#include "esphome/core/log.h"

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
  if (port_number_ != nullptr && !std::isnan(port_number_->state)) {
    float v = port_number_->state;
    if (v < 1.0f) return 1;
    if (v > 65535.0f) return 65535;
    return static_cast<uint16_t>(v);
  }
  return port_;
}

std::string MumbleComponent::get_username() const {
  if (username_text_ != nullptr && !username_text_->state.empty()) {
    return username_text_->state;
  }
  return username_;
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

void MumbleComponent::trigger_ptt() {
  ptt_active_ = !ptt_active_;
  ESP_LOGD(TAG, "PTT %s", ptt_active_ ? "on" : "off");
  // Stub: actual transmit enable when audio pipeline is implemented
}

void MumbleComponent::log_connection_config() const {
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", get_server().c_str(), get_port());
  ESP_LOGCONFIG(TAG, "  Username: %s", get_username().c_str());
  ESP_LOGCONFIG(TAG, "  Channel: %s",
                get_channel().empty() ? "(root)" : get_channel().c_str());
}

void MumbleComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Mumble client");
  log_connection_config();
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                mode_ == 0 ? "always_on" : "push_to_talk");
  ESP_LOGCONFIG(TAG, "  Crypto: %s", crypto_ == 0 ? "lite" : "legacy");
}

void MumbleComponent::loop() {
  // Stub: no connection logic yet. When implemented, check get_server() and
  // get_username() are non-empty before attempting connect.
}

void MumbleComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Mumble client:");
  log_connection_config();
}

}  // namespace mumble
}  // namespace esphome
