# 0010 – Full Mumble Protocol Implementation

## Summary

Implements all remaining Mumble protocol messages and operations in the ESP32 component so the project can be extended for bots, automation, and developer use cases. The work is organized in eight phases: message structs and dispatch, event callbacks, text messaging, user management, channel management, permissions, voice targeting, advanced protocol messages, and ESPHome integration.

## Implemented

### Phase 1: Message structs and dispatch

- **Message structs** in `mumble_messages.h` / `mumble_messages.cpp`: `MsgTextMessage`, `MsgPermissionDenied`, `MsgACL`, `MsgQueryUsers`, `MsgBanList`/`MsgBanEntry`, `MsgContextActionModify`, `MsgContextAction`, `MsgUserList`/`MsgUserListUser`, `MsgVoiceTarget`/`MsgVoiceTargetTarget`, `MsgUserStats`, `MsgRequestBlob`, `MsgSuggestConfig`, `MsgPluginDataTransmission`, `MsgPermissionQuery`. Added marshal for `MsgChannelRemove`, `MsgUserRemove`, `MsgChannelState`, and full `MsgUserState` (including `comment` and full marshal).
- **Handlers** in `mumble_client.cpp`: all 27 message types are dispatched; new handlers unmarshal and log; BanList, TextMessage, PermissionDenied, ACL, QueryUsers, ContextActionModify, ContextAction, UserList, VoiceTarget, UserStats, RequestBlob, SuggestConfig, PluginDataTransmission, PermissionQuery.
- **Event/callback system** in `MumbleClient`: `set_text_message_callback`, `set_permission_denied_callback`, `set_channel_state_callback`, `set_channel_remove_callback`, `set_user_state_callback`, `set_user_remove_callback`, `set_raw_message_callback`. Callbacks are invoked after internal state updates; raw callback receives every message.
- **Bot mode**: `set_bot_mode(bool)` on `MumbleClient`; when true, Authenticate sends `client_type = 1`.

### Phase 2: Text messaging

- **Send**: `send_text_message(message, channel_id)`, `send_text_message(message, sessions)`, `send_tree_message(message, channel_id)` on `MumbleClient`.
- **Receive**: TextMessage callback and component `on_text_message_callback_`; component stores `last_text_message_`, `last_text_sender_session_`, `last_text_sender_name_`, `last_text_channel_id_`.
- **ESPHome**: `mumble.send_text_message` action (message, channel_id); `on_text_message` trigger; automations can read last message via component getters.

### Phase 3: User management

- **UserState**: Full marshal; `send_self_mute`, `send_self_deaf`, `send_user_comment` on client.
- **UserRemove**: Marshal; `send_kick(session, reason)`, `send_ban(session, reason)`.
- **BanList**: Marshal/unmarshal; `send_ban_list_query()`.
- **QueryUsers**: Marshal/unmarshal; `send_query_users(ids, names)`.
- **UserStats**: Marshal/unmarshal; `send_user_stats(session, stats_only)`.
- **UserList**: Unmarshal and handler (receive only).

### Phase 4: Channel management

- **ChannelState**: Full marshal; `send_create_channel(parent_id, name, temporary)`, `send_edit_channel(channel_id, name, position, max_users)`, `send_channel_links(channel_id, links)`.
- **ChannelRemove**: Marshal; `send_remove_channel(channel_id)`.

### Phase 5: Permissions

- **`mumble_permissions.h`**: Permission constants (e.g. `PERMISSION_TRAVERSE`, `PERMISSION_ENTER`, `PERMISSION_SPEAK`, `PERMISSION_KICK`, `PERMISSION_BAN`, …).
- **MsgPermissionQuery**: Marshal/unmarshal; handler stores result in `permission_cache_` (channel_id → permissions); ServerSync permissions stored for root (channel 0).
- **Client API**: `send_permission_query(channel_id)`, `send_acl_query(channel_id)`, `has_permission(channel_id, permission)`.

### Phase 6: Voice targeting

- **Voice packet target**: `build_voice_packet(..., target)` in `mumble_voice.cpp`; target 0 = channel, 1–30 = voice target id, 31 = loopback.
- **Component**: `voice_target_id_`, `set_voice_target_id`, `get_voice_target_id`; `send_voice_packet` uses `voice_target_id_`.
- **Client**: `send_voice_target(id, targets)` to register a voice target with the server.

### Phase 7: Advanced protocol

- **RequestBlob**: `send_request_blob(session_texture, session_comment, channel_description)`.
- **ContextAction**: `send_context_action(session, channel_id, action)`.
- **PluginDataTransmission**: `send_plugin_data(data_id, data, receiver_sessions)`.
- SuggestConfig: receive only (handler already present).

### Phase 8: ESPHome integration

- **Config**: `bot_mode: true/false` in YAML; passed to component and client in setup.
- **Actions**: `mumble.self_mute` (mute: true/false), `mumble.self_deaf` (deaf: true/false), `mumble.kick_user` (session_id, reason).
- **Component API**: `send_self_mute`, `send_self_deaf`, `send_kick_user`; C++ action classes `MumbleSelfMuteAction`, `MumbleSelfDeafAction`, `MumbleKickUserAction`.

## Files touched

- `components/mumble/mumble_messages.h`, `mumble_messages.cpp` – new/updated message structs and marshal/unmarshal.
- `components/mumble/mumble_client.h`, `mumble_client.cpp` – handlers, callbacks, send_* methods, permission cache, bot_mode.
- `components/mumble/mumble_permissions.h` – new; permission constants.
- `components/mumble/mumble_voice.h`, `mumble_voice.cpp` – target parameter in `build_voice_packet`.
- `components/mumble/mumble_component.h`, `mumble_component.cpp` – text message callback/trigger, send_text_message, send_voice_target, voice_target_id_, send_self_mute/deaf/kick_user, bot_mode, last_text_* state.
- `components/mumble/__init__.py` – `on_text_message` trigger, `send_text_message` action, `bot_mode` config, `self_mute`, `self_deaf`, `kick_user` actions.

## Post-review fixes

- **Hardware mute pin**: `mute_pin` is now applied in `__init__.py` (`cg.add(var.set_mute_pin(pin))`). In `loop()`, when `mute_pin_` is set, its state is read (LOW = muted); on change, `send_self_mute` and `set_microphone_enabled` are updated. On boards like ESP32-S3 Box the mute circuit is hardware-isolated, so `mute_pin` is optional and not used there.
- **Jitter/playback**: `JitterBuffer::has_playout_started()` added. `OpusAudioDecoder::decode_lost()` remains available for future PLC (packet loss concealment) but is not wired into the playout loop — naively calling it on every empty `pop()` caused audio degradation and latency because `pop()` returns 0 for both prebuffering and true underruns.
- **Communicator log**: Log message corrected from "10s silence" to "2s silence" to match `COMMUNICATOR_SILENCE_MS`.
- **Docs**: Duplicate include removed; product Non-Goals updated (text messaging and ACL are in scope); technical-overview component structure and YAML table updated; all `research/` references removed from docs and code comments.

## Verification

- `esphome compile esphome/esp32-s3-box.yaml` succeeds after all changes.
