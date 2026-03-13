# Build and Flash

## Prerequisites

- [ESPHome](https://esphome.io/) 2024.x or later (2025.5.0+ for Box/Box-3)
- Python 3.9+
- One of the supported ESP32-S3 boards

**Before compiling**: You must specify Wi‑Fi credentials. Create `esphome/secrets.yaml` (or add to your config) with `wifi_ssid` and `wifi_password`; the configs reference these via `!secret`. See `secrets.example.yaml` for the expected format.

### Installing ESPHome

```bash
pip install esphome
```

On macOS you can also use Homebrew:

```bash
brew install esphome
```

Or use the [ESPHome Dashboard](https://web.esphome.io/) for a browser-based workflow.

## Build

From the project root:

```bash
esphome compile esphome/generic-esp32s3.yaml
```

Or use the build script:

```bash
./scripts/build.sh generic    # generic ESP32-S3 (Arduino)
./scripts/build.sh box        # ESP32-S3 Box (Arduino, requires 2025.5.0+)
./scripts/build.sh box3       # ESP32-S3 Box 3 (Arduino)
./scripts/build.sh all        # all configs
```

Or via Makefile:

```bash
make build                    # default: generic
make CONFIG=box build         # ESP32-S3 Box (Arduino)
make CONFIG=box3 build        # ESP32-S3 Box 3 (Arduino)
make all
```

## Flash

Connect the device via USB, then:

```bash
esphome run esphome/generic-esp32s3.yaml --device /dev/ttyUSB0
```

Or:

```bash
./scripts/flash.sh generic --device /dev/ttyUSB0
make flash PORT=/dev/ttyUSB0
```

On macOS the port is typically `/dev/cu.usbmodem*`.

## Configuration

- **Wi‑Fi** (required before compile): Add `esphome/secrets.yaml` with `wifi_ssid` and `wifi_password`, or edit the `wifi` section in the YAML directly. The example configs use `!secret` references.
- **Mumble**: Server, port, username, password, channel, mode (always-on / push-to-talk), and crypto (default: **Legacy**; or **Lite** for trusted LAN) are exposed as config entities. Username defaults to `esp32-<MAC>`. Changing server, username, password, channel, or crypto forces a reconnect. **Speaker Volume** controls output and persists across reboots; microphone is controlled via physical button wiring. On Box/Box-3, **Speaker Power** toggles the hardware amplifier (GPIO46). Diagnostics include WiFi signal, Mumble connected, ping, and **Reset Config**. **Voice Received** appears under Sensors. All values persist in NVS and are restored on boot.

## CI

The [`.github/workflows/build.yml`](../.github/workflows/build.yml) workflow builds both configs when run manually (Actions → Build → Run workflow). It uses the [esphome/workflows](https://github.com/esphome/workflows) reusable workflow.

## Dependencies

- **Opus**: The Mumble component uses a **local vendored Opus library** (`lib/micro-opus/`) based on [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus). It uses heap-based pseudostack allocation (PSRAM when available) and Xtensa DSP optimizations. Do not remove or modify `lib/micro-opus/` — it is required for compilation.
- **ESP32-S3 Box / Box-3 (Arduino)**: Both Box configs use `framework: type: arduino`. UDP voice works on Arduino; ESP-IDF has known unicast UDP issues and is not used for now.

## Troubleshooting

- **Component not found**: Ensure `external_components` points to `../components` relative to the YAML file (configs are in `esphome/`).
- **Board not found**: Use `esphome boards` to list available boards. For Box 3 use `esp32s3box3`; for original Box use `esp32s3box`.
- **Compilation errors**: Ensure you have the correct ESPHome version (`esphome version`). For ESP32-S3 Box, use ESPHome 2025.5.0 or later. If Opus-related errors occur, verify `lib/micro-opus/` exists and is complete (no missing files).
- **UDP ping timeout**: Arduino framework: UDP works on same-LAN. If ping stays "nan" and voice uses TCP tunnel, check firewall/NAT. ESP-IDF: unicast UDP does not work (known issue); use Arduino.
- **"UDP ping echo not received"**: On same-LAN setups this was historically caused by an OCB2 encryption mismatch; the implementation now matches grumble. If it persists, check NAT/firewall rules or try Lite crypto mode (cleartext UDP) to isolate networking vs. encryption.
