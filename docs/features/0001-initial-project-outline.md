# 0001 -- Initial Project Outline

## Summary

Establish the foundational documentation for the ESP32-Mumble project: a product overview defining scope and goals, a technical overview detailing architecture and implementation strategy, and an updated README as the project's front door.

## Motivation

The project is greenfield with no source code or documentation. Before writing firmware, the team needs a shared understanding of what the product does, how it will work technically, and what hardware it targets. These documents serve as the reference for all subsequent implementation work.

## Changes

### Category: Documentation

1. **Create `docs/product-overview.md`**
   - Vision statement (always-on intercom / PTT on ESP32-S3, ESPHome-compatible, LAN-only)
   - Use cases (always-on, push-to-talk, multi-room, HA integration)
   - Target hardware table (ESP32-S3 Box 3, Onju Voice, M5Stack Atom Echo, generic)
   - Feature list (audio, protocol, controls, LED indicators)
   - Home Assistant configuration entities (server host/port, username, password, channel)
   - Home Assistant runtime entities (mute, volume, channel select, talking, connected)
   - Server requirements (go-mumble-server or any Mumble server; Lite recommended, Legacy optional)
   - Non-goals for v1 (text chat, ACL, Secure crypto, CELT/Speex, positional audio)

2. **Create `docs/technical-overview.md`**
   - System architecture diagram (ESP32 <-> go-mumble-server <-> Home Assistant)
   - Mumble protocol details:
     - Connection lifecycle (TLS, Version, Authenticate, CryptSetup+Sync; correct message order)
     - TCP wire format (type u16 + length u32 + payload)
     - UDP voice packet format (header, varint sequence, varint length, Opus frame)
     - Lite crypto mode negotiation (CryptoModes=0x01, empty key)
   - Audio pipeline:
     - Capture path (I2S -> AEC -> Opus encode -> UDP)
     - Playback path (UDP -> jitter buffer -> Opus decode -> resample -> I2S)
     - FreeRTOS task layout (core pinning)
     - AEC via ESP-SR/ESP-ADF
     - Microphone array beamforming
   - ESPHome integration:
     - External component structure (`components/mumble/`)
     - YAML configuration schema
     - HA configuration entities (text/number, NVS-persisted)
     - HA runtime entities (switch, number, select, binary_sensor)
   - Hardware abstraction:
     - Board profiles (ESP32-S3 Box 3, Onju Voice, M5Stack Atom Echo, generic)
     - Pin maps and codec configurations
   - Dependencies (ESP-IDF 5.x, libopus, mbedTLS, ESPHome, ESP-ADF/ESP-SR)
   - Technical risks and mitigations

3. **Rewrite `README.md`**
   - Project name and description
   - Pre-alpha status notice
   - Supported hardware table
   - Architecture diagram
   - Links to product-overview.md and technical-overview.md
   - Prerequisites list
   - Example ESPHome YAML configuration
   - Project structure outline
   - Contributing and license sections

### Sequence

No ordering dependencies between files, but logically:
1. `docs/product-overview.md` (establishes scope)
2. `docs/technical-overview.md` (references product goals)
3. `README.md` (links to both docs)

## Risks and Open Questions

- **Opus ESP-IDF port maturity**: Need to verify that libopus compiles and runs efficiently on ESP32-S3. May require custom build flags or assembly optimizations.
- **Legacy crypto (OCB2-AES128)**: Standard Mumble support; Legacy is the default crypto mode. Lite mode is optional for trusted LAN.
- **AEC effectiveness**: ESP-SR's AEC may not be sufficient for all hardware configurations. Always-on mode may need to fall back to push-to-talk on some boards.
- **Full-duplex I2S**: Only proven on Onju Voice via gnumpi/esphome_audio ADF pipelines. Other boards may need different approaches.
- **ESPHome component API stability**: External component interfaces may change across ESPHome versions.
- **go-mumble-server Lite mode**: Lite is a go-mumble-server extension. For standard Mumble servers (Murmur), Legacy mode is required.
