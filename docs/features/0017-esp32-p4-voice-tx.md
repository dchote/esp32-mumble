# 0017 — ESP32-P4 Support and Voice Transmit Fixes

## Summary

Add the Waveshare ESP32-P4-WIFI6-POE-ETH as a first-class board, and fix voice transmit behavior shared by every board: Mumble sequence numbers, VAD cut-off during speech, push-to-talk, and a short preroll.

## Motivation

The ESP32-P4 board is Ethernet/PoE and RISC-V. It cannot use the Xtensa Opus paths or the PlatformIO linker revision this repo's other boards rely on. Separately, Mumla and desktop Mumble were dropping most of our audio because the voice sequence stepped by one 20 ms packet and restarted at 0 on every utterance. The adaptive noise floor also rose during speech and ended transmit after about a second.

## ESP32-P4

- Config: `esphome/waveshare-esp32-p4-poe.yaml`
- Board `esp32-p4_r3`, 360 MHz, native `toolchain: esp-idf` (PlatformIO's ESP-IDF 5.5.5 package does not ship `sections.rev3.ld.in`)
- Ethernet IP101 (MDC GPIO31, MDIO GPIO52, power GPIO51, clock GPIO50, PHY address 1). No Wi-Fi in this config. The default username uses `ESP_MAC_ETH`. `esp_wifi_set_ps` is compiled only when `USE_WIFI` is set.
- ES8311 analog mic. `use_microphone` stays off (that flag selects a PDM mic). **Mic Preamp** writes register `0x14` as `0x10 | (dB / 3)` (30 dB is `0x1A`, the same value ESPHome's driver uses for max PGA). **Mic Digital Gain** writes register `0x16` as `dB / 6`, matching `ES8311_MIC_GAIN_*`.
- `mic_warmup: 50ms` (component default remains 200 ms for the ES7210 on the S3 boxes)
- Header PTT on GPIO20. **Talk** enables the microphone only when mode is push-to-talk, so turning it off does not mute always-on.
- Same OTA entities as the other boards. Slug `waveshare-esp32-p4-poe`. **Ethernet IP** replaces WiFi Signal.
- `lib/micro-opus/CMakeLists.txt` builds the vendored codec as an IDF component. `OPUS_XTENSA_LX7` is set only for the ESP32-S3 (PlatformIO build flag and the CMake target).

## Voice sequence

The sequence counts 10 ms frames. A 20 ms Opus packet advances it by 2. It is not reset when an utterance starts. Mumla treats a restart at 0 as audio it has already played.

Hangover ends by sending the last 20 ms frame with the terminator bit, which is how desktop Mumble closes a stream. An abrupt stop (PTT release, or the I2S bus taken for playback) still sends an empty terminator. That packet occupies its sequence number so the next utterance cannot reuse it.

## Voice activity

- The noise floor adapts quickly only while waiting for speech. During transmit it creeps at 0.0005 per frame, so continuous speech no longer pulls the threshold up and cuts the sentence off. A floor that calibrated too low therefore recovers slowly, and communicator auto-close on steady noise takes longer.
- Hangover is 800 ms (40 frames), so gaps between words stay inside one transmission.
- Push-to-talk does not calibrate and does not run VAD. Transmit starts on the first frame.
- Always-on and communicator keep a 200 ms PCM preroll and send it when VAD triggers. If that trigger happens during echo suppression, the ring is discarded instead of being sent on a later utterance. The flush encodes those frames in the capture call (at most 10 frames, once per utterance).

## Changes

1. `docs/features/0017-esp32-p4-voice-tx.md` — this document
2. `esphome/waveshare-esp32-p4-poe.yaml` — board config
3. `components/mumble/` — `mic_warmup`, sequence, VAD, preroll, PTT
4. `lib/micro-opus/CMakeLists.txt` and `library.json` — IDF component; LX7 define only on the S3
5. CI, release workflow, installer page, `scripts/build.sh`, `scripts/flash.sh`
6. README, product overview, technical overview, build notes, feature 0008 and 0015

## Verification

- `esphome compile esphome/esp32-s3-box.yaml`
- `esphome compile esphome/waveshare-esp32-p4-poe.yaml`
- On P4 hardware: Ethernet join, always-on and push-to-talk against Murmur, listen in Mumla
- On an S3 board: the sequence, hangover, and preroll changes are shared, so a short always-on and push-to-talk check is still worth doing

## References

- [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-POE-ETH)
- Desktop sequence counter: [mumble `AudioInput.cpp`](https://github.com/mumble-voip/mumble/blob/master/src/mumble/AudioInput.cpp)
