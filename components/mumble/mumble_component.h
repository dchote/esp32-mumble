#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mumble {

class MumbleComponent : public Component {
 public:
  void set_server(const std::string &server) { server_ = server; }
  void set_port(uint16_t port) { port_ = port; }
  void set_username(const std::string &username) { username_ = username; }
  void set_password(const std::string &password) { password_ = password; }
  void set_channel(const std::string &channel) { channel_ = channel; }
  void set_mode(uint8_t mode) { mode_ = mode; }
  void set_crypto(uint8_t crypto) { crypto_ = crypto; }
  void set_ptt_pin(GPIOPin *pin) { ptt_pin_ = pin; }
  void set_mute_pin(GPIOPin *pin) { mute_pin_ = pin; }

  void set_server_text(text::Text *t) { server_text_ = t; }
  void set_port_text(text::Text *t) { port_text_ = t; }
  void set_username_text(text::Text *t) { username_text_ = t; }
  void set_password_text(text::Text *t) { password_text_ = t; }
  void set_channel_text(text::Text *t) { channel_text_ = t; }
  void set_mode_select(select::Select *s) { mode_select_ = s; }
  void set_microphone_switch(switch_::Switch *s) { microphone_switch_ = s; }

  bool get_microphone_enabled() const { return microphone_enabled_; }
  void set_microphone_enabled(bool enabled);
  bool is_connected() const { return connected_; }
  float get_ping_ms() const { return ping_ms_; }
  std::string get_server() const;
  uint16_t get_port() const;
  std::string get_username() const;
  std::string get_password() const;
  std::string get_channel() const;
  uint8_t get_mode() const;

  void trigger_ptt();  // for ptt_pin: press-and-hold to talk (future)

  void setup() override;
  void loop() override;
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
  GPIOPin *ptt_pin_{nullptr};
  GPIOPin *mute_pin_{nullptr};

  text::Text *server_text_{nullptr};
  text::Text *port_text_{nullptr};
  text::Text *username_text_{nullptr};
  text::Text *password_text_{nullptr};
  text::Text *channel_text_{nullptr};
  select::Select *mode_select_{nullptr};
  switch_::Switch *microphone_switch_{nullptr};

  bool microphone_enabled_{false};  // explicit on/off; distinct from PTT (press-and-hold)
  bool connected_{false};   // future: set by connection logic
  float ping_ms_{NAN};      // future: set by ping logic
};

template<typename... Ts>
class MumbleMicrophoneEnableAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumbleMicrophoneEnableAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(Ts... x) override { this->parent_->set_microphone_enabled(true); }
};

template<typename... Ts>
class MumbleMicrophoneDisableAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumbleMicrophoneDisableAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(Ts... x) override { this->parent_->set_microphone_enabled(false); }
};

template<typename... Ts>
class MumblePttPressAction : public Action<Ts...>, public Parented<MumbleComponent> {
 public:
  explicit MumblePttPressAction(MumbleComponent *parent) { this->set_parent(parent); }
  void play(Ts... x) override { this->parent_->trigger_ptt(); }  // press-and-hold PTT
};

}  // namespace mumble
}  // namespace esphome
