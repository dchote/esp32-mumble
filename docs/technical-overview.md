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
  │──── Version (CryptoModes=0x01 or 0x03) ────────►│  Lite only or Lite+Legacy
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

**Channel changes** are performed by sending a `UserState` message (type 9) with the new `channel_id`. The `select.mumble_channel` entity drives this when the user switches channels in Home Assistant.

### Crypto Modes

go-mumble-server negotiates security per client during the Version exchange. The ESP32 client advertises supported modes; the server selects the best mutual tier.

| Mode | CryptoModes bit | UDP treatment | Use case |
|------|-----------------|---------------|----------|
| **Lite** | 0x01 | Cleartext, no per-packet crypto | Trusted LAN, minimal CPU; **recommended default** |
| **Legacy** | 0x02 | OCB2-AES128, 4-byte overhead | Standard Mumble servers (Murmur, go-mumble-server); **optional**, chipset may lack headroom |

**Lite mode** — Client sends `CryptoModes = 0x01`. Server responds with CryptSetup (empty key). UDP voice is cleartext; TCP control remains TLS-encrypted. Suitable for trusted LAN deployments.

**Legacy mode (standard Mumble)** — Client sends `CryptoModes = 0x02` or `0x03` (legacy or both). Server responds with CryptSetup (16-byte key). UDP voice is encrypted with OCB2-AES128. Compatible with any standard Mumble server. **ESP32-S3 support is uncertain**: OCB2 encrypt/decrypt per 20 ms frame adds CPU and latency. Will be implemented as an optional build-time or runtime option; if benchmarks show insufficient headroom, Legacy will remain disabled by default or unsupported on constrained boards.

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

UDP connectivity is confirmed by a ping/echo exchange. The client sends a codec-type-1 packet; the server echoes it back. If no echo is received, the client falls back to tunneling voice over TCP using UDPTunnel (message type 1).

### Protocol Reference

The authoritative protocol documentation lives in `research/go-mumble-server/docs/protocol/`:

- [control-messages.md](research/go-mumble-server/docs/protocol/control-messages.md) — TCP message catalog and framing
- [voice-data.md](research/go-mumble-server/docs/protocol/voice-data.md) — UDP voice packet format and varint encoding
- [security-modes.md](research/go-mumble-server/docs/protocol/security-modes.md) — Crypto modes (Lite, Legacy, Secure)

## Audio Pipeline

### Capture Path (Microphone to Network)

```
I2S Mic ──► DMA Buffer ──► [multi-ch downmix/beamform] ──► AEC ──► Opus Encoder ──► UDP ──► WiFi TX
  │                              ▲                           ▲
  │                              │                           │
  └── (multi-ch mic array) ──────┘                           │
                                                             │
Speaker playback reference (reference input) ────────────────┘
```

1. **I2S capture**: The microphone feeds PCM samples via I2S DMA into a ring buffer. Sample rate is 16 kHz, 16-bit, mono. Boards with multi-channel ADCs (ES7210) capture multiple channels; a downmix or beamforming stage reduces to mono before encoding.
2. **Echo cancellation**: The AEC module (ESP-SR or ESP-ADF) subtracts the speaker's playback signal from the microphone input to prevent feedback loops in always-on mode.
3. **Opus encoding**: libopus encodes 20 ms frames (320 samples at 16 kHz) into compressed packets at ~16 kbps. The encoder runs in VOIP mode with constrained VBR.
4. **Packet framing**: The Opus frame is wrapped in the Mumble UDP voice packet format (header byte + sequence varint + length varint + payload).
5. **UDP transmit**: The packet is sent to the server. In Lite mode no encryption is applied; in Legacy mode OCB2-AES128 encrypts the packet before send.

### Playback Path (Network to Speaker)

```
WiFi RX ──► UDP Packet ──► Parse Header ──► Opus Decoder ──► Mixer ──► Resampler ──► I2S Speaker
                                                                            │
                                                              AEC ref ◄─────┘
```

1. **UDP receive**: Voice packets arrive from the server containing another user's session ID, sequence number, and Opus frame.
2. **Jitter buffer**: Packets are held in a small buffer (40-100 ms) to smooth out network jitter. Packets arriving out of order are resequenced; late packets are dropped.
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
    mumble_component.h   # Main component; settings, PTT action
    mumble_component.cpp
    mumble_client.h      # MumbleClient C++ class (connection, protocol)
    mumble_client.cpp
    mumble_audio.h       # Audio pipeline (capture, playback, Opus)
    mumble_audio.cpp
    mumble_protocol.h    # Wire format encoding/decoding, message structs
    mumble_protocol.cpp
    mumble_varint.h      # Varint encode/decode
```

The component is loaded as an ESPHome external component. Connection settings can be inline or referenced from text/number entities (editable in the HA UI, persisted to NVS). Use the `mumble.ptt_press` action for a Push to Talk button in HA (toggle: press to start, press again to stop).

```yaml
external_components:
  - source: { type: local, path: components }

# Optional: text/number entities for HA-editable settings (restore_value: true)
text:
  - platform: template
    id: mumble_server_host
    name: "Mumble Server"
    mode: text
    optimistic: true
    restore_value: true
    initial_value: "192.168.1.100"
number:
  - platform: template
    id: mumble_server_port
    name: "Mumble Port"
    optimistic: true
    restore_value: true
    initial_value: 64738
    min_value: 1
    max_value: 65535
    step: 1
button:
  - platform: template
    name: "Mumble Push to Talk"
    on_press:
      - mumble.ptt_press: mumble_client

mumble:
  id: mumble_client
  server: ""              # or inline; if using entities use server_text_id
  port: 64738
  username: ""
  password: ""
  channel: ""
  server_text_id: mumble_server_host
  port_number_id: mumble_server_port
  # username_text_id, password_text_id, channel_text_id ...
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
| `mode` | enum | `always_on` | `always_on` or `push_to_talk` |
| `server_text_id` | id | none | Text entity for server host (overrides `server` when set) |
| `port_number_id` | id | none | Number entity for port |
| `username_text_id` | id | none | Text entity for username |
| `password_text_id` | id | none | Text entity for password |
| `channel_text_id` | id | none | Text entity for default channel |
| `ptt_pin` | pin | none | GPIO for push-to-talk button (required if mode is push_to_talk) |
| `mute_pin` | pin | none | GPIO for hardware mute switch |
| `crypto` | enum | `lite` | Crypto mode: `lite` (recommended) or `legacy` (standard Mumble; optional, CPU-permitting) |

### Home Assistant Entities

**Configuration entities** -- editable from the HA UI, persisted to NVS:

| Entity ID | Platform | Description |
|-----------|----------|-------------|
| `text.mumble_server_host` | `text` | Server hostname or IP |
| `number.mumble_server_port` | `number` | Server port |
| `text.mumble_username` | `text` | Username |
| `text.mumble_password` | `text` | Password (mode: password) |
| `text.mumble_default_channel` | `text` | Channel to join on connect |

When a configuration entity is changed in HA, the new value is written to NVS and the client reconnects with the updated settings. YAML values serve as initial defaults that are overridden once HA values are set.

**Runtime entities** -- reflect live state (planned; not yet implemented):

| Entity ID | Platform | Description |
|-----------|----------|-------------|
| `switch.mumble_mute` | `switch` | Microphone mute |
| `number.mumble_volume` | `number` | Speaker volume (0-100) |
| `select.mumble_channel` | `select` | Active channel (options from server) |
| `binary_sensor.mumble_talking` | `binary_sensor` | Transmitting audio |
| `binary_sensor.mumble_connected` | `binary_sensor` | Connected to server |

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
- Speaker enable: GPIO46

**ESP32-S3 Box** (older revision)
- I2S: LRCLK=GPIO47, BCLK=GPIO17, MCLK=GPIO2, DIN=GPIO16, DOUT=GPIO15
- I2C: SCL=GPIO18, SDA=GPIO8
- ADC: ES7210 (4-ch, 16 kHz, 16-bit)
- DAC: ES8311 (48 kHz, mono)
- Speaker enable: GPIO46 (if present)

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
| libopus | 1.4+ | Opus audio encoding and decoding |
| mbedTLS | (bundled with ESP-IDF) | TLS for Mumble control channel |
| ESP-ADF | optional | ADF pipeline for full-duplex I2S |
| ESP-SR | optional | AEC, noise suppression, beamforming |

The build uses ESP-IDF's CMake system. ESPHome compiles the external component as part of its normal build process, pulling in the Opus library as an ESP-IDF managed component or a vendored source tree.

## Key Technical Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Opus encode/decode CPU cost | Audio dropouts or high latency | Use 16 kHz narrowband mode (~3 ms encode per 20 ms frame on S3). Pin audio tasks to dedicated core. |
| Full-duplex I2S reliability | Glitches, buffer underruns | Follow the ADF duplex pipeline pattern proven in Onju Voice. Fall back to half-duplex with mute-on-play if needed. |
| AEC quality | Echo leaks in always-on mode | Start with ESP-SR AFE. Allow fallback to push-to-talk if AEC is insufficient for a board. |
| RAM budget | Opus + TLS + ESPHome may exceed SRAM | Use PSRAM for audio buffers and Opus state. Optimize TLS buffer sizes. Target boards with 8 MB PSRAM. |
| TLS handshake time | Slow reconnects | Cache TLS sessions. Use TLS 1.2 with minimal cipher suite. |
| Wire format encoding | Compatibility with upstream Mumble | Hand-written encoder (field-tagged, no protobuf lib) matched to go-mumble-server's wire format. Integration test against live server. |
| Legacy crypto (OCB2-AES128) | CPU may be insufficient on ESP32-S3 | Implement as optional; benchmark before enabling by default. Lite mode remains primary for trusted LAN. |
