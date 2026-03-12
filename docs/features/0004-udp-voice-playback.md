# 0004 -- UDP Voice Transport and Audio Playback

## Summary

Implement UDP voice transport (send/receive), OCB2-AES128 encryption (Legacy mode), Opus decoding, jitter buffering, and I2S speaker output. Also adds Lite/Legacy crypto selection from Home Assistant, a "Voice Received" binary sensor, and a UDP ping/echo mechanism for UDP connectivity detection with TCP tunnel fallback.

## Implementation

### 1. UDP Transport (mumble_udp.h/.cpp)

- **WiFiUDP** socket on ephemeral port; sends to server:64738
- **UDP ping**: Codec-type-1 packet sent every 5s; server echoes it back. If echo received, `udp_active_` = true and voice flows over UDP. If no echo within 5s, falls back to TCP tunnel (UDPTunnel, message type 1).
- **Encryption**: Packets encrypted/decrypted via `MumbleCryptState` when in Legacy mode; cleartext in Lite mode.
- **Audio callback**: Decrypted voice packets forwarded to `MumbleComponent::on_voice_packet()`.

### 2. OCB2-AES128 Encryption (mumble_ocb2.h/.cpp)

- Ported from [mumble-voip/grumble](https://github.com/mumble-voip/grumble) OCB2 implementation.
- Uses mbedTLS `aes_crypt_ecb` for AES block operations.
- Byte-oriented GF(2^128) doubling (`times2`/`times3`) matching grumble exactly: `block[0]` = MSB, polynomial 0x87, partial-block length at bytes 14–15.
- 4-byte overhead per packet: 1 nonce byte + 3-byte truncated authentication tag.
- Nonce management: encrypt IV incremented per packet (little-endian counter); decrypt IV tracked with replay detection and late-packet handling.
- XEX\* attack countermeasure (per IACR 2019/311) in encrypt path.

### 3. Crypto Mode Selection

- **CryptoModes in Version message**: Client advertises only the user-selected mode (`CRYPTO_LITE = 0x01` or `CRYPTO_LEGACY = 0x02`) so the server negotiates the correct tier.
- **HA select entity** (`crypto_select_id`): "7. Crypto" with options `legacy` (default) and `lite`. Changing crypto forces a reconnect so the server re-negotiates.
- **CryptSetup handling**: On Legacy, server sends 16-byte key + client/server nonces. `MumbleCryptState` initialized in `MumbleComponent::loop()` once CryptSetup is received. On Lite, CryptSetup has empty key; UDP packets flow unencrypted.
- **Reset Config** also resets crypto to `legacy`.

### 4. Voice Packet Parsing (mumble_voice.h/.cpp)

- Legacy binary format: header byte (codec 3 bits | target 5 bits), sender session (varint), sequence number (varint), payload length (varint, bit 13 = terminator), Opus frame data.
- Mumble custom varint (not protobuf LEB128).

### 5. Opus Decoding (mumble_audio.h/.cpp)

- `OpusAudioDecoder`: wraps `opus_decoder_create` / `opus_decode` from vendored micro-opus library.
- 16 kHz mono, decodes 20ms frames (320 samples).
- Uses heap-based pseudostack via micro-opus (PSRAM when available), avoiding stack overflow.

### 6. Jitter Buffer (mumble_audio.h/.cpp)

- Ring buffer of decoded PCM frames; capacity 16 frames (320ms).
- Target depth: 2 frames (40ms) before first playout.
- Drops late packets (sequence < next playout).
- Each loop iteration, all available frames are flushed to the speaker's ring buffer. The I2S DMA handles real-time playout timing, not the ESPHome main loop.

### 7. Speaker Output (mumble_audio.h/.cpp, YAML configs)

- `EsphomeSpeakerSink`: writes PCM via ESPHome `speaker->play()` API.
- Speaker started when first voice frame arrives; stopped after 500ms idle timeout.
- `start()` re-applies stored volume (`speaker->set_volume(speaker->get_volume())`) to ensure the DAC register matches even if hardware re-initializes.
- Speaker `buffer_duration: 500ms` in YAML configs provides sufficient I2S DMA headroom to absorb main-loop jitter.
- DAC config: ES8311 at 16 kHz on Box/Box3; I2S external DAC on generic/Atom Echo.

### 8. Speaker Power (Box/Box-3 only)

- GPIO switch on GPIO46 controlling the hardware power amplifier.
- Renamed from "Speaker Enable" to "Speaker Power" with `mdi:speaker` icon.
- `restore_mode: RESTORE_DEFAULT_ON` — amplifier powered on by default.
- Visible in HA Configuration section (Box/Box-3 configs only; generic and Atom Echo omit it).

### 9. Boot-Time Settings Restore

ESPHome template entities with `set_action` or `lambda` do not re-fire their actions when NVS values are restored on boot. Without explicit handling, Speaker Volume would default to 100% and Microphone Enabled would always start OFF.

- **`on_boot` at priority `-100`**: Runs after all template components have restored NVS values. Applies stored Speaker Volume via `speaker.volume_set` and stored Microphone Enabled via `set_microphone_enabled()`.
- **Microphone switch**: Changed from polling `lambda` (which read `false` from the freshly-booted component) to `optimistic: true` + `restore_mode: RESTORE_DEFAULT_OFF`. The component's `set_microphone_enabled()` calls `publish_state()` on the switch to keep it in sync after boot.

### 10. Voice Received Binary Sensor

- `binary_sensor.mumble_voice_active` ("Voice Received") in the Sensors section of HA.
- True while voice frames are being decoded and played; goes false after 500ms idle.
- Replaced the earlier "Voice Packets" counter sensor which was flooding HA history logs.

### 11. TCP Tunnel Fallback

- `MumbleClient::on_udp_tunnel()` receives voice packets via TCP (message type 1) when UDP is not available.
- Same `on_voice_packet()` callback path as UDP; transparent to the audio pipeline.

## Files Changed

| File | Change |
|------|--------|
| `components/mumble/mumble_udp.h/.cpp` | New: UDP socket, ping, send/recv, encryption integration |
| `components/mumble/mumble_ocb2.h/.cpp` | New: OCB2-AES128 encrypt/decrypt, ported from grumble |
| `components/mumble/mumble_audio.h/.cpp` | New: OpusAudioDecoder, JitterBuffer, AudioMixer, EsphomeSpeakerSink (start re-applies volume) |
| `components/mumble/mumble_voice.h/.cpp` | New: VoicePacket struct, parse_voice_packet() |
| `components/mumble/mumble_component.h/.cpp` | Added: voice pipeline wiring, playout loop, crypto init, Voice Received |
| `components/mumble/mumble_client.h/.cpp` | Added: crypto_mode_, on_udp_tunnel(), CryptoModes in Version |
| `components/mumble/mumble_messages.h/.cpp` | Added: CryptSetup marshal (for nonce resync) |
| `components/mumble/__init__.py` | Added: crypto, crypto_select_id, speaker_id schema entries |
| `esphome/*.yaml` (all 4 configs) | Added: speaker, crypto select, Voice Received sensor, buffer_duration: 500ms, on_boot restore (volume + mic), Speaker Power (Box/Box-3), optimistic mic switch |
| `lib/micro-opus/` | Vendored Opus library (heap pseudostack, PSRAM, Xtensa) |

## Not in Scope

- Microphone capture and Opus encoding (transmit path)
- Full-duplex operation
- Acoustic echo cancellation
- Beamforming / multi-channel mic input
- Secure mode (AES-256-GCM)
