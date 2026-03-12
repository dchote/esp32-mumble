# Product Overview

## Vision

ESP32-Mumble is an always-on intercom and push-to-talk voice client that runs on ESP32-S3 microcontrollers. It implements the Mumble voice chat protocol, connects to [go-mumble-server](https://github.com/dchote/go-mumble-server), and integrates with Home Assistant through ESPHome.

The project targets LAN deployments, with Lite crypto mode (cleartext UDP voice) as the default for constrained hardware. Optional support for standard Mumble protocol (Legacy mode, OCB2-AES128) allows connectivity to any Mumble server (Murmur, go-mumble-server, etc.), though ESP32-S3 may lack CPU headroom for Legacy — support is conditional on benchmarks.

## Use Cases

### Always-On Intercom

The device listens and transmits continuously, acting as a room intercom. Audio from the microphone streams to the Mumble channel in real time. A physical mute button (or HA-controlled software mute) silences the microphone without disconnecting. This is the default operating mode.

### Microphone Control

The **Microphone Enabled** switch in Home Assistant explicitly turns transmitting on or off. In push-to-talk mode, a physical PTT button (`ptt_pin`) can be wired for hold-to-talk: press and hold to talk, release to stop. PTT is distinct from the Microphone Enabled switch — the switch is a persistent on/off toggle; PTT is momentary.

### Multi-Room Intercom

Multiple ESP32-Mumble devices join the same Mumble server, each in an assigned channel or all in one shared channel. Rooms can be grouped or isolated by channel, with channel assignment configurable from Home Assistant.

### Home Assistant Integration

Each device appears in Home Assistant as an ESPHome node with configurable entities for server connection, operating mode (always-on / push-to-talk), mute state, volume, active channel, and talking status. Automations can mute devices on a schedule, switch channels in response to events, or trigger notifications when someone begins talking.

## Target Hardware

| Board | Microphone | Speaker/DAC | Notes |
|---|---|---|---|
| **ESP32-S3 Box 3** | ES7210 4-ch mic array | ES8311 DAC | Mic array enables beamforming; I2C codec control |
| **ESP32-S3 Box** | ES7210 4-ch mic array | ES8311 DAC | Older revision of Box 3 |
| **Onju Voice** | SPH0645 I2S mic | MAX98357A I2S amp | Drop-in Google Nest Mini replacement; touch, LEDs, mute switch |
| **M5Stack Atom Echo** | PDM microphone | External I2S DAC | Compact form factor |
| **Generic ESP32-S3** | Any I2S microphone | Any I2S amplifier/DAC | User-defined pin configuration |

All targets require an ESP32-S3 with PSRAM and Wi-Fi. The S3's dual-core architecture is needed for simultaneous audio encode/decode and network I/O.

## Features

### Audio

- Opus encoding of microphone input (16 kHz, mono, ~16 kbps)
- Opus decoding of received voice streams to speaker output
- Full-duplex operation (simultaneous capture and playback)
- Acoustic echo cancellation (AEC) to prevent speaker output from feeding back into the microphone
- Microphone array support with beamforming on boards with multi-channel ADCs (ES7210)

### Mumble Protocol

- **Lite mode** (default): Connect with cleartext UDP voice, TLS control. Minimal CPU; go-mumble-server or compatible.
- **Legacy mode** (optional): Standard Mumble protocol with OCB2-AES128 UDP encryption. Connects to Murmur, go-mumble-server, or any Mumble server. May exceed ESP32-S3 CPU; enable only if benchmarks allow.
- Join a configured channel on connect
- Transmit and receive Opus voice packets
- Channel changes via `UserState` (e.g. from HA channel select)
- Respond to server pings to maintain connection
- UDP voice with TCP fallback (UDPTunnel) if UDP is unreachable

### Controls and Indicators

- **Microphone Enabled** switch in Home Assistant (explicit on/off)
- Mute/unmute via hardware switch or button
- Push-to-talk via physical button (`ptt_pin`; press-and-hold)
- Volume control via touch surface, button, or Home Assistant
- LED status indication:
  - Connecting (slow pulse)
  - Connected/idle (steady)
  - Talking (active animation)
  - Muted (distinct color)
  - Error (red)

### Home Assistant Configuration

The following settings are exposed as Home Assistant entities and can be changed from the HA UI without reflashing the device. Values persist in NVS across reboots.

| Entity | Type | Description |
|---|---|---|
| Server | Text | Hostname or IP of the Mumble server |
| Port | Text | Server port (default 64738, displays as integer) |
| Username | Text | Mumble username; defaults to `esp32-<MAC>` on first run (user can overwrite) |
| Password | Text | Server or user password; no default |
| Channel | Text | Channel to join on connect (empty = root; case-insensitive match) |
| Mode | Select | **Always on** or **Push to talk** (persisted across reboots) |
| Microphone Enabled | Switch | Explicitly enable or disable transmitting (Controls section) |

Changing server, port, username, password, or channel forces an immediate reconnect to the Mumble server.

### Home Assistant Diagnostics

| Entity | Type | Description |
|---|---|---|
| WiFi Signal | Sensor | WiFi RSSI in dBm |
| Mumble Connected | Binary Sensor | Whether connected to the Mumble server |
| Mumble Ping | Sensor | Round-trip ping time to server in ms |
| Reset Config | Button | Restore server, port, username, password, channel, mode, and mic to defaults |

### Home Assistant Runtime Entities

The following runtime entities are planned (not yet implemented):

| Entity | Type | Description |
|---|---|---|
| Mute | Switch | Microphone mute state |
| Volume | Number | Speaker volume level |
| Channel | Select | Active channel (options populated from server after connect) |
| Talking | Binary Sensor | Whether the device is currently transmitting audio |
| Connected | Binary Sensor | Whether the device is connected to the server |

## Server Requirements

**Lite mode** (recommended):

- **go-mumble-server** or compatible with Lite crypto mode enabled
- Trusted LAN deployment
- TLS certificate (self-signed acceptable)
- Opus codec enabled

**Legacy mode** (optional, standard Mumble):

- Any Mumble server (Murmur, go-mumble-server, etc.)
- Standard Mumble protocol

## Non-Goals (v1)

The following are explicitly out of scope for the initial version:

- Text messaging
- ACL or permission management
- Server administration from the device
- Secure (AES-256-GCM) crypto mode
- CELT or Speex codec support
- Positional audio
- Multiple simultaneous server connections
- Audio recording or playback of stored media
