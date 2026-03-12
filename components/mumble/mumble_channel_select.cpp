#include "mumble_channel_select.h"
#include "mumble_component.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cstdlib>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.channel_select";

void MumbleChannelSelect::setup() {
  this->traits.set_options({"0:(connecting...)"});
  this->publish_state("0:(connecting...)");
}

void MumbleChannelSelect::dump_config() {
  LOG_SELECT("", "Mumble Channel", this);
}

void MumbleChannelSelect::control(const std::string &value) {
  if (!parent_) return;
  // Parse "id:  Name" - channel id is before first ':'
  const char *p = value.c_str();
  char *end = nullptr;
  unsigned long id = strtoul(p, &end, 10);
  if (end == p || *end != ':') {
    ESP_LOGW(TAG, "Invalid channel option format: %s", value.c_str());
    return;
  }
  parent_->join_channel_by_id(static_cast<uint32_t>(id));
  this->publish_state(value);
}

}  // namespace mumble
}  // namespace esphome
