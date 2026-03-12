#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mumble {

class MumbleComponent;

class MumbleChannelSelect : public select::Select, public Component {
 public:
  void set_parent(MumbleComponent *parent) { parent_ = parent; }
  void setup() override;
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

 private:
  MumbleComponent *parent_{nullptr};
};

}  // namespace mumble
}  // namespace esphome
