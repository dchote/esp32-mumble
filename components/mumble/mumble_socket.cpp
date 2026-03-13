#include "mumble_socket.h"
#include "mumble_network_types.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_tls.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <atomic>
#include <cerrno>
#include <cstring>

namespace esphome {
namespace mumble {

uint32_t mumble_millis() {
  return (uint32_t)(esp_timer_get_time() / 1000);
}
void mumble_delay_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t mumble_dns_lookup(const char *host) {
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  int err = getaddrinfo(host, nullptr, &hints, &res);
  if (err != 0 || res == nullptr) return 0;
  uint32_t ip = 0;
  for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
    if (p->ai_family == AF_INET && p->ai_addr != nullptr) {
      ip = ((struct sockaddr_in *) p->ai_addr)->sin_addr.s_addr;
      break;
    }
  }
  freeaddrinfo(res);
  return ip;
}

// --- ESP-IDF TLS client ---
// Blocking connect runs in a dedicated FreeRTOS task so the ESPHome loop
// task is never starved (avoids task watchdog).
struct TlsConnectTaskParams {
  std::string host;
  uint16_t port;
  esp_tls_cfg_t *cfg;
  esp_tls_t *tls;
  std::atomic<int> result{0};       // 0=running, 1=success, -1=failed
  SemaphoreHandle_t done_sem;       // signaled when task exits
};

static void tls_connect_task(void *arg) {
  auto *p = static_cast<TlsConnectTaskParams *>(arg);
  int ret = esp_tls_conn_new_sync(p->host.c_str(), (int) p->host.size(), p->port, p->cfg, p->tls);
  p->result.store((ret == 1) ? 1 : -1, std::memory_order_release);
  xSemaphoreGive(p->done_sem);
  vTaskDelete(nullptr);
}

class TlsClientEspIdf : public TlsClient {
 public:
  TlsClientEspIdf() {
    cfg_ = {};
    task_params_.done_sem = xSemaphoreCreateBinary();
  }
  ~TlsClientEspIdf() override {
    stop();
    if (task_params_.done_sem != nullptr) {
      vSemaphoreDelete(task_params_.done_sem);
    }
  }

  void set_ca_cert(const char *pem) override {
    ca_cert_pem_ = pem;
    cfg_.cacert_pem_buf = reinterpret_cast<const unsigned char *>(ca_cert_pem_.c_str());
    cfg_.cacert_pem_bytes = ca_cert_pem_.size() + 1;
    cfg_.skip_common_name = false;
  }
  void set_insecure() override {
    cfg_.cacert_pem_buf = nullptr;
    cfg_.cacert_pem_bytes = 0;
    cfg_.skip_common_name = true;
  }
  bool connect(const char *host, uint16_t port, uint32_t timeout_ms) override {
    stop();
    cfg_.timeout_ms = (int) timeout_ms;
    cfg_.non_block = true;
    tls_ = esp_tls_init();
    if (tls_ == nullptr) return false;
    task_params_.host = host;
    task_params_.port = port;
    task_params_.cfg = &cfg_;
    task_params_.tls = tls_;
    task_params_.result.store(0, std::memory_order_relaxed);
    xSemaphoreTake(task_params_.done_sem, 0);  // reset semaphore
    BaseType_t ok = xTaskCreate(tls_connect_task, "tls_conn", 10240,
                                &task_params_, 5, &connect_task_);
    if (ok != pdPASS) {
      esp_tls_conn_destroy(tls_);
      tls_ = nullptr;
      return false;
    }
    return false;  // In progress; caller polls via connect_poll()
  }
  bool connect(uint32_t ip, uint16_t port, uint32_t timeout_ms) override {
    struct in_addr addr;
    addr.s_addr = ip;  // already network byte order from getaddrinfo
    char ipstr[16];
    inet_ntoa_r(addr, ipstr, sizeof(ipstr));
    return connect(ipstr, port, timeout_ms);
  }
  int connect_poll() override {
    if (connect_task_ == nullptr) return -1;
    int r = task_params_.result.load(std::memory_order_acquire);
    if (r == 0) return 0;  // still running
    xSemaphoreTake(task_params_.done_sem, portMAX_DELAY);
    connect_task_ = nullptr;
    if (r == 1) return 1;
    esp_tls_conn_destroy(tls_);
    tls_ = nullptr;
    return -1;
  }
  bool connect_in_progress() const override { return connect_task_ != nullptr; }
  void stop() override {
    if (connect_task_ != nullptr) {
      // Wait for the background task to finish before destroying shared resources
      xSemaphoreTake(task_params_.done_sem, portMAX_DELAY);
      connect_task_ = nullptr;
    }
    if (tls_ != nullptr) {
      esp_tls_conn_destroy(tls_);
      tls_ = nullptr;
    }
  }
  bool connected() const override { return tls_ != nullptr && connect_task_ == nullptr; }
  int available() const override {
    if (!connected()) return 0;
    ssize_t n = esp_tls_get_bytes_avail(tls_);
    return n > 0 ? (int) n : 0;
  }
  int read(uint8_t *buf, size_t len) override {
    if (!connected() || buf == nullptr) return -1;
    int n = esp_tls_conn_read(tls_, buf, len);
    if (n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE)
      return 0;  // non-blocking: no data ready yet
    return n >= 0 ? n : -1;
  }
  size_t write(const uint8_t *buf, size_t len) override {
    if (!connected() || buf == nullptr) return 0;
    int n = esp_tls_conn_write(tls_, buf, len);
    if (n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE)
      return 0;  // non-blocking: can't write yet
    return n > 0 ? (size_t) n : 0;
  }
  int last_error(char *buf, size_t buf_len) override {
    if (buf != nullptr && buf_len > 0) buf[0] = '\0';
    return tls_ == nullptr ? -1 : 0;
  }
  uint32_t get_peer_ip() const override {
    if (tls_ == nullptr) return 0;
    int fd = -1;
    if (esp_tls_get_conn_sockfd(tls_, &fd) != ESP_OK || fd < 0) return 0;
    struct sockaddr_in peer = {};
    socklen_t len = sizeof(peer);
    if (getpeername(fd, (struct sockaddr *) &peer, &len) != 0) return 0;
    // ESP-IDF/lwIP getpeername returns s_addr in host byte order; sendto expects network order.
    return htonl(peer.sin_addr.s_addr);
  }

 private:
  esp_tls_t *tls_{nullptr};
  esp_tls_cfg_t cfg_{};
  std::string ca_cert_pem_;
  TlsConnectTaskParams task_params_;
  TaskHandle_t connect_task_{nullptr};
};

// --- ESP-IDF UDP socket: lwIP netconn API (BSD sockets have UDP send issues) ---
class UdpSocketNetconn : public UdpSocket {
 public:
  UdpSocketNetconn() : conn_(nullptr) {}
  ~UdpSocketNetconn() override { stop(); }

  bool begin(uint16_t local_port, uint32_t local_ip = 0) override {
    (void) local_ip;
    stop();
    conn_ = netconn_new(NETCONN_UDP);
    if (conn_ == nullptr) return false;
    err_t err = netconn_bind(conn_, nullptr, local_port);  // nullptr = IP_ADDR_ANY
    if (err != ERR_OK) {
      netconn_delete(conn_);
      conn_ = nullptr;
      return false;
    }
    netconn_set_recvtimeout(conn_, 1);  // 1ms = poll, don't block
    return true;
  }
  void stop() override {
    if (conn_ != nullptr) {
      netconn_delete(conn_);
      conn_ = nullptr;
    }
    recv_len_ = 0;
    recv_pos_ = 0;
  }
  int parse_packet() override {
    if (conn_ == nullptr) return -1;
    struct netbuf *in = nullptr;
    err_t err = netconn_recv(conn_, &in);
    if (err != ERR_OK || in == nullptr) return -1;
    void *data = nullptr;
    u16_t len = 0;
    if (netbuf_data(in, &data, &len) != ERR_OK || data == nullptr || len == 0 || len > sizeof(recv_buf_)) {
      netbuf_delete(in);
      return -1;
    }
    memcpy(recv_buf_, data, len);
    recv_len_ = len;
    recv_pos_ = 0;
    netbuf_delete(in);
    return (int) len;
  }
  int read(uint8_t *buf, size_t len) override {
    if (buf == nullptr || recv_len_ == 0) return -1;
    size_t avail = recv_len_ - recv_pos_;
    if (len > avail) len = avail;
    memcpy(buf, recv_buf_ + recv_pos_, len);
    recv_pos_ += len;
    return (int) len;
  }
  void flush() override { recv_len_ = 0; recv_pos_ = 0; }
  bool connect_remote(uint32_t ip, uint16_t port) override {
    (void) ip;
    (void) port;
    return false;  // netconn_sendto doesn't need connect
  }
  bool begin_packet(uint32_t ip, uint16_t port) override {
    remote_ip_ = ip;
    remote_port_ = port;
    send_pos_ = 0;
    return true;
  }
  size_t write(const uint8_t *buf, size_t len) override {
    if (send_pos_ + len > sizeof(send_buf_)) len = sizeof(send_buf_) - send_pos_;
    memcpy(send_buf_ + send_pos_, buf, len);
    send_pos_ += len;
    return len;
  }
  bool end_packet() override {
    if (conn_ == nullptr || send_pos_ == 0) return false;
    static const char *const TAG = "mumble.udp";
    struct netbuf *out = netbuf_new();
    if (out == nullptr) return false;
    if (netbuf_ref(out, send_buf_, send_pos_) != ERR_OK) {
      netbuf_delete(out);
      return false;
    }
    ip_addr_t dst;
    IP4_ADDR(&dst, (uint8_t) (remote_ip_ >> 24), (uint8_t) (remote_ip_ >> 16),
             (uint8_t) (remote_ip_ >> 8), (uint8_t) (remote_ip_ & 0xff));
    err_t err = netconn_sendto(conn_, out, &dst, remote_port_);
    netbuf_delete(out);
    bool ok = (err == ERR_OK);
    if (!ok) {
      char ipbuf[16];
      mumble_ip_to_dotted(remote_ip_, ipbuf, sizeof(ipbuf));
      ESP_LOGW(TAG, "UDP netconn_sendto: target %s:%u len=%zu err=%d",
               ipbuf, remote_port_, send_pos_, (int) err);
    }
    return ok;
  }

 private:
  struct netconn *conn_{nullptr};
  uint8_t recv_buf_[1024 + 64];
  size_t recv_len_{0};
  size_t recv_pos_{0};
  uint8_t send_buf_[1024 + 64];
  size_t send_pos_{0};
  uint32_t remote_ip_{0};
  uint16_t remote_port_{0};
};

TlsClient *mumble_create_tls_client() { return new TlsClientEspIdf; }
UdpSocket *mumble_create_udp_socket() { return new UdpSocketNetconn; }
void mumble_free_tls_client(TlsClient *c) { delete c; }
void mumble_free_udp_socket(UdpSocket *s) { delete s; }

}  // namespace mumble
}  // namespace esphome

#else  // Arduino

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

namespace esphome {
namespace mumble {

uint32_t mumble_millis() { return millis(); }
void mumble_delay_ms(uint32_t ms) { delay(ms); }

uint32_t mumble_dns_lookup(const char *host) {
  IPAddress ip;
  if (!WiFi.hostByName(host, ip)) return 0;
  // Return network byte order (consistent with ESP-IDF get_peer_ip)
  return (uint32_t) ip[0] << 24 | (uint32_t) ip[1] << 16 | (uint32_t) ip[2] << 8 | ip[3];
}

class TlsClientArduino : public TlsClient {
 public:
  void set_ca_cert(const char *pem) override { client_.setCACert(pem); }
  void set_insecure() override { client_.setInsecure(); }
  bool connect(const char *host, uint16_t port, uint32_t timeout_ms) override {
    client_.setTimeout(timeout_ms / 1000);
    client_.setHandshakeTimeout(timeout_ms / 1000);
    return client_.connect(host, port, timeout_ms / 1000);
  }
  bool connect(uint32_t ip, uint16_t port, uint32_t timeout_ms) override {
    client_.setTimeout(timeout_ms / 1000);
    client_.setHandshakeTimeout(timeout_ms / 1000);
    IPAddress a((ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return client_.connect(a, port, timeout_ms / 1000);
  }
  void stop() override { client_.stop(); }
  bool connected() const override { return client_.connected(); }
  int available() const override { return client_.available(); }
  int read(uint8_t *buf, size_t len) override { return client_.read(buf, len); }
  size_t write(const uint8_t *buf, size_t len) override { return client_.write(buf, len); }
  int last_error(char *buf, size_t buf_len) override {
    return client_.lastError(buf, buf_len);
  }
  uint32_t get_peer_ip() const override {
    if (!client_.connected()) return 0;
    IPAddress ip = client_.remoteIP();
    return (uint32_t) ip[0] << 24 | (uint32_t) ip[1] << 16 | (uint32_t) ip[2] << 8 | ip[3];
  }

 private:
  mutable WiFiClientSecure client_;
};

class UdpSocketArduino : public UdpSocket {
 public:
  bool begin(uint16_t local_port, uint32_t local_ip = 0) override {
    (void) local_ip;  // Arduino WiFiUDP doesn't support bind to specific IP
    return udp_.begin(local_port);
  }
  void stop() override { udp_.stop(); }
  int parse_packet() override { return udp_.parsePacket(); }
  int read(uint8_t *buf, size_t len) override { return udp_.read(buf, len); }
  void flush() override {
    while (udp_.available()) udp_.read();
  }
  bool begin_packet(uint32_t ip, uint16_t port) override {
    IPAddress a((ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return udp_.beginPacket(a, port);
  }
  size_t write(const uint8_t *buf, size_t len) override { return udp_.write(buf, len); }
  bool end_packet() override { return udp_.endPacket(); }

 private:
  WiFiUDP udp_;
};

TlsClient *mumble_create_tls_client() { return new TlsClientArduino; }
UdpSocket *mumble_create_udp_socket() { return new UdpSocketArduino; }
void mumble_free_tls_client(TlsClient *c) { delete c; }
void mumble_free_udp_socket(UdpSocket *s) { delete s; }

}  // namespace mumble
}  // namespace esphome

#endif
