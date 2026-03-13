# 0006 -- ESP-IDF Migration and Socket Abstraction

## Summary

Migrates the Mumble component from Arduino-only networking to a dual-backend design: ESP-IDF (esp_tls, lwip) and Arduino (WiFiClientSecure, WiFiUDP). Adds mic warmup delay and reduces debug logging verbosity.

**Current status (2025-03)**: All board configs use **Arduino** because ESP-IDF/lwIP has a known issue where unicast UDP packets do not leave the device (send() returns success but packets never appear on the wire). Arduino WiFiUDP works correctly. Development and validation proceed on Arduino; ESP-IDF debugging will resume once Arduino is fully validated.

## Motivation

- **Arduino I2S limitations**: ESPHome documents I2S microphone issues with Arduino on ESP32 ([issue #7126](https://github.com/esphome/issues/issues/7126)); wake-word-voice-assistants uses ESP-IDF on Box hardware and works.
- **Mic initialization**: "Mic does not start unless toggle on/off" — first enable after boot could race with I2S/ES7210 init.
- **Framework flexibility**: Allow Box to use ESP-IDF while keeping Arduino support for other boards.

## Implementation

### 1. Socket Abstraction (mumble_socket.h, mumble_socket.cpp)

- **TlsClient**: `connect()`, `connected()`, `available()`, `read()`, `write()`, `stop()`, `set_ca_cert()`, `set_insecure()`
- **UdpSocket**: `begin()`, `stop()`, `parse_packet()`, `read()`, `flush()`, `begin_packet()`, `write()`, `end_packet()`
- **Helpers**: `mumble_millis()`, `mumble_delay_ms()`, `mumble_dns_lookup()` — framework-agnostic wrappers

### 2. ESP-IDF Backend (USE_ESP_IDF)

- TLS: `esp_tls_init()`, `esp_tls_conn_new_sync()`, `esp_tls_conn_destroy()`, `esp_tls_get_bytes_avail()`, `esp_tls_conn_read/write`
- UDP: lwip `socket()`, `sendto`, `recvfrom`, `bind`
- DNS: `getaddrinfo()`

### 3. Arduino Backend

- Wraps `WiFiClientSecure` and `WiFiUDP` behind the same interfaces.

### 4. mbedtls Linking (ESP-IDF)

- Mumble crypto (GCM, AES/OCB2) uses mbedtls directly. The main component's `idf_component_register` has no REQUIRES; mbedtls wasn't linked.
- **Pre-build script** (`scripts/patch_mbedtls_requires.py`): patches `src/CMakeLists.txt` to add `REQUIRES mbedtls` before compile.

### 5. Mic Warmup (mumble_component.cpp)

- When transitioning bus from NONE to MIC, add 80 ms delay before `microphone_->start()` to allow I2S/ES7210 to stabilize.

### 6. Debug Logging

- "Mic raw first 32 bytes" and "Mic diag: capture_used=..., max_rms=..." downgraded to `ESP_LOGD` (visible only at DEBUG level).

## Files Changed

| File | Change |
|------|--------|
| components/mumble/mumble_socket.h | New: TlsClient, UdpSocket interfaces; factory functions |
| components/mumble/mumble_socket.cpp | New: ESP-IDF and Arduino backends |
| components/mumble/mumble_client.h/cpp | Use TlsClient\*; mumble_millis, mumble_dns_lookup |
| components/mumble/mumble_udp.h/cpp | Use UdpSocket\*; mumble_millis, mumble_dns_lookup |
| components/mumble/mumble_component.cpp | mumble_millis, mumble_delay_ms; mic warmup; ESP_LOGD for diag |
| components/mumble/__init__.py | Conditional Arduino libs (WiFi, NetworkClientSecure) only when using Arduino |
| esphome/esp32-s3-box.yaml | framework: type: arduino (ESP-IDF has UDP issues; use Arduino) |
| scripts/patch_mbedtls_requires.py | New: patch src/CMakeLists.txt REQUIRES mbedtls |
| scripts/build.sh | Add box target |

## Build Requirements

- **ESP32-S3 Box / Box-3**: ESPHome 2025.5.0+, framework arduino (UDP works)
- **ESP-IDF backend**: Available but unicast UDP broken; not used in configs

## Migration Notes

- Partition table changes when switching framework; first flash after ESP-IDF migration must be via USB, not OTA.
- Mic diag logs: set component log level to DEBUG to see raw bytes and capture_used/max_rms.
