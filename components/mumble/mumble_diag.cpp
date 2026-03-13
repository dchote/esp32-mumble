#include "mumble_diag.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "lwip/sockets.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#endif

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.diag";

void mumble_log_hex(const char *tag, const char *prefix, const uint8_t *data, size_t len,
                    size_t max_bytes) {
  if (data == nullptr || len == 0) return;
  size_t n = len > max_bytes ? max_bytes : len;
  char buf[96];  // 32 bytes * 2 hex + spaces + null
  if (n * 3 > sizeof(buf) - 1) n = (sizeof(buf) - 1) / 3;
  size_t pos = 0;
  for (size_t i = 0; i < n && pos < sizeof(buf) - 4; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x ", data[i]);
  }
  if (len > max_bytes) {
    snprintf(buf + pos, sizeof(buf) - pos, "... (%zu more)", len - max_bytes);
  }
  ESP_LOGD(tag, "%s len=%zu hex=%s", prefix, len, buf);
}

void mumble_diag_run_boot() {
#ifdef USE_ESP_IDF
  ESP_LOGI(TAG, "=== UDP/IP byte-order diagnostics ===");

  // 1. Endianness check
  union {
    uint32_t u32;
    uint8_t u8[4];
  } val;
  val.u32 = 0x0A0B0C0D;  // 10.11.12.13
  bool le = (val.u8[0] == 0x0D);
  ESP_LOGI(TAG, "  Host byte order: %s", le ? "little-endian" : "big-endian");
  ESP_LOGI(TAG, "  0x0A0B0C0D in memory: %02x %02x %02x %02x",
           (unsigned) val.u8[0], (unsigned) val.u8[1], (unsigned) val.u8[2],
           (unsigned) val.u8[3]);

  // 2. htonl/ntohl behavior
  uint32_t host_val = 0xAC140D11;  // 172.20.13.17 as "logical" big-endian
  uint32_t net_val = htonl(host_val);
  uint32_t back = ntohl(net_val);
  ESP_LOGI(TAG, "  Test IP 172.20.13.17:");
  ESP_LOGI(TAG, "    As uint32 (MSB first): 0x%08lx", (unsigned long) host_val);
  ESP_LOGI(TAG, "    htonl(0xAC140D11) = 0x%08lx", (unsigned long) net_val);
  ESP_LOGI(TAG, "    ntohl(back) = 0x%08lx", (unsigned long) back);

  // 3. What getpeername/sendto expect
  // POSIX: sin_addr.s_addr is network byte order (big-endian).
  // For 172.20.13.17, the 4 bytes are AC 14 0D 11.
  struct in_addr ia;
  if (inet_aton("172.20.13.17", &ia) != 0) {
    ESP_LOGI(TAG, "  inet_aton(172.20.13.17) -> s_addr=0x%08lx",
             (unsigned long) (uint32_t) ia.s_addr);
    uint8_t *p = (uint8_t *) &ia.s_addr;
    ESP_LOGI(TAG, "    Bytes in s_addr: %02x %02x %02x %02x",
             (unsigned) p[0], (unsigned) p[1], (unsigned) p[2], (unsigned) p[3]);
    uint32_t ho = ntohl(ia.s_addr);
    ESP_LOGI(TAG, "    As dotted quad (ntohl): %lu.%lu.%lu.%lu",
             (unsigned long) ((ho >> 24) & 0xFF), (unsigned long) ((ho >> 16) & 0xFF),
             (unsigned long) ((ho >> 8) & 0xFF), (unsigned long) (ho & 0xFF));
  }

  // 4. ESP-IDF: getpeername returns host order; apply htonl for network byte order
  uint32_t for_net = htonl(ia.s_addr);
  uint32_t ho2 = ntohl(for_net);
  ESP_LOGI(TAG, "  For network order: htonl(s_addr) -> %lu.%lu.%lu.%lu",
           (unsigned long) ((ho2 >> 24) & 0xFF), (unsigned long) ((ho2 >> 16) & 0xFF),
           (unsigned long) ((ho2 >> 8) & 0xFF), (unsigned long) (ho2 & 0xFF));

  ESP_LOGI(TAG, "  => get_peer_ip() returns htonl(peer) for network byte order; used by netconn IP4_ADDR");
  ESP_LOGI(TAG, "=== end diagnostics ===");
#else
  ESP_LOGI(TAG, "Diagnostics skipped (Arduino build)");
#endif
}

}  // namespace mumble
}  // namespace esphome
