# ESP32-Mumble

An ESP32-S3 Mumble voice chat client for ESPHome, turning microcontroller boards into always-on intercoms and push-to-talk devices.

> **Status: Early Development / Pre-Alpha** -- Initial framework builds; protocol and audio integration in progress.

## What Is This?

ESP32-Mumble implements the [Mumble](https://www.mumble.info/) voice chat protocol on ESP32-S3 hardware. It supports **Lite mode** (cleartext UDP, minimal CPU) and optional **Legacy mode** (standard Mumble with OCB2-AES128) for connectivity to any Mumble server. Connects to [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur.

The firmware runs as an [ESPHome](https://esphome.io/) external component, integrating with Home Assistant for configuration and control.

### Key Capabilities

- **Lite** (default) and optional **Legacy** (standard Mumble) crypto modes
- **Always-on intercom** or **push-to-talk** operation
- **Opus** audio encoding/decoding at 16 kHz
- **Full-duplex** simultaneous capture and playback
- **Acoustic echo cancellation** for open-mic use
- **Home Assistant** entities for server config, mode (always-on / PTT), mute, volume, channel, and status
- **Multi-room** intercom via Mumble channels

## Supported Hardware

| Board | Mic | Speaker | Highlights |
|---|---|---|---|
| ESP32-S3 Box 3 | ES7210 mic array | ES8311 DAC | 4-mic beamforming |
| ESP32-S3 Box | ES7210 mic array | ES8311 DAC | Older Box revision |
| Onju Voice | SPH0645 I2S | MAX98357A I2S | Nest Mini form factor, touch, LEDs |
| M5Stack Atom Echo | PDM mic | External DAC | Compact |
| Generic ESP32-S3 | Any I2S mic | Any I2S DAC/amp | User-defined pins |

All boards require an ESP32-S3 with PSRAM and Wi-Fi.

## Architecture

```
ESP32-S3                              go-mumble-server
┌────────────────────┐               ┌──────────────────┐
│ ESPHome Component  │──TCP/TLS────►│  Control (64738)  │
│                    │               │                   │
│ I2S Mic ► Opus Enc │──UDP (Lite/Legacy)──►│  Voice  (64738)   │
│ Opus Dec ► I2S Spk │◄──UDP (Lite/Legacy)──│                   │
│                    │               └──────────────────┘
│ HA Entities ◄─────►│ Home Assistant
└────────────────────┘
```

See the full documentation:

- **[Product Overview](docs/product-overview.md)** -- Vision, use cases, features, and target hardware
- **[Technical Overview](docs/technical-overview.md)** -- Protocol details, audio pipeline, ESPHome integration, and hardware abstraction

## Prerequisites

- A Mumble server (e.g. [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur) on your LAN
- [ESPHome](https://esphome.io/) 2024.x or later — on macOS: `brew install esphome`
- [Home Assistant](https://www.home-assistant.io/) (optional, for configuration and control UI)
- One of the supported ESP32-S3 boards listed above

## Quick Start

```bash
# Clone, then set Wi‑Fi in esphome/generic-esp32s3.yaml; optionally set initial_value for Mumble server/username
esphome compile esphome/generic-esp32s3.yaml
esphome run esphome/generic-esp32s3.yaml --device /dev/ttyUSB0
```

See [docs/build.md](docs/build.md) for full build and flash instructions.

Configure Mumble server, port, username, password, channel, and mode (always-on / push-to-talk) from the Home Assistant UI after adding the device; values persist in NVS. Use the **Microphone Enabled** switch in HA to enable or disable transmitting. Example config uses text and select entities (`server_text_id`, `port_text_id`, `mode_select_id`, etc.) plus `microphone_switch_id`; see `esphome/generic-esp32s3.yaml` for the full pattern.

## Project Structure

```
esp32-mumble/
├── components/          # ESPHome external components
│   └── mumble/          # Mumble client component
├── docs/
│   ├── build.md         # Build and flash instructions
│   ├── features/
│   │   ├── 0001-initial-project-outline.md
│   │   └── 0002-initial-code-framework.md
│   ├── product-overview.md
│   └── technical-overview.md
├── esphome/             # Example device configs
│   ├── generic-esp32s3.yaml
│   ├── esp32-s3-box3.yaml
│   └── secrets.example.yaml
├── scripts/             # Build and flash scripts
├── .github/workflows/   # CI (build.yml)
├── README.md
└── LICENSE
```

## Contributing

This project is in early development. Contributions, ideas, and hardware testing are welcome. Please open an issue to discuss before submitting large changes.

## License

[MIT](LICENSE)
