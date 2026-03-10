# ESP32-Mumble

An ESP32-S3 Mumble voice chat client for ESPHome, turning microcontroller boards into always-on intercoms and push-to-talk devices.

> **Status: Early Development / Pre-Alpha** -- No buildable firmware yet. Project documentation and architecture are being established.

## What Is This?

ESP32-Mumble implements the [Mumble](https://www.mumble.info/) voice chat protocol on ESP32-S3 hardware. It supports **Lite mode** (cleartext UDP, minimal CPU) and optional **Legacy mode** (standard Mumble with OCB2-AES128) for connectivity to any Mumble server. Connects to [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur.

The firmware runs as an [ESPHome](https://esphome.io/) external component, integrating with Home Assistant for configuration and control.

### Key Capabilities

- **Lite** (default) and optional **Legacy** (standard Mumble) crypto modes
- **Always-on intercom** or **push-to-talk** operation
- **Opus** audio encoding/decoding at 16 kHz
- **Full-duplex** simultaneous capture and playback
- **Acoustic echo cancellation** for open-mic use
- **Home Assistant** entities for server config, mute, volume, channel, and status
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
- [ESPHome](https://esphome.io/) 2024.x or later
- [Home Assistant](https://www.home-assistant.io/) (optional, for configuration and control UI)
- One of the supported ESP32-S3 boards listed above

## Quick Start

> Build and flash instructions will be added once the firmware is functional.

The ESPHome configuration will look like:

```yaml
external_components:
  - source:
      type: local
      path: components

mumble:
  server: "192.168.1.100"
  port: 64738
  username: "kitchen-intercom"
  password: "secret"
  channel: "Intercom"
  mode: always_on
  mute_pin: GPIO38
```

Server connection, username, password, and channel can also be changed at runtime from the Home Assistant UI without reflashing.

## Project Structure

```
esp32-mumble/
├── components/          # ESPHome external components (planned)
│   └── mumble/          # Mumble client component
├── docs/
│   ├── features/
│   │   └── 0001-initial-project-outline.md
│   ├── product-overview.md
│   └── technical-overview.md
├── research/            # Reference implementations (git-ignored)
│   ├── go-mumble-server/
│   ├── wake-word-voice-assistants/
│   └── onju-voice-satellite/
├── README.md
└── LICENSE
```

## Contributing

This project is in early development. Contributions, ideas, and hardware testing are welcome. Please open an issue to discuss before submitting large changes.

## License

[MIT](LICENSE)
