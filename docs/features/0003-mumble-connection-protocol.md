# 0003 -- Mumble Connection and Protocol Handling

## Summary

Implement the Mumble TCP/TLS control channel on ESP32: protobuf wire encoder/decoder, message structs for handshake and sync, TLS connection via WiFiClientSecure, authentication flow, channel/user state tracking, TCP ping keepalive, and reconnection logic. Integrates MumbleClient into MumbleComponent lifecycle.

## Implementation

### 1. Protobuf Wire Codec (mumble_protocol.h/.cpp)

- `proto_append_tag`, `proto_append_varint`, `proto_append_fixed32`, `proto_append_string`, `proto_append_bytes`, `proto_append_bool` for encoding
- `proto_read_varint`, `proto_read_tag`, `proto_read_length_delimited`, `proto_read_fixed32`, `proto_skip_field` for decoding
- Standard protobuf wire format: field-tagged, 7-bit varints, little-endian fixed32/fixed64

### 2. Message Structs (mumble_messages.h/.cpp)

- **MsgVersion** (encode+decode), **MsgAuthenticate** (encode), **MsgCryptSetup** (decode)
- **MsgServerSync**, **MsgChannelState**, **MsgChannelRemove** (decode)
- **MsgUserState** (decode + marshal_channel_change for channel changes)
- **MsgUserRemove**, **MsgPing** (encode+decode), **MsgReject** (decode)
- **MsgCodecVersion**, **MsgServerConfig** (decode)

### 3. MumbleClient (mumble_client.h/.cpp)

- **TLS**: WiFiClientSecure with setInsecure() for trusted LAN
- **State machine**: DISCONNECTED → CONNECTING → AUTHENTICATING → CONNECTED
- **Handshake**: Server sends Version → client sends Version + Authenticate → server sends CryptSetup, CodecVersion, ChannelState[], UserState[], ServerSync, ServerConfig
- **Channel/User tracking**: ChannelInfo, UserInfo structs; update_channel, remove_channel, update_user, remove_user
- **Ping keepalive**: TCP Ping every 15s, RTT measurement, 45s timeout
- **Reconnection**: Exponential backoff 1s–30s, config re-read on each attempt
- **Channel join**: find_channel_by_name, send UserState with channel_id after ServerSync

### 4. MumbleComponent Integration

- Owns MumbleClient; passes config in setup() and loop()
- Updates connected_, ping_ms_ from client state
- Config changes (e.g. from HA entities) picked up on next connect

### 5. Build

- Added `cg.add_library("WiFi", None)` and `cg.add_library("NetworkClientSecure", None)` for ESPHome 2026.2+ (Arduino libs disabled by default)

## Not in Scope (this feature)

- UDP voice transport
- Opus encode/decode
- UDP ping and TCP voice fallback

Crypto modes and CryptSetup are implemented in the UDP/voice layer: **Legacy** (default, OCB2-AES128), **Lite** (cleartext), and **Secure** (AES-256-GCM when server sends 32-byte key). CryptSetup key length indicates mode (0 / 16 / 32 bytes). See [0004-udp-voice-playback.md](0004-udp-voice-playback.md) and [technical-overview.md](../technical-overview.md) (Security modes).
