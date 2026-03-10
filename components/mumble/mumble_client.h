#pragma once

#include <memory>
#include <string>

namespace esphome {
namespace mumble {

class MumbleClient {
 public:
  MumbleClient() = default;
  ~MumbleClient() = default;

  void set_server(const std::string &server) { server_ = server; }
  void set_port(uint16_t port) { port_ = port; }

  // Stub: initiate TCP connect. No TLS or full handshake yet.
  bool connect();
  void disconnect();
  bool is_connected() const { return connected_; }
  void loop();

 private:
  std::string server_;
  uint16_t port_{64738};
  bool connected_{false};
};

}  // namespace mumble
}  // namespace esphome
