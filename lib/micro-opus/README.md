# micro-opus (local copy)

Local vendored copy of the Opus audio codec for ESP32, based on [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus) v0.3.4.

## Why local?

- **No submodules** — All sources are copied and patched in-place
- **Heap pseudostack** — Uses `NONTHREADSAFE_PSEUDOSTACK` instead of `USE_ALLOCA`, so Opus working memory (~120KB) comes from heap/PSRAM, not the task stack. Avoids loopTask stack overflow during voice decode.
- **PSRAM-aware** — Uses `heap_caps_malloc_prefer` to allocate from PSRAM when available
- **Xtensa optimizations** — Pre-applied Xtensa LX7 patches for faster decode on ESP32-S3

## Structure

- `src/opus_src/`, `src/celt/`, `src/silk/` — Patched Opus sources
- `src/include/` — Public headers (`opus.h`, etc.)
- `src/custom_support.h` — PSRAM-aware allocation overrides
- `src/config.h` — Build configuration
- `library.json` — PlatformIO library manifest

## Updating

To refresh from upstream: clone [esphome-libs/micro-opus](https://github.com/esphome-libs/micro-opus), apply patches (see `cmake/staging.cmake`), copy the staged opus tree and support files into this directory. Update `library.json` if source paths change.
