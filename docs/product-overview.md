# Product Overview

## Vision

ESP32-Mumble is an always-on intercom and push-to-talk voice client that runs on ESP32-S3 microcontrollers. It implements the Mumble voice chat protocol, connects to [go-mumble-server](https://github.com/dchote/go-mumble-server), and integrates with Home Assistant through ESPHome.

The project targets LAN deployments. **Legacy mode** (OCB2-AES128 UDP encryption, standard Mumble protocol) is the default and works with any Mumble server (Murmur, go-mumble-server). **Lite mode** (cleartext UDP) is available for trusted LAN use when the server supports it.

## Use Cases

### Always-On Intercom

The device listens and transmits continuously, acting as a room intercom. Audio from the microphone streams to the Mumble channel in real time. A physical mute button (or HA-controlled software mute) silences the microphone without disconnecting. This is the default operating mode.

### Microphone Control

Transmitting is controlled by a physical button wired to `mumble.microphone_enable` / `mumble.microphone_disable` (press = on, release = off). In push-to-talk mode this provides hold-to-talk; in always-on mode it toggles. The `mumble.ptt_press` action toggles mic state for single-press buttons.

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

- Opus decoding of received voice streams to speaker output — **implemented**
- Opus encoding of microphone input (16 kHz, mono, ~16 kbps) — **implemented**
- Half-duplex capture and playback (shared I2S bus; mic and speaker take turns)
- Echo suppression: transmit is suppressed while receiving or within 100 ms after
- Microphone array support with beamforming on boards with multi-channel ADCs (ES7210)

### Mumble Protocol

- **Legacy mode** (default): Standard Mumble protocol with OCB2-AES128 UDP encryption. Connects to Murmur, go-mumble-server, or any Mumble server.
- **Lite mode** (optional): Cleartext UDP voice, TLS control. Minimal CPU; use on trusted LAN when the server supports it (e.g. go-mumble-server with Lite enabled).
- Join a configured channel on connect
- Receive and transmit Opus voice packets (UDP when active; TCP tunnel fallback when UDP unreachable)
- Channel changes via `UserState` (e.g. from HA channel select)
- Respond to server pings to maintain connection
- UDP voice with TCP fallback (UDPTunnel) if UDP is unreachable

### Controls and Indicators

- Microphone on/off via physical button (`mumble.microphone_enable` / `mumble.microphone_disable`)
- Push-to-talk via physical button (press-and-hold)
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
| Crypto | Select | **Legacy** (OCB2-AES128, default) or **Lite** (cleartext UDP for trusted LAN) |
| Speaker Volume | Number | Playback volume level (0–100, default 80) |
| Speaker Power | Switch | Hardware amplifier power on/off (Box/Box-3 only, GPIO46) |

All settings persist in NVS and are restored on boot — including speaker volume. Changing server, port, username, password, channel, or crypto forces an immediate reconnect.

### Home Assistant Diagnostics

| Entity | Type | Description |
|---|---|---|
| WiFi Signal | Sensor | WiFi RSSI in dBm |
| Mumble Connected | Binary Sensor | Whether connected to the Mumble server |
| Mumble Ping | Sensor | Round-trip ping time to server in ms |
| Voice Received | Binary Sensor | True while voice is being received and processed (Sensors) |
| Reset Config | Button | Restore server, port, username, password, channel, mode, and crypto to defaults |

### Home Assistant Runtime Entities

| Entity | Type | Status | Description |
|---|---|---|---|
| Voice Sending | Binary Sensor | Implemented | True while the device is transmitting audio |
| Speaker Volume | Number | Implemented | Speaker volume level 0–100 (persists across reboots) |
| Speaker Power | Switch | Implemented | Hardware amplifier on/off, Box/Box-3 only (persists) |
| Mumble Connected | Binary Sensor | Implemented | Connection status (Diagnostics) |
| Voice Received | Binary Sensor | Implemented | True while receiving voice (Sensors) |
| Channel | Text | Implemented | Channel name to join (config) |
| Channel Select | Select | Optional | Server-populated channel list (`channel_select_id`) |

## Server Requirements

The client uses **Legacy** crypto by default; **Lite** is optional for trusted LAN.

**Legacy mode** (default):

- Any Mumble server (Murmur, go-mumble-server, etc.)
- Standard Mumble protocol with OCB2-AES128
- Opus codec enabled

**Lite mode** (optional):

- **go-mumble-server** or compatible with Lite crypto mode enabled
- Trusted LAN deployment
- TLS certificate (self-signed acceptable)

## Non-Goals (v1)

The following are explicitly out of scope for the initial version:

- Text messaging
- ACL or permission management
- Server administration from the device
- Client certificate / client-initiated Secure tier (server can negotiate Secure; client accepts it)
- CELT or Speex codec support
- Positional audio
- Multiple simultaneous server connections
- Audio recording or playback of stored media
