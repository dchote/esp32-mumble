#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome {
namespace mumble {

// Time helpers (framework-agnostic)
uint32_t mumble_millis();
void mumble_delay_ms(uint32_t ms);

// TLS client abstraction
class TlsClient {
 public:
  virtual ~TlsClient() = default;
  virtual void set_ca_cert(const char *pem) = 0;
  virtual void set_insecure() = 0;
  virtual bool connect(const char *host, uint16_t port, uint32_t timeout_ms) = 0;
  virtual bool connect(uint32_t ip, uint16_t port, uint32_t timeout_ms) = 0;
  /** For async backends: poll connect progress. 1=done, 0=in progress, -1=failed. Default: -1 (sync backend). */
  virtual int connect_poll() { return -1; }
  /** For async backends: true if connect started but not yet complete. Default: false. */
  virtual bool connect_in_progress() const { return false; }
  virtual void stop() = 0;
  virtual bool connected() const = 0;
  virtual int available() const = 0;
  virtual int read(uint8_t *buf, size_t len) = 0;
  virtual size_t write(const uint8_t *buf, size_t len) = 0;
  virtual int last_error(char *buf, size_t buf_len) = 0;
  /** Peer IP in network byte order when connected; 0 if not available. */
  virtual uint32_t get_peer_ip() const { return 0; }
  /** Local IP in network byte order when connected; 0 if not available. Use for UDP bind. */
  virtual uint32_t get_local_ip() const { return 0; }
};

// UDP socket abstraction
class UdpSocket {
 public:
  virtual ~UdpSocket() = default;
  /** Start UDP. local_ip in network byte order; 0 = INADDR_ANY. */
  virtual bool begin(uint16_t local_port, uint32_t local_ip = 0) = 0;
  virtual void stop() = 0;
  virtual int parse_packet() = 0;
  virtual int read(uint8_t *buf, size_t len) = 0;
  virtual void flush() = 0;  // Consume remaining data after read
  virtual bool begin_packet(uint32_t ip, uint16_t port) = 0;
  virtual size_t write(const uint8_t *buf, size_t len) = 0;
  virtual bool end_packet() = 0;
  /** Optional: connect UDP socket to remote (enables send() instead of sendto). ESP-IDF workaround. */
  virtual bool connect_remote(uint32_t ip, uint16_t port) { (void) ip; (void) port; return false; }
};

// Resolve hostname to IP (returns 0 on failure)
uint32_t mumble_dns_lookup(const char *host);

// Factory
TlsClient *mumble_create_tls_client();
UdpSocket *mumble_create_udp_socket();
void mumble_free_tls_client(TlsClient *c);
void mumble_free_udp_socket(UdpSocket *s);

}  // namespace mumble
}  // namespace esphome
