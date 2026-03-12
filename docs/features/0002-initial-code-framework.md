# 0002 -- Initial Code Framework and Build Infrastructure

## Summary

Establish the initial code framework for ESP32-Mumble: ESPHome external component scaffold with protocol stubs, build scripts, and CI pipelines.

## Motivation

The project had documentation (0001) but no source code. A skeletal but complete codebase is needed to validate the build toolchain, ESPHome integration, and protocol scaffolding before implementing TLS, Opus, and audio hardware.

## Changes

### 1. ESPHome External Component (`components/mumble/`)

- `__init__.py` — Component registration, YAML schema (server, port, username, password, channel, mode; optional entity IDs: server_text_id, port_text_id, username_text_id, password_text_id, channel_text_id, mode_select_id, microphone_switch_id; ptt_pin, mute_pin, crypto)
- `mumble_component.h` / `.cpp` — Main component; setup, loop, dump_config
- `mumble_protocol.h` / `.cpp` — TCP framing (6-byte header), message type constants
- `mumble_varint.h` / `.cpp` — Mumble custom varint (UDP voice); port of go-mumble-server varint.go
- `mumble_client.h` / `.cpp` — MumbleClient stub (connect/disconnect stubs)
- `mumble_audio.h` / `.cpp` — MicrophoneSource/SpeakerSink interface stubs

### 2. Example Configs (`esphome/`)

- `generic-esp32s3.yaml` — Minimal ESP32-S3, Wi‑Fi, mumble component
- `esp32-s3-box3.yaml` — ESP32-S3 Box 3 board config
- `secrets.example.yaml` — Template for API encryption key

### 3. Build Scripts

- `scripts/build.sh` — `esphome compile` for generic, box3, or all
- `scripts/flash.sh` — `esphome run` with device selection
- `Makefile` — `make build`, `make flash`

### 4. CI

- `.github/workflows/build.yml` — Builds both configs when run manually (workflow_dispatch) via esphome/workflows

### 5. Documentation

- `docs/build.md` — Prerequisites, build/flash, CI, troubleshooting
- `README.md` — Build section, status update
- `docs/technical-overview.md` — Protocol Reference section

## References

- Plan: Initial Code Framework and Build Infrastructure
- [go-mumble-server protocol docs](research/go-mumble-server/docs/protocol/)
- [sh123/esp32_opus_arduino](https://github.com/sh123/esp32_opus_arduino) (libopus dependency for future audio work)
