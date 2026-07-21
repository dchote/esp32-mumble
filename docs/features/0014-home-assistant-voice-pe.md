# 0014 — Home Assistant Voice PE Support

## Summary

Add Home Assistant Voice Preview Edition as a first-class ESPHome board target for ESP32-Mumble: Mumble-focused YAML with XMOS/AIC3204 audio, jog-wheel volume, mute slider, and center-button PTT/Communicator.

## Motivation

Voice PE is a widely available ESP32-S3 appliance with a rotary dial, hardware mute slider, center button, dual-mic XMOS DSP, AIC3204 DAC, and LED ring. Supporting it expands the project's hardware matrix beyond Box/Box-3 and Atom Echo without porting the full Home Assistant Voice Assistant stack.

## Approach

- **Mumble-only** config (no wake word, VA, media_player, or Sendspin)
- **ESP-IDF** framework (lwIP netconn UDP), aligned with official Voice PE and Box
- Vendor the small `voice_kit` component locally for reproducible CI
- Wire physical controls to existing Mumble/HA entities

## Hardware Controls

| Control | Pins | Behavior |
|---------|------|----------|
| Center button | GPIO0 (inverted) | Connected + unmuted: communicator mode → `mumble.communicator_toggle`; else PTT enable/disable |
| Mute slider | GPIO3 | HW mute → `microphone.mute` + `microphone_disable`; unmute restores TX (respects always_on); overrides HA Mute |
| Jog wheel | GPIO16 / GPIO18 | Volume ±5 on `mumble_speaker_volume` (0–100) |
| Speaker amp | GPIO47 | Speaker Power switch |
| LED ring | GPIO21 data, GPIO45 power | Mumble status (connecting / connected / talking / muted / error) |
| Jack detect | GPIO17 | Sensor only in v1 |

## Audio

- I2C GPIO5/6; XMOS `voice_kit` reset GPIO4 @ 0x42 (firmware v1.3.1 DFU)
- Mic: I2S secondary DIN15 / BCLK13 / LRCLK14, 16 kHz, mono left, 32-bit
- Speaker: I2S secondary DOUT10 / BCLK8 / LRCLK7 + AIC3204 at **48 kHz**; `resampler` speaker accepts Mumble/chime 16 kHz mono (`EsphomeSpeakerSink` sets stream info)

## Changes

1. `docs/features/0014-home-assistant-voice-pe.md` — this document
2. `components/voice_kit/` — vendored from esphome/home-assistant-voice-pe
3. `esphome/home-assistant-voice-pe.yaml` — board config
4. `scripts/build.sh`, `.github/workflows/build.yml`, cursor rules — Voice PE target
5. README, product/technical overview, build.md — supported hardware docs

## Out of Scope (v1)

- Wake word / Voice Assistant / media_player / Sendspin
- Dial-held LED hue control
- Factory-reset long-press / Improv BLE
- Full-duplex AEC beyond existing echo suppression

## Verification

- `esphome compile esphome/home-assistant-voice-pe.yaml`
- On hardware: dial volume, mute slider, center PTT/Communicator, LED status

## References

- Research clone: `research/home-assistant-voice-pe` ([esphome/home-assistant-voice-pe](https://github.com/esphome/home-assistant-voice-pe))
- Plan: Home Assistant Voice PE Support
