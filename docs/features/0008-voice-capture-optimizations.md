# 0008 — Voice Capture Optimizations

## Summary

Combines the voice capture pipeline, ESP-IDF migration, and network/socket standardization into a single reference. Implements the full microphone-to-network transmit path with dual backend support (Arduino and ESP-IDF), portable IP handling, and UDP diagnostics for ESP-IDF debugging.

**Current status (2025-03)**: **ESP32-S3 Box and Box-3** use ESP-IDF with lwIP **netconn** API for UDP. Generic, M5Stack Atom Echo, and others use Arduino (WiFiUDP). Voice transmit uses UDP when active; TCP tunnel fallback when UDP unreachable.

---

## 1. Voice Capture, Encoding, and Transmission

### 1.1 Opus Encoder (mumble_audio.h/.cpp)

- **OpusAudioEncoder**: `opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP)`, 16 kbps, complexity 1, VBR and DTX enabled.
- **encode()**: One 20 ms frame (320 samples) in, Opus payload out.
- Encoder state on heap (PSRAM when available via micro-opus).

### 1.2 Voice Packet Builder (mumble_voice.h/.cpp)

- **build_voice_packet()**: Client-to-server legacy format: header byte (Opus codec 4, target 0), sequence varint, payload length varint (bit 13 = terminator), Opus payload.

### 1.3 TCP Tunnel Fallback (mumble_client.h/.cpp)

- **send_udp_tunnel()**: Sends raw voice packet in TCP message type 1 (UDPTunnel) when UDP is not active.

### 1.4 Transmit Pipeline (mumble_component.h/.cpp)

- **Capture**: `on_microphone_data()` → ring buffer (8 frames = 2560 samples).
- **VAD**: Energy-based RMS per 20 ms frame; threshold 200; 3 frames above = voice start, 15 frames below = voice stop (terminator sent).
- **Echo suppression**: In always_on mode, TX suppressed while receiving or within 100 ms after.
- **Half-duplex I2S**: `manage_i2s_bus()` coordinates mic/speaker handoff to avoid "Parent I2S bus not free".
- **State machine**: IDLE → CAPTURING → TRANSMITTING → CAPTURING.
- **Send path**: Encode → build_voice_packet() → `udp_.send_audio()` if UDP active, else `client_.send_udp_tunnel()`.
- **Voice Sending** binary sensor: true while in TRANSMITTING.

### 1.5 Mic Warmup

- When transitioning bus from NONE to MIC, add 80 ms delay before `microphone_->start()` for I2S/ES7210 stability.

### 1.6 Constants

- Sample rate: 16 kHz; frame: 320 samples (20 ms).
- VAD threshold: 200 RMS; attack 3 frames; hangover 15 frames.
- Echo suppress tail: 100 ms.
- Capture buffer: 8 frames. Opus: 16 kbps, complexity 1, VOIP, DTX on.

---

## 2. ESP-IDF Migration and Socket Abstraction

### 2.1 Motivation

- Arduino I2S limitations on ESP32; wake-word-voice-assistants uses ESP-IDF on Box.
- Mic initialization race; framework flexibility for Box vs other boards.

### 2.2 Socket Abstraction (mumble_socket.h, mumble_socket.cpp)

- **TlsClient**: `connect()`, `connected()`, `available()`, `read()`, `write()`, `stop()`, `set_ca_cert()`, `set_insecure()`, `get_peer_ip()`
- **UdpSocket**: `begin()`, `stop()`, `parse_packet()`, `read()`, `flush()`, `begin_packet()`, `write()`, `end_packet()`, `connect_remote()`
- **Helpers**: `mumble_millis()`, `mumble_delay_ms()`, `mumble_dns_lookup()`

### 2.3 Backends

- **ESP-IDF** (USE_ESP_IDF): esp_tls, lwIP **netconn** API for UDP (netconn_sendto), BSD sockets for TCP. BSD sendto() had unicast UDP issues; netconn uses the tcpip task path and works.
- **Arduino**: WiFiClientSecure, WiFiUDP behind same interfaces.

### 2.4 mbedtls Linking (ESP-IDF)

- Pre-build script `scripts/patch_mbedtls_requires.py` patches `src/CMakeLists.txt` to add `REQUIRES mbedtls`. Fixed to handle PIOFRAMEWORK as string or list.

### 2.5 Component and Config

- `__init__.py`: Conditional WiFi/NetworkClientSecure only when Arduino; `microphone_id` schema.
- YAML: `microphone_id`, "Voice Sending" binary sensor on all board configs.

---

## 3. Network Standardization and UDP Debugging

### 3.1 IP Representation (mumble_network_types.h)

- **mumble_ip_t**: typedef for IP in network byte order (big-endian). All component IPs use this.
- **mumble_ip_to_dotted()**: Format IP for logging. Used in mumble_udp.cpp and mumble_socket.cpp.

### 3.2 UDP Diagnostics

- In `UdpSocketNetconn::end_packet()`: log target IP:port and netconn_sendto err on failure. Transport text sensor shows UDP vs TCP.

### 3.3 ESP-IDF Board Config

- **esp32-s3-box.yaml**: Uses `framework: type: esp-idf` with lwIP netconn for UDP. sdkconfig_options include CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY for TLS. First flash after switching framework must be via USB, not OTA.

### 3.4 Diagnostics

- `mumble_diag_run_boot()`: Byte-order and sockaddr diagnostics; runs only when USE_ESP_IDF defined.

---

## 4. Files Changed (Consolidated)

| File | Change |
|------|--------|
| components/mumble/mumble_audio.h/.cpp | OpusAudioEncoder |
| components/mumble/mumble_voice.h/.cpp | build_voice_packet() |
| components/mumble/mumble_client.h/.cpp | send_udp_tunnel(), TlsClient, mumble_dns_lookup |
| components/mumble/mumble_socket.h/.cpp | TlsClient, UdpSocket, ESP-IDF and Arduino backends |
| components/mumble/mumble_udp.h/.cpp | UdpSocket, mumble_ip_to_dotted logging |
| components/mumble/mumble_network_types.h | mumble_ip_t, mumble_ip_to_dotted |
| components/mumble/mumble_component.h/.cpp | Tx pipeline, manage_i2s_bus, mic warmup |
| components/mumble/mumble_diag.h | Diagnostics run comment |
| components/mumble/__init__.py | microphone_id, conditional Arduino libs |
| esphome/esp32-s3-box.yaml | framework esp-idf, lwIP netconn UDP, microphone, Voice Sending |
| esphome/esp32-s3-box3.yaml | framework esp-idf, lwIP netconn UDP, microphone, Voice Sending |
| esphome/m5stack-atom-echo.yaml, generic-esp32s3.yaml | microphone, Voice Sending |
| scripts/patch_mbedtls_requires.py | REQUIRES mbedtls, PIOFRAMEWORK fix |

---

## 5. Build Requirements

- **ESP32-S3 Box / Box-3**: ESPHome 2025.5.0+, framework **esp-idf** (lwIP netconn for UDP).
- **Generic, M5Stack Atom Echo**: framework arduino (WiFiUDP).
- Partition table changes when switching framework; first flash after ESP-IDF switch must be via USB, not OTA.
