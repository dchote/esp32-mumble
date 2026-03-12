# Technical Overview

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Trusted LAN                                  │
│                                                                     │
│  ┌──────────────────┐         ┌──────────────────────────────────┐  │
│  │  go-mumble-server│         │  ESP32-S3 Device                 │  │
│  │                  │◄───────►│                                  │  │
│  │  TCP/TLS :64738  │ Control │  ┌────────────┐  ┌───────────┐  │  │
│  │  (Version, Auth, │ Channel │  │ MumbleClient│  │ AudioPipe │  │  │
│  │   CryptSetup,    │         │  │ (TCP/TLS)  │  │ (I2S+Opus)│  │  │
│  │   Ping, Sync)    │         │  └─────┬──────┘  └─────┬─────┘  │  │
│  │                  │         │        │               │         │  │
│  │  UDP :64738      │◄───────►│  ┌─────┴───────────────┴─────┐  │  │
│  │  (Opus voice,    │  Voice  │  │  UDP Voice (Lite or Legacy) │  │  │
│  │   Lite/Legacy)   │  Packets│  │  Cleartext or OCB2-AES128   │  │  │
│  │                  │         │  └────────────────────────────┘  │  │
│  └──────────────────┘         │                                  │  │
│                               │  ┌─────────┐  ┌──────────────┐  │  │
│  ┌──────────────────┐         │  │ ESPHome │  │ Home         │  │  │
│  │  Home Assistant  │◄───────►│  │ API     │  │ Assistant    │  │  │
│  │                  │  Native │  │         │  │ Entities     │  │  │
│  │  Dashboard       │  API    │  └─────────┘  └──────────────┘  │  │
│  └──────────────────┘         └──────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

The ESP32-S3 device maintains two network connections:

1. **Mumble server** — TCP/TLS for control messages and UDP for voice packets (Lite: cleartext; Legacy: OCB2-AES128), both on port 64738.
2. **Home Assistant** -- ESPHome native API for entity state, configuration, and OTA updates.

## Mumble Protocol

### Connection Lifecycle

The server sends Version immediately after TLS handshake; the client responds with Version and Authenticate. CryptSetup is sent by the server *after* authentication as part of sync:

```
ESP32                                         go-mumble-server
  │                                                 │
  │──── TCP connect + TLS handshake ───────────────►│
  │                                                 │
  │◄─── Version (CryptoModes=0x07) ─────────────────│  Server sends first
  │──── Version (CryptoModes=0x01 or 0x02) ────────►│  Lite only or Legacy only (per user selection)
  │──── Authenticate (user, pass, opus=true) ──────►│
  │                                                 │
  │◄─── CryptSetup (key=empty or 16 bytes) ─────────│  After auth, signals mode
  │◄─── ChannelState[] ────────────────────────────│
  │◄─── UserState[] ──────────────────────────────│
  │◄─── ServerConfig ─────────────────────────────│
  │◄─── CodecVersion (opus=true) ──────────────────│
  │◄─── ServerSync (session, welcome) ─────────────│
  │                                                 │
  │──── UDP Ping ──────────────────────────────────►│
  │◄─── UDP Ping echo ────────────────────────────│
  │                                                 │
  │◄──► UDP Voice (Opus, Lite or Legacy) ◄────────►│
  │◄──► TCP Ping (keepalive) ◄────────────────────►│
```

**Channel selection** — The client joins the channel specified in the Channel text entity. Changing the channel in HA forces a reconnect; the new channel name is matched case-insensitively (e.g. `root` matches `Root`).

### Crypto Modes

go-mumble-server negotiates security per client during the Version exchange. The ESP32 client advertises supported modes; the server selects the best mutual tier.

| Mode | CryptoModes bit | UDP treatment | Use case |
|------|-----------------|---------------|----------|
| **Legacy** | 0x02 | OCB2-AES128, 4-byte overhead | Standard Mumble (Murmur, go-mumble-server); **default** |
| **Lite** | 0x01 | Cleartext, no per-packet crypto | Trusted LAN, minimal CPU; **optional** |

**Lite mode** — Client sends `CryptoModes = 0x01`. Server responds with CryptSetup (empty key). UDP voice is cleartext; TCP control remains TLS-encrypted. Suitable for trusted LAN deployments.

**Legacy mode (standard Mumble)** — Client sends `CryptoModes = 0x02` only. Server responds with CryptSetup (16-byte key). UDP voice is encrypted with OCB2-AES128. Compatible with any standard Mumble server. Legacy is the default. Changing crypto in HA (Legacy ↔ Lite) forces a reconnect so the server negotiates the new mode.

**OCB2 implementation** — The Legacy OCB2-AES128 codec (`mumble_ocb2.cpp`) is ported from [mumble-voip/grumble](https://github.com/mumble-voip/grumble) and must match its byte layout exactly for interoperability with go-mumble-server. Key requirements: GF(2^128) doubling uses block\[0\] as MSB (big-endian); partial-block length is encoded at bytes 14–15; polynomial 0x87 for reduction.

### TCP Wire Format

Every TCP message uses a 6-byte header:

```
┌──────────────┬────────────────────┬──────────────────┐
│ Type (u16be) │ Length (u32be)      │ Payload (bytes)  │
│ 2 bytes      │ 4 bytes            │ 0..8MB           │
└──────────────┴────────────────────┴──────────────────┘
```

Message types relevant to this client:

| Type | Name | Direction | Purpose |
|------|------|-----------|---------|
| 0 | Version | Both | Protocol version, OS info, CryptoModes |
| 1 | UDPTunnel | Both | Voice data tunneled over TCP (fallback) |
| 2 | Authenticate | C→S | Username, password, codec support |
| 3 | Ping | Both | Keepalive and latency measurement |
| 5 | ServerSync | S→C | Session ID, welcome text, permissions |
| 7 | ChannelState | S→C | Channel tree |
| 9 | UserState | Both | User presence, channel membership |
| 15 | CryptSetup | S→C | Encryption key and nonces |
| 21 | CodecVersion | S→C | Supported codecs |

Message payloads use a field-tagged, length-delimited wire format (similar layout to Mumble protobuf). The implementation uses a minimal hand-written encoder/decoder — **no protobuf library** — to reduce flash and RAM usage. See go-mumble-server's `pkg/mumble/protocol/wire` for reference.

### UDP Voice Packet Format

Client-to-server:

```
┌──────────────────────────────┬─────────────────┬───────────────────────┬───────────────┐
│ Header byte                  │ Sequence (varint)│ Payload len (varint) │ Opus frame    │
│ codec(3b) | target(5b)       │                  │ bit 13 = terminator  │               │
└──────────────────────────────┴─────────────────┴───────────────────────┴───────────────┘
```

Server-to-client (adds sender session):

```
┌──────────────┬──────────────────┬─────────────────┬───────────────────────┬───────────────┐
│ Header byte  │ Session (varint) │ Sequence (varint)│ Payload len (varint) │ Opus frame    │
└──────────────┴──────────────────┴─────────────────┴───────────────────────┴───────────────┘
```

- Codec field: `4` = Opus (the only codec this client supports).
- Target field: `0` = normal (current channel), `31` = server loopback (useful for testing).
- Varint encoding uses Mumble's **custom** format (not standard LEB128): the first-byte bit pattern determines byte count. See `research/go-mumble-server/docs/protocol/voice-data.md` and gumble's varint package. Do not assume protobuf-style 7-bit groups.

This client uses the **legacy binary format**; the server also supports a modern wire format (Mumble 1.5+) but legacy is sufficient and widely compatible.

### UDP Ping

UDP connectivity is confirmed by a ping/echo exchange. The client sends a codec-type-1 packet (encrypted in Legacy mode); the server decrypts it, matches the session, and echoes it back. If no echo is received within the timeout, the client falls back to tunneling voice over TCP using UDPTunnel (message type 1). On same-LAN setups, failure to receive the echo typically indicated an OCB2 compatibility bug (resolved: byte layout now matches grumble).

### Protocol Reference

The authoritative protocol documentation lives in `research/go-mumble-server/docs/protocol/`:

- [control-messages.md](research/go-mumble-server/docs/protocol/control-messages.md) — TCP message catalog and framing
- [voice-data.md](research/go-mumble-server/docs/protocol/voice-data.md) — UDP voice packet format and varint encoding
- [security-modes.md](research/go-mumble-server/docs/protocol/security-modes.md) — Crypto modes (Lite, Legacy, Secure)

## Audio Pipeline

### Capture Path (Microphone to Network) — *Not yet implemented*

```
I2S Mic ──► DMA Buffer ──► [multi-ch downmix/beamform] ──► AEC ──► Opus Encoder ──► UDP ──► WiFi TX
  (planned)
```

Microphone capture and transmit are not yet implemented. The planned pipeline: I2S capture → downmix/beamforming → AEC (with speaker reference) → Opus encode → Mumble UDP packet framing → send to server.

### Playback Path (Network to Speaker) — *Implemented*

```
WiFi RX ──► UDP Packet ──► Parse Header ──► Opus Decoder ──► Mixer ──► Resampler ──► I2S Speaker
                                                                            │
                                                              AEC ref ◄─────┘
```

1. **UDP receive**: Voice packets arrive from the server containing another user's session ID, sequence number, and Opus frame.
2. **Jitter buffer**: Packets are held in a small buffer (~40 ms prebuffer, 320 ms capacity) to smooth arrival jitter. All available frames are flushed to the speaker's ring buffer each loop iteration; the I2S DMA handles real-time playout timing.
3. **Opus decoding**: libopus decodes the compressed frame to 16 kHz PCM.
4. **Mixing**: If multiple users are speaking simultaneously, their decoded PCM streams are summed.
5. **Resampling**: If the DAC runs at a different rate (e.g. 48 kHz for ES8311), a sample rate converter upsamples the 16 kHz PCM.
6. **I2S playback**: PCM samples are written to the I2S DMA buffer for the speaker/DAC. This output also feeds the AEC reference input.

### FreeRTOS Task Layout

| Task | Core | Priority | Responsibility |
|------|------|----------|----------------|
| `mumble_control` | 0 | Medium | TCP/TLS connection, message handling, ping |
| `audio_capture` | 1 | High | I2S read, AEC, Opus encode, UDP send |
| `audio_playback` | 1 | High | UDP receive, jitter buffer, Opus decode, I2S write |
| `esphome_loop` | 0 | Medium | ESPHome component loop, HA API, entity updates |

Pinning the audio tasks to core 1 keeps them isolated from network and ESPHome overhead on core 0. A shared ring buffer with FreeRTOS queue notifications connects the capture/playback tasks to the network tasks.

### Acoustic Echo Cancellation

In always-on mode the speaker output will be picked up by the microphone, creating an echo loop. AEC is addressed by feeding the speaker's playback PCM as a reference signal to the echo cancellation algorithm:

- **ESP-SR AFE**: Espressif's audio front-end library includes AEC, noise suppression, and VAD. Runs on the ESP32-S3's dedicated vector instructions.
- **ESP-ADF AEC**: The Audio Development Framework provides an AEC pipeline element that can be inserted between I2S capture and the application.
- **Fallback**: On boards or configurations where AEC is unavailable, the system degrades to push-to-talk only (muting the mic while the speaker is active).

### Microphone Array Support

Boards with multi-channel ADCs (ES7210 on ESP32-S3 Box / Box 3) can capture from 2-4 microphones simultaneously. The audio pipeline supports:

- **Beamforming**: Spatial filtering to focus on a direction and reject off-axis noise.
- **Channel selection**: Use a single preferred channel from the array.
- **Downmix**: Simple average or weighted sum of channels to mono.

The pipeline order is: multi-channel capture → beamforming/downmix (to mono) → AEC (speaker reference subtracted from mic) → Opus encode. Beamforming runs before AEC to provide the cleanest mono input for echo cancellation.

## ESPHome Integration

### External Component Structure

```
components/
  mumble/
    __init__.py          # ESPHome component registration, YAML schema
    mumble_component.h   # Main component; settings, microphone switch, PTT
    mumble_component.cpp
    mumble_client.h      # MumbleClient C++ class (connection, protocol)
    mumble_client.cpp
    mumble_audio.h       # Audio pipeline (capture, playback, Opus)
    mumble_audio.cpp
    mumble_protocol.h    # Wire format encoding/decoding, message structs
    mumble_protocol.cpp
    mumble_varint.h      # Varint encode/decode
    mumble_ocb2.h/.cpp   # OCB2-AES128 for Legacy UDP encryption (ported from grumble)
    mumble_udp.h/.cpp    # UDP send/recv, ping, voice packet framing
```

The component is loaded as an ESPHome external component. Connection settings (server, port, username, password, channel) can be inline or referenced from text entities; mode (always_on / push_to_talk) can be set in YAML or via an optional select entity. Username defaults to `esp32-<MAC>` on first run if not set; password has no default. All HA-linked values are editable in the HA UI and persisted to NVS. Changing server, username, password, or channel forces a reconnect. Use the **Microphone Enabled** switch with `mumble.microphone_enable` and `mumble.microphone_disable` for explicit on/off control. The `mumble.ptt_press` action is for the physical PTT button (press-and-hold). The `mumble.reset_config` action resets all config entities to defaults.

**Empty-state "unknown" workaround** — ESPHome's `TemplateText::setup()` only calls `publish_state()` when the restored/initial value is non-empty. Text entities with `initial_value: ""` (or omitted) never get a state published, so Home Assistant displays them as "unknown". The Mumble component works around this by calling `publish_empty_text_defaults()` during `setup()`, which force-publishes an empty string for any text entity that still has `has_state() == false`. Always use `initial_value: ""` in the YAML (not omitted) so the intent is clear, and rely on the component to publish the state.

**Boot-time restore of controls** — ESPHome template entities with `set_action` or `lambda` do not re-fire their actions when NVS values are restored on boot. Speaker Volume (template number) and Microphone Enabled (template switch) would lose their saved state without explicit handling. The fix uses two mechanisms:

1. **`on_boot` at priority `-100`** (runs after all template components have restored NVS values): reads the stored Speaker Volume and applies it via `speaker.volume_set`; reads the stored Microphone Enabled state and calls `set_microphone_enabled()` on the component.
2. **`EsphomeSpeakerSink::start()` re-applies volume** — `speaker->set_volume(speaker->get_volume())` after `start()` ensures the DAC register matches the stored volume even if audio hardware re-initializes when the speaker starts.

The Microphone Enabled switch uses `optimistic: true` + `restore_mode: RESTORE_DEFAULT_OFF` instead of a polling lambda, so its NVS state is managed by ESPHome's native switch restore mechanism.

```yaml
external_components:
  - source: { type: local, path: components }

# Optional: text/select entities for HA-editable settings (restore_value: true)
# Port uses text (not number) so it displays as integer in HA
text:
  - platform: template
    id: mumble_server_host
    name: "1. Server"
    mode: text
    optimistic: true
    restore_value: true
    initial_value: "192.168.1.100"
  - platform: template
    id: mumble_server_port
    name: "2. Port"
    mode: text
    optimistic: true
    restore_value: true
    initial_value: "64738"
select:
  - platform: template
    id: mumble_mode
    name: "6. Mode"
    optimistic: true
    restore_value: true
    initial_option: "always_on"
    options: ["always_on", "push_to_talk"]
switch:
  - platform: template
    name: "Microphone Enabled"
    id: mumble_microphone_enabled
    optimistic: true
    restore_mode: RESTORE_DEFAULT_OFF
    turn_on_action:
      - mumble.microphone_enable: mumble_client
    turn_off_action:
      - mumble.microphone_disable: mumble_client

mumble:
  id: mumble_client
  server: ""              # or inline; if using entities use server_text_id
  port: 64738
  username: ""
  password: ""
  channel: ""
  server_text_id: mumble_server_host
  port_text_id: mumble_server_port
  # username_text_id, password_text_id, channel_text_id, crypto_select_id ...
  mode_select_id: mumble_mode
  microphone_switch_id: mumble_microphone_enabled
  mode: always_on
  mute_pin: GPIO38        # optional hardware mute switch
```

### YAML Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `server` | string | optional | Mumble server hostname or IP (or use `server_text_id`) |
| `port` | int | 64738 | Mumble server port |
| `username` | string | optional | Username to authenticate with (or use `username_text_id`) |
| `password` | string | `""` | Server or user password |
| `channel` | string | `""` | Channel to join after connect (empty = root) |
| `mode_select_id` | id | none | Select entity for mode (options: `always_on`, `push_to_talk`; overrides `mode` when set) |
| `mode` | enum | `always_on` | `always_on` or `push_to_talk` (fallback when no `mode_select_id`) |
| `server_text_id` | id | none | Text entity for server host (overrides `server` when set) |
| `port_text_id` | id | none | Text entity for port (displays as integer in HA) |
| `username_text_id` | id | none | Text entity for username |
| `password_text_id` | id | none | Text entity for password |
| `channel_text_id` | id | none | Text entity for default channel |
| `crypto_select_id` | id | none | Select entity for crypto (`legacy` or `lite`) |
| `microphone_switch_id` | id | none | Switch entity for Microphone Enabled (Controls) |
| `ptt_pin` | pin | none | GPIO for push-to-talk button (press-and-hold; required if mode is push_to_talk) |
| `mute_pin` | pin | none | GPIO for hardware mute switch |
| `crypto` | enum | `legacy` | Crypto mode: `legacy` (default, OCB2-AES128) or `lite` (cleartext UDP). Use `crypto_select_id` for HA entity. Changing crypto forces reconnect. |

### Home Assistant Entities

**Configuration entities** -- editable from the HA UI, persisted to NVS:

| Entity ID | Platform | Description |
|-----------|----------|-------------|
| `text.mumble_server_host` | `text` | Server hostname or IP |
| `text.mumble_server_port` | `text` | Server port (integer, no decimals) |
| `select.mumble_mode` | `select` | Mode: `always_on` or `push_to_talk` |
| `text.mumble_username` | `text` | Username |
| `text.mumble_password` | `text` | Password (mode: password) |
| `text.mumble_default_channel` | `text` | Channel to join on connect |
| `select.mumble_crypto` | `select` | Crypto: `legacy` or `lite` |

When server, username, password, or channel is changed in HA, the client disconnects immediately and reconnects with the new settings. YAML values serve as initial defaults that are overridden once HA values are set. Channel name lookup is case-insensitive (e.g. `root` matches `Root`).

**Controls**:

| Entity ID | Platform | Description |
|-----------|----------|-------------|
| `switch.mumble_microphone_enabled` | `switch` | Microphone Enabled (explicit on/off; persists via NVS) |
| `number.mumble_speaker_volume` | `number` | Speaker volume 0–100 (persists via NVS; applied on boot) |
| `switch.speaker_power` | `switch` | Speaker Power — PA enable GPIO46 (Box/Box-3 only; persists) |

**Diagnostics**:

| Entity ID | Platform | Description |
|-----------|----------|-------------|
| `sensor.wifi_signal` | `sensor` | WiFi RSSI (dBm) |
| `binary_sensor.mumble_connected` | `binary_sensor` | Connected to Mumble server |
| `sensor.mumble_ping` | `sensor` | Round-trip ping time to server in ms |
| `binary_sensor.mumble_voice_active` | `binary_sensor` | Voice Received: true while voice is being received (Sensors) |
| `button.mumble_reset_config` | `button` | Reset all config to defaults (server, port, username, password, channel, mode) |

**Runtime entities**:

| Entity ID | Platform | Status | Description |
|-----------|----------|--------|-------------|
| `switch.mumble_microphone_enabled` | `switch` | Implemented | Microphone Enabled (Controls; NVS restore on boot) |
| `number.mumble_speaker_volume` | `number` | Implemented | Speaker volume 0–100 (NVS restore on boot) |
| `switch.speaker_power` | `switch` | Implemented | Speaker Power — PA enable (Box/Box-3 only) |
| `binary_sensor.mumble_connected` | `binary_sensor` | Implemented | Connection status (Diagnostics) |
| `binary_sensor.mumble_voice_active` | `binary_sensor` | Implemented | Voice Received (Sensors, with Microphone) |
| `select.mumble_channel` | `select` | Optional | Channel select with server options (`channel_select_id`) |
| `binary_sensor.mumble_talking` | `binary_sensor` | Planned | Transmitting audio |

## Hardware Abstraction

Board-specific details (pin assignments, codec I2C addresses, I2S parameters) are isolated in per-board YAML packages and C++ board profiles. The audio pipeline code operates on abstract interfaces:

- **MicrophoneSource**: Provides mono 16 kHz PCM frames. Implemented per-board to handle I2S configuration, multi-channel downmix, and PDM vs standard I2S.
- **SpeakerSink**: Accepts mono 16 kHz PCM frames and handles resampling and I2S output to the DAC/amplifier.
- **BoardConfig**: Pin map, I2C bus configuration, LED strip parameters, button/touch GPIO.

### Board Profiles

**ESP32-S3 Box 3**
- I2S: LRCLK=GPIO45, BCLK=GPIO17, MCLK=GPIO2, DIN=GPIO16, DOUT=GPIO15
- I2C: SCL=GPIO18, SDA=GPIO8
- ADC: ES7210 (4-ch, 16 kHz, 16-bit)
- DAC: ES8311 (48 kHz, mono)
- Speaker Power (PA enable): GPIO46

**ESP32-S3 Box** (older revision)
- I2S: LRCLK=GPIO47, BCLK=GPIO17, MCLK=GPIO2, DIN=GPIO16, DOUT=GPIO15
- I2C: SCL=GPIO18, SDA=GPIO8
- ADC: ES7210 (4-ch, 16 kHz, 16-bit)
- DAC: ES8311 (48 kHz, mono)
- Speaker Power (PA enable): GPIO46

**Onju Voice**
- I2S: LRCLK=GPIO13, BCLK=GPIO18, DIN=GPIO17, DOUT=GPIO12
- Mic: SPH0645 (I2S, mono)
- Amp: MAX98357A (I2S, mono)
- Mute: GPIO38 (switch), GPIO21 (amp enable, inverted)
- LEDs: GPIO11 (SK6812, 6 LEDs)
- Touch: GPIO4 (vol-), GPIO2 (vol+), GPIO3 (action)

**M5Stack Atom Echo**
- I2S: DIN=GPIO23 (PDM mic)
- Speaker: external I2S DAC (DOUT pin board-dependent; see M5Stack docs)
- Compact; no touch or LED ring

**Generic**
- All pins user-defined in YAML
- Assumes single I2S mic + single I2S speaker

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| ESP-IDF | 5.x | SoC framework, FreeRTOS, I2S, Wi-Fi, mbedTLS |
| ESPHome | 2024.x or later | Component framework, HA integration, OTA |
| micro-opus | (local, `lib/micro-opus/`) | Opus audio codec; heap pseudostack, PSRAM, Xtensa DSP; based on esphome-libs/micro-opus |
| mbedTLS | (bundled with ESP-IDF) | TLS for Mumble control channel; OCB2-AES128 for Legacy crypto |
| ESP-ADF | optional | ADF pipeline for full-duplex I2S |
| ESP-SR | optional | AEC, noise suppression, beamforming |

The build uses ESPHome/PlatformIO. The Opus library is **vendored locally** in `lib/micro-opus/` — a pre-patched copy of [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus) with heap-based pseudostack (avoids stack overflow from alloca-based builds), PSRAM-aware allocation, and Xtensa LX7 optimizations for ESP32-S3.

## Key Technical Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Opus encode/decode CPU cost | Audio dropouts or high latency | Use 16 kHz narrowband mode (~3 ms encode per 20 ms frame on S3). Pin audio tasks to dedicated core. |
| Full-duplex I2S reliability | Glitches, buffer underruns | Follow the ADF duplex pipeline pattern proven in Onju Voice. Fall back to half-duplex with mute-on-play if needed. |
| AEC quality | Echo leaks in always-on mode | Start with ESP-SR AFE. Allow fallback to push-to-talk if AEC is insufficient for a board. |
| RAM budget | Opus + TLS + ESPHome may exceed SRAM | Use PSRAM for audio buffers and Opus state. Optimize TLS buffer sizes. Target boards with 8 MB PSRAM. |
| TLS handshake time | Slow reconnects | Cache TLS sessions. Use TLS 1.2 with minimal cipher suite. |
| Wire format encoding | Compatibility with upstream Mumble | Hand-written encoder (field-tagged, no protobuf lib) matched to go-mumble-server's wire format. Integration test against live server. |
| Legacy crypto (OCB2-AES128) | Compatibility with server | Implemented; ported from grumble. Byte layout and GF operations match go-mumble-server. Lite mode available for trusted LAN. |
