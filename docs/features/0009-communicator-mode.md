# 0009 — Star Trek Communicator Mode

## Summary

Communicator mode provides half-duplex, walkie-talkie-style communication with audible open/close chimes. A single button press opens a session; speaking is transmitted via Mumble with VAD; 2 seconds of silence auto-closes the session. A second press cancels early.

## State Machine

```
IDLE ──(button press)──► OPEN_CHIME ──(chime done)──► MIC_ACTIVE
                                                          │
                                              voice_sending_ = true
                                                          │
                                              voice_sending_ = false
                                                          ▼
                                                   SILENCE_WINDOW
                                                     │         │
                                            voice resumes   2s timeout
                                                │              │
                                           MIC_ACTIVE    CLOSE_CHIME ──► IDLE
                                                              ▲
                                              (button press during session)
```

### States

| State | Bus Owner | Behavior |
|-------|-----------|----------|
| `IDLE` | MIC (if connected) | No communicator activity |
| `OPEN_CHIME` | SPEAKER | Playing embedded open chime; no mic, no RX |
| `MIC_ACTIVE` | MIC | Microphone live, VAD + Opus TX active; RX suppressed |
| `SILENCE_WINDOW` | MIC | Mic stays live; 2s countdown resets if TX resumes |
| `CLOSE_CHIME` | SPEAKER | Playing embedded close chime; then transitions to IDLE |

## Chime Playback

The chime is embedded directly in the firmware as a `static const uint8_t[]` array generated from `esphome/sounds/communicator.wav` (16-bit mono PCM, 16 kHz).

### Generation

```bash
python scripts/generate_communicator_chime.py
```

This reads the WAV file, extracts raw PCM data, and writes `components/mumble/communicator_chime_data.h`. The generated header is committed to the repo — no build-time script execution needed.

### Playback Details

- Writes one 640-byte chunk (20 ms of audio) per loop iteration to the speaker ring buffer
- Respects `speaker->play()` return value — only advances by bytes actually accepted
- Volume is scaled by `CHIME_VOLUME_SCALE` (0.25) before writing to avoid clipping
- Uses `speaker->finish()` (graceful drain) not `stop()` (hard kill) after all data is written
- Waits for `speaker->is_stopped()` before transitioning state

## I2S Bus Management

The communicator integrates with `manage_i2s_bus()` for half-duplex I2S arbitration:

- `chime_playing_` takes highest bus priority (SPEAKER)
- TX intent (`should_transmit()`) takes priority over RX playback
- Stale bus detection: if bus is SPEAKER but speaker has stopped, bus resets to NONE for re-acquisition
- During `MIC_ACTIVE` and `SILENCE_WINDOW`, incoming voice packets are suppressed (`on_voice_packet` returns early)

## Actions

| Action | Description |
|--------|-------------|
| `mumble.communicator_toggle` | Start session (if idle) or cancel (if active) |
| `mumble.start_communicator_transmit` | Skip chime, go directly to MIC_ACTIVE |
| `mumble.play_communicator_chime_then_transmit` | Play open chime, then MIC_ACTIVE |

## Mode Switching

Mode changes are detected every loop iteration via `last_mode_` tracking. Switching away from communicator cancels any active session. Switching to/from always-on adjusts `microphone_enabled_` accordingly.

## YAML Integration

All board configs (Box, Box-3, Atom Echo) use the same button logic:

```yaml
on_press:
  - if:
      condition:
        - binary_sensor.is_on: mumble_connected_sensor
      then:
        - if:
            condition:
              - lambda: 'return id(mumble_client).get_mode() == 2;'
            then:
              - mumble.communicator_toggle: mumble_client
            else:
              - mumble.microphone_enable: mumble_client
on_release:
  - if:
      condition:
        - lambda: 'return id(mumble_client).get_mode() != 2;'
      then:
        - mumble.microphone_disable: mumble_client
```

## Files Changed

| Area | Files |
|------|-------|
| Component | `components/mumble/__init__.py`, `mumble_component.h`, `mumble_component.cpp`, `mumble_audio.h`, `mumble_audio.cpp` |
| Chime data | `components/mumble/communicator_chime_data.h` (generated), `esphome/sounds/communicator.wav` |
| Generator | `scripts/generate_communicator_chime.py` |
| Board configs | `esp32-s3-box.yaml`, `esp32-s3-box3.yaml`, `m5stack-atom-echo.yaml` |
| All configs | Mode option `communicator` in all YAML files |
