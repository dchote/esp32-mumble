# ESP32-Mumble

An ESP32-S3 Mumble voice chat client for ESPHome, turning microcontroller boards into always-on intercoms and push-to-talk devices.

> **Status: Alpha** -- Voice receive and transmit implemented. Legacy (default) and Lite crypto and HA integration working. Tested with go-mumble-server. **Voice transmit is not 100%**: UDP path works for pings; server may not yet have a valid UDP route for the device (`SendAudio no UDP path`); voice may fall back to TCP tunnel. **All board configs use Arduino framework** (ESP-IDF/lwIP unicast UDP has known issues; ESP-IDF support will be restored after Arduino validation).

## What Is This?

ESP32-Mumble implements the [Mumble](https://www.mumble.info/) voice chat protocol on ESP32-S3 hardware. **Default crypto is Legacy** (OCB2-AES128, standard Mumble); **Lite** (cleartext UDP, minimal CPU) is optional for trusted LAN. Connects to [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur.

The firmware runs as an [ESPHome](https://esphome.io/) external component, integrating with Home Assistant for configuration and control.

### Key Capabilities

- **Crypto modes**: **Legacy** (default) — OCB2-AES128; **Lite** — cleartext UDP for trusted LAN; **Secure** — AES-256-GCM when server negotiates it (go-mumble-server)
- **Always-on intercom** or **push-to-talk** operation
- **Opus** audio encoding/decoding at 16 kHz
- **Microphone capture and transmit** (Opus encode, VAD, echo suppression)
- **Acoustic echo cancellation** for open-mic use
- **Home Assistant** entities for server config, mode (always-on / PTT), mute, volume, channel, and status
- **Multi-room** intercom via Mumble channels

## Supported Hardware

| Board | Framework | Mic | Speaker | Highlights |
|---|---|---|---|---|
| ESP32-S3 Box | Arduino | ES7210 mic array | ES8311 DAC | UDP voice working |
| ESP32-S3 Box 3 | Arduino | ES7210 mic array | ES8311 DAC | Same as Box; pin differences |
| Onju Voice | Arduino | SPH0645 I2S | MAX98357A I2S | Nest Mini form factor, touch, LEDs |
| M5Stack Atom Echo | Arduino | PDM mic | External DAC | Compact |
| Generic ESP32-S3 | Arduino | Any I2S mic | Any I2S DAC/amp | User-defined pins |

All boards require an ESP32-S3 with PSRAM and Wi-Fi.

## Architecture

```
ESP32-S3                              go-mumble-server
┌────────────────────┐               ┌──────────────────┐
│ ESPHome Component  │──TCP/TLS────►│  Control (64738)  │
│                    │               │                   │
│ Mic/Opus encode    │               │  Voice  (64738)   │
│ Opus Dec ► I2S Spk │◄──UDP voice (rx/tx)────│
│                    │               └──────────────────┘
│ HA Entities ◄─────►│ Home Assistant
└────────────────────┘
```

See the full documentation:

- **[Product Overview](docs/product-overview.md)** -- Vision, use cases, features, and target hardware
- **[Technical Overview](docs/technical-overview.md)** -- Protocol details, audio pipeline, ESPHome integration, and hardware abstraction

## Prerequisites

- A Mumble server (e.g. [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur) on your LAN
- [ESPHome](https://esphome.io/) 2024.x or later (2025.5.0+ for Box/Box-3)
- [Home Assistant](https://www.home-assistant.io/) (optional, for configuration and control UI)
- One of the supported ESP32-S3 boards listed above

## Quick Start

```bash
# Clone, then set Wi‑Fi in esphome/secrets.yaml; optionally set initial_value for Mumble server
esphome compile esphome/generic-esp32s3.yaml
esphome run esphome/generic-esp32s3.yaml --device /dev/ttyUSB0
```

See [docs/build.md](docs/build.md) for full build and flash instructions.

Configure Mumble server, port, username, password, channel, mode, and crypto from the Home Assistant UI after adding the device; **crypto defaults to Legacy** (OCB2-AES128). Values persist in NVS and are restored on boot. Username defaults to `esp32-<MAC>`; you can overwrite it. Changing server, username, password, channel, or crypto forces a reconnect. Use **Speaker Volume** to adjust playback level and **Microphone Enabled** to enable or disable transmitting — both persist across reboots. On Box/Box-3, **Speaker Power** controls the hardware amplifier. Diagnostics show connection status, ping. **Voice Received** (Sensors) indicates when voice is being received. The **Reset Config** button restores all settings to defaults. See `esphome/generic-esp32s3.yaml` for the full pattern.

## Project Structure

```
esp32-mumble/
├── components/          # ESPHome external components
│   └── mumble/          # Mumble client component
├── lib/
│   └── micro-opus/      # Local Opus codec (heap pseudostack, PSRAM, Xtensa; no submodules)
├── scripts/
│   ├── build.sh
│   ├── flash.sh
│   └── patch_mbedtls_requires.py  # ESP-IDF Box: adds mbedtls to REQUIRES
├── docs/
│   ├── build.md         # Build and flash instructions
│   ├── features/
│   │   ├── 0001-initial-project-outline.md
│   │   ├── 0002-initial-code-framework.md
│   │   ├── 0003-mumble-connection-protocol.md
│   │   ├── 0004-udp-voice-playback.md
│   │   ├── 0005-voice-capture-encoding-transmission.md
│   │   └── 0006-esp-idf-migration.md
│   ├── product-overview.md
│   └── technical-overview.md
├── esphome/             # Example device configs
│   ├── esp32-s3-box.yaml
│   ├── esp32-s3-box3.yaml
│   ├── generic-esp32s3.yaml
│   ├── m5stack-atom-echo.yaml
│   └── secrets.example.yaml
├── .github/workflows/   # CI (build.yml)
├── README.md
└── LICENSE
```

## Contributing

This project is in early development. Contributions, ideas, and hardware testing are welcome. Please open an issue to discuss before submitting large changes.

## License

[MIT](LICENSE)
