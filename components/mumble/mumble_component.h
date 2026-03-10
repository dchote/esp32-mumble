#pragma once

#include "esphome/components/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
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
  void set_port_number(number::Number *n) { port_number_ = n; }
  void set_username_text(text::Text *t) { username_text_ = t; }
  void set_password_text(text::Text *t) { password_text_ = t; }
  void set_channel_text(text::Text *t) { channel_text_ = t; }
  void set_mode_select(select::Select *s) { mode_select_ = s; }

  std::string get_server() const;
  uint16_t get_port() const;
  std::string get_username() const;
  std::string get_password() const;
  std::string get_channel() const;
  uint8_t get_mode() const;

  void trigger_ptt();

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
  number::Number *port_number_{nullptr};
  text::Text *username_text_{nullptr};
  text::Text *password_text_{nullptr};
  text::Text *channel_text_{nullptr};
  select::Select *mode_select_{nullptr};

  bool ptt_active_{false};
};

class MumblePttPressAction : public Action<>, public Parented<MumbleComponent> {
 public:
  void play() override { this->parent_->trigger_ptt(); }
};

}  // namespace mumble
}  // namespace esphome
