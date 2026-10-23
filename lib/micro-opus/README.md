# micro-opus (local copy)

Local vendored copy of the Opus audio codec for ESP32, based on [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus) v0.4.1.

## Why local?

- **No submodules** — All sources are copied and patched in-place
- **Static pseudostack** — Uses `NONTHREADSAFE_PSEUDOSTACK` instead of `USE_ALLOCA`, so Opus working memory is a 32 KB static buffer, not the task stack and not a 120 KB heap block. 16 kHz mono encode/decode peaks at about 25 KB. The old 120 KB heap request failed on the Atom Echo (no PSRAM) once Wi-Fi and TLS were up, and the first voice encode wrote through the resulting NULL.
- **PSRAM-aware encoder state** — Opus encoder/decoder state still prefers PSRAM when the board has it, and falls back to internal RAM (Atom Echo).
- **Xtensa optimizations** — Pre-applied Xtensa LX7 patches for faster decode on ESP32-S3

## Structure

- `src/opus_src/`, `src/celt/`, `src/silk/` — Patched Opus sources
- `src/include/` — Public headers (`opus.h`, etc.)
- `src/custom_support.h` — PSRAM-aware allocation overrides
- `src/config.h` — Build configuration
- `library.json` — PlatformIO library manifest

## Updating

To refresh from upstream: clone [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus), apply patches (see `cmake/staging.cmake`), copy the staged opus tree and support files into this directory. Update `library.json` if source paths change.
