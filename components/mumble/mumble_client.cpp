#include "mumble_client.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.client";

bool MumbleClient::connect() {
  ESP_LOGD(TAG, "connect stub: %s:%u", server_.c_str(), port_);
  connected_ = false;
  return false;  // Stub: no actual connection
}

void MumbleClient::disconnect() {
  connected_ = false;
  ESP_LOGD(TAG, "disconnect");
}

void MumbleClient::loop() {
  // Stub: no message handling
}

}  // namespace mumble
}  // namespace esphome
