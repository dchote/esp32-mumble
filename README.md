# ESP32-Mumble

An ESP32-S3 Mumble voice chat client for ESPHome, turning microcontroller boards into always-on intercoms and push-to-talk devices.

> **Status: Voice-functional alpha** — Voice receive and transmit working with three modes: always-on, push-to-talk, and communicator. Legacy (default) and Lite crypto, HA integration. Tested with go-mumble-server. **Box and Box-3** use ESP-IDF with lwIP netconn for UDP. **Generic and Atom Echo** use Arduino. UDP when active; TCP tunnel fallback when UDP unreachable.

## What Is This?

ESP32-Mumble implements the [Mumble](https://www.mumble.info/) voice chat protocol on ESP32-S3 hardware. **Default crypto is Legacy** (OCB2-AES128, standard Mumble); **Lite** (cleartext UDP, minimal CPU) is optional for trusted LAN. Connects to [go-mumble-server](https://github.com/dchote/go-mumble-server) or Murmur.

The firmware runs as an [ESPHome](https://esphome.io/) external component, integrating with Home Assistant for configuration and control.

### Key Capabilities

- **Three operating modes**: Always-on intercom, push-to-talk, and **Communicator** (Star Trek-style half-duplex)
- **Crypto modes**: **Legacy** (default) — OCB2-AES128; **Lite** — cleartext UDP for trusted LAN; **Secure** — AES-256-GCM when server negotiates it (go-mumble-server)
- **Opus** audio encoding/decoding at 16 kHz
- **Microphone capture and transmit** (Opus encode, VAD, echo suppression)
- **Acoustic echo cancellation** for open-mic use
- **Home Assistant** entities for server config, mode, mute, volume, channel, and status
- **Multi-room** intercom via Mumble channels

---

### Communicator Mode

Communicator mode turns any supported board into a **Star Trek-style half-duplex intercom**. A single button press opens a communication session with an audible chime, transmits your voice, and automatically closes when you stop talking.

**How it works:**

1. **Press the button** — an open chime plays, then the microphone goes live
2. **Speak** — audio is transmitted via Mumble with VAD (voice activity detection); silence is not sent
3. **Stop speaking** — after 2 seconds of silence the session auto-closes with a close chime
4. **Press again during a session** — immediately cancels and plays the close chime

While a communicator session is active, incoming voice is suppressed so the bus stays on the microphone. The chime is embedded directly in the firmware as raw PCM and played through the component's own bus-aware speaker path — no media player needed.

Communicator mode is selectable from the **Mode** entity in Home Assistant (alongside Always On and Push to Talk) and persists across reboots. On boards with a physical button (Box, Box-3, Atom Echo), the same button handles all three modes automatically.

---

## Supported Hardware

| Board | Framework | Mic | Speaker | Highlights |
|---|---|---|---|---|
| ESP32-S3 Box | ESP-IDF | ES7210 mic array | ES8311 DAC | lwIP netconn UDP |
| ESP32-S3 Box 3 | ESP-IDF | ES7210 mic array | ES8311 DAC | Same as Box; pin differences |
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
# Clone, set Wi‑Fi in esphome/secrets.yaml; optionally set Mumble server in YAML
esphome compile esphome/esp32-s3-box.yaml   # Box: ESP-IDF; or generic-esp32s3.yaml for Arduino
esphome run esphome/esp32-s3-box.yaml --device /dev/ttyUSB0
```

For Box/Box-3 (ESP-IDF): first flash must be via USB, not OTA.

See [docs/build.md](docs/build.md) for full build and flash instructions.

Configure Mumble server, port, username, password, channel, mode, and crypto from the Home Assistant UI after adding the device; **crypto defaults to Legacy** (OCB2-AES128). Values persist in NVS and are restored on boot. Username defaults to `esp32-<MAC>`; you can overwrite it. Changing server, username, password, channel, or crypto forces a reconnect. **Mode** selects between Always On, Push to Talk, and Communicator — switching modes takes effect immediately (mic is disabled when leaving always-on). Use **Speaker Volume** to adjust playback level. On Box/Box-3, **Speaker Power** controls the hardware amplifier. Diagnostics show connection status, ping, and voice activity. The **Reset Config** button restores all settings to defaults. See `esphome/generic-esp32s3.yaml` for the full pattern.

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
│   ├── generate_communicator_chime.py  # Generates communicator_chime_data.h from WAV
│   └── patch_mbedtls_requires.py       # ESP-IDF Box: adds mbedtls to REQUIRES
├── docs/
│   ├── build.md         # Build and flash instructions
│   ├── features/
│   │   ├── 0001-initial-project-outline.md
│   │   ├── 0002-initial-code-framework.md
│   │   ├── 0003-mumble-connection-protocol.md
│   │   ├── 0004-udp-voice-playback.md
│   │   ├── 0008-voice-capture-optimizations.md
│   │   └── 0009-communicator-mode.md
│   ├── product-overview.md
│   └── technical-overview.md
├── esphome/             # Example device configs
│   ├── esp32-s3-box.yaml
│   ├── esp32-s3-box3.yaml
│   ├── generic-esp32s3.yaml
│   ├── m5stack-atom-echo.yaml
│   └── secrets.example.yaml
├── README.md
└── LICENSE
```

## Contributing

This project is in early development. Contributions, ideas, and hardware testing are welcome. Please open an issue to discuss before submitting large changes.

## License

[MIT](LICENSE)
