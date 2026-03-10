# Build and Flash

## Prerequisites

- [ESPHome](https://esphome.io/) 2024.x or later
- Python 3.9+
- One of the supported ESP32-S3 boards

### Installing ESPHome

```bash
pip install esphome
```

Or use the [ESPHome Dashboard](https://web.esphome.io/) for a browser-based workflow.

## Build

From the project root:

```bash
esphome compile esphome/generic-esp32s3.yaml
```

Or use the build script:

```bash
./scripts/build.sh generic    # generic ESP32-S3
./scripts/build.sh box3       # ESP32-S3 Box 3
./scripts/build.sh all        # all configs
```

Or via Makefile:

```bash
make build                    # default: generic
make CONFIG=box3 build
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

- **Wi‑Fi**: Edit the `wifi` section in the YAML (`ssid` / `password`).
- **Mumble**: Server host, port, username, password, and channel are exposed as text/number entities. Configure them in the Home Assistant UI after the device is added; values persist in NVS. You can also set `initial_value` on each template text/number in the YAML for first-time defaults.

## CI

The [`.github/workflows/build.yml`](../.github/workflows/build.yml) workflow builds both configs on push and pull requests to `main` or `master`. It uses the [esphome/workflows](https://github.com/esphome/workflows) reusable workflow.

## Troubleshooting

- **Component not found**: Ensure `external_components` points to `../components` relative to the YAML file (configs are in `esphome/`).
- **Board not found**: Use `esphome boards` to list available boards. For Box 3 use `esp32s3box3`.
- **Compilation errors**: Ensure you have the correct ESPHome version (`esphome version`).
