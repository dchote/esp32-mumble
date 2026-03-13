#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace esphome {
namespace mumble {

/** IP address in network byte order (big-endian).
 *  All IPs in this component (get_peer_ip, cached_ip_, mumble_dns_lookup, etc.)
 *  use this convention for portability across Arduino and ESP-IDF backends.
 *  For sockaddr_in, lwIP ip_addr_t (IP4_ADDR), and logging use mumble_ip_to_dotted(). */
using mumble_ip_t = uint32_t;

/** Format IP (network byte order) as dotted decimal. Writes to buf, returns chars written. */
inline size_t mumble_ip_to_dotted(mumble_ip_t ip, char *buf, size_t len) {
  if (buf == nullptr || len == 0) return 0;
  return static_cast<size_t>(
      snprintf(buf, len, "%u.%u.%u.%u", (unsigned)((ip >> 24) & 0xFF),
               (unsigned)((ip >> 16) & 0xFF), (unsigned)((ip >> 8) & 0xFF),
               (unsigned)(ip & 0xFF)));
}

}  // namespace mumble
}  // namespace esphome
