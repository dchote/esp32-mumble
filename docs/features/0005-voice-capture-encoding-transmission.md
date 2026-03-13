# 0005 -- Voice Capture, Encoding, and Transmission

## Summary

Implements the full microphone-to-network transmit pipeline: I2S microphone capture via ESPHome microphone component, energy-based VAD, echo suppression, Opus encoding (16 kHz mono, ~16 kbps), Mumble voice packet framing, and UDP transmission with TCP tunnel fallback. Supports both `always_on` (VAD-gated) and `push_to_talk` modes. Adds "Voice Sending" binary sensor and TX start/stop logging.

**Status note:** Voice transmit is not 100%: UDP path works for pings; the Mumble server may report `SendAudio no UDP path` when forwarding to the ESP32; voice may use TCP tunnel fallback. Server-side UDP address handling is under investigation.

## Implementation

### 1. Opus Encoder (mumble_audio.h/.cpp)

- **OpusAudioEncoder** class: `opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP)`, bitrate 16 kbps, complexity 1, VBR and DTX enabled.
- **encode()**: One 20 ms frame (320 samples) in, Opus payload out; returns bytes written or negative on error.
- Encoder state on heap (PSRAM when available via micro-opus).

### 2. Voice Packet Builder (mumble_voice.h/.cpp)

- **build_voice_packet()**: Client-to-server legacy format: header byte (Opus codec 4, target 0), sequence varint, payload length varint (bit 13 = terminator), Opus payload. No session field (server adds it when relaying).

### 3. TCP Tunnel Fallback (mumble_client.h/.cpp)

- **send_udp_tunnel()**: Sends raw voice packet in TCP message type 1 (UDPTunnel) when UDP is not active.

### 4. Component Schema (__init__.py)

- **microphone_id**: Optional reference to an ESPHome `microphone` component. Resolved in `to_code()` and passed to `set_microphone()`.

### 5. YAML Config (all 4 board configs)

- **ESP32-S3 Box / Box 3**: `audio_adc` (es7210), `microphone` (i2s_audio, DIN GPIO16, external ADC), "Voice Sending" template binary sensor, `microphone_id: box_mic`.
- **M5Stack Atom Echo**: `microphone` (i2s_audio, DIN GPIO23, PDM, correct_dc_offset), "Voice Sending" sensor, `microphone_id: echo_microphone`.
- **Generic ESP32-S3**: `microphone` (placeholder DIN GPIO7), "Voice Sending" sensor, `microphone_id: generic_mic`.

### 6. Transmit Pipeline (mumble_component.h/.cpp)

- **Capture**: `on_microphone_data()` callback appends PCM (from `std::vector<uint8_t>`) into a ring buffer (8 frames = 2560 samples).
- **VAD**: Energy-based RMS per 20 ms frame; threshold 200; 3 frames above = voice start, 15 frames below = voice stop (terminator sent).
- **Echo suppression**: In always_on mode, when receiving or within 100 ms after it stops, TX encoding is suppressed (echo suppress tail).
- **Half-duplex I2S bus management**: ESPHome's I2S component allows only one consumer (mic or speaker) at a time. `manage_i2s_bus()` centralizes all start/stop: when receiving voice, mic is stopped and speaker started; when receiving stops, speaker is stopped and mic restarted. Uses `microphone_->is_stopped()` and `speaker_->is_stopped()` to wait for bus release before starting the other, avoiding "Parent I2S bus not free" errors.
- **State machine**: IDLE (mic stopped) → CAPTURING (mic on, waiting for VAD) → TRANSMITTING (encoding and sending). When VAD releases, send terminator and return to CAPTURING.
- **Send path**: Encode frame → build_voice_packet() → `udp_.send_audio()` if UDP active, else `client_.send_udp_tunnel()`.
- **Voice Sending**: `get_voice_sending()` true while in TRANSMITTING (and during hangover until terminator). Template binary sensor "Voice Sending" in YAML uses this.
- **Logging**: "Voice TX started" when entering TRANSMITTING, "Voice TX stopped" when sending terminator or when stopping transmit (e.g. mic disabled).

### 7. Mumble Block

- All configs add `microphone_id: <mic_id>` to the mumble block.

## Files Changed

| File | Change |
|------|--------|
| components/mumble/mumble_audio.h | OpusAudioEncoder class |
| components/mumble/mumble_audio.cpp | Encoder init/encode/destroy, bitrate/complexity/DTX |
| components/mumble/mumble_voice.h | build_voice_packet() declaration |
| components/mumble/mumble_voice.cpp | build_voice_packet() implementation |
| components/mumble/mumble_client.h | send_udp_tunnel() declaration |
| components/mumble/mumble_client.cpp | send_udp_tunnel() implementation |
| components/mumble/mumble_component.h | TxState, BusOwner, microphone_, encoder, capture buffer, VAD/TX state, get_voice_sending(), set_microphone(), manage_i2s_bus() |
| components/mumble/mumble_component.cpp | on_microphone_data, should_transmit, frame_rms, send_voice_packet, audio_capture, manage_i2s_bus, setup/loop wiring |
| components/mumble/__init__.py | microphone_id schema and to_code() |
| esphome/esp32-s3-box.yaml | audio_adc, microphone, Voice Sending sensor, microphone_id |
| esphome/esp32-s3-box3.yaml | Same |
| esphome/m5stack-atom-echo.yaml | microphone (PDM), Voice Sending sensor, microphone_id |
| esphome/generic-esp32s3.yaml | microphone (placeholder), Voice Sending sensor, microphone_id |

## Constants

- Sample rate: 16 kHz; frame: 320 samples (20 ms).
- VAD threshold: 200 RMS; attack 3 frames; hangover 15 frames.
- Echo suppress tail: 100 ms after receiving stops.
- Capture buffer: 8 frames (2560 samples).
- Opus: 16 kbps, complexity 1, VOIP, DTX on.
