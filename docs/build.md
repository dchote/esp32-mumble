# Build and Flash

## Prerequisites

- [ESPHome](https://esphome.io/) **2026.7.0+** (this repo pins **2026.7.1** in `requirements.txt`)
- Python **3.12+**
- One of the supported ESP32-S3 boards

**Before compiling**: Create `esphome/secrets.yaml` from `secrets.example.yaml` with `wifi_ssid`, `wifi_password`, `api_encryption_key`, and `ota_password`. The board configs reference these via `!secret`.

### Installing ESPHome

```bash
pip install -r requirements.txt
```

On macOS you can also use Homebrew (`brew install esphome`), then ensure the CLI is at least 2026.7.0 (`esphome version`).

Or use the [ESPHome Dashboard](https://web.esphome.io/) for a browser-based workflow.

**Migrating from older configs**: If you already have `secrets.yaml` without API encryption or an OTA password, add:

```yaml
api_encryption_key: "..."   # generate: openssl rand -base64 32
ota_password: "..."         # choose a strong password
```

Then reflash once so the device picks up encrypted API and password-protected OTA.

## Build

From the project root:

```bash
esphome compile esphome/generic-esp32s3.yaml
```

Or use the build script:

```bash
./scripts/build.sh generic    # generic ESP32-S3 (Arduino)
./scripts/build.sh box        # ESP32-S3 Box (ESP-IDF, lwIP netconn, requires 2026.7.0+)
./scripts/build.sh box3       # ESP32-S3 Box 3 (ESP-IDF, lwIP netconn)
./scripts/build.sh all        # all configs
```

Or via Makefile:

```bash
make build                    # default: generic
make CONFIG=box build         # ESP32-S3 Box (ESP-IDF)
make CONFIG=box3 build        # ESP32-S3 Box 3 (ESP-IDF)
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

- **Secrets** (required before compile): Add `esphome/secrets.yaml` with Wi‑Fi, `api_encryption_key`, and `ota_password` (see `secrets.example.yaml`). The example configs enable HA API encryption and OTA password via `!secret`.
- **Mumble TLS**: By default the client skips server certificate verification (trusted LAN). Set the Mumble component `ca_cert` option to a PEM CA to enable verification.
- **Mumble**: Server, port, username, password, channel, mode (always-on / push-to-talk), and crypto (default: **Legacy**; or **Lite** for trusted LAN) are exposed as config entities. **Server** defaults to empty and is auto-detected from the Home Assistant server IP when the device is adopted by HA (e.g. for go-mumble-server addon); set it manually in YAML or in HA to override. Username defaults to `esp32-<MAC>`. Channel defaults to **Root**. Changing server, username, password, channel, or crypto forces a reconnect. **Speaker Volume** controls output and persists across reboots; microphone is controlled via physical button wiring. On Box/Box-3, **Speaker Power** toggles the hardware amplifier (GPIO46). Diagnostics include WiFi signal, Mumble connected, ping, and **Reset Config**. **Voice Received** appears under Sensors. All values persist in NVS and are restored on boot.

## Dependencies

- **Opus**: The Mumble component uses a **local vendored Opus library** (`lib/micro-opus/`) based on [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus) v0.4.1. It uses heap-based pseudostack allocation (PSRAM when available) and Xtensa DSP optimizations. Do not remove or modify `lib/micro-opus/` — it is required for compilation.
- **ESP32-S3 Box / Box-3 (ESP-IDF)**: Both use `framework: type: esp-idf` with lwIP netconn for UDP. First flash must be via USB, not OTA.
- **Toolchain**: All board configs set `esp32.toolchain: platformio`. ESPHome 2026.7 defaults to the native ESP-IDF toolchain, which does not support this project's local `symlink://` micro-opus library or the mbedtls PlatformIO pre-script.

## Troubleshooting

- **Component not found**: Ensure `external_components` points to `../components` relative to the YAML file (configs are in `esphome/`).
- **Board not found**: Use `esphome boards` to list available boards. For Box 3 use `esp32s3box3`; for original Box use `esp32s3box`.
- **Compilation errors**: Ensure you have the correct ESPHome version (`esphome version` should be 2026.7.0+; `pip install -r requirements.txt` pins 2026.7.1). If Opus-related errors occur, verify `lib/micro-opus/` exists and is complete (no missing files).
- **UDP ping timeout**: Box and Box-3 (ESP-IDF netconn) support UDP. If ping stays "nan" and voice uses TCP tunnel, check firewall/NAT.
- **"UDP ping echo not received"**: On same-LAN setups this was historically caused by an OCB2 encryption mismatch; the implementation now matches grumble. If it persists, check NAT/firewall rules or try Lite crypto mode (cleartext UDP) to isolate networking vs. encryption.
