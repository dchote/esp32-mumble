# 0018 — Atom Echo panic on voice transmit

## Summary

An M5Stack Atom Echo joined the server, opened the microphone, logged `Voice TX started`, and then rebooted with `StoreProhibited` at address 0. The first Opus encode asked the heap for a 120 KB scratch buffer. The Atom Echo is an original ESP32 with no PSRAM, and by the time someone speaks that allocation fails. Opus used the NULL pointer as the base of its scratch buffer.

## Report

The control connection and encrypted UDP ping succeeded. The panic is on the first voice encode, not in the microphone or the crypto path:

```
[I][mumble.udp:140]: UDP confirmed via ping echo
[D][mumble:184]: Microphone enabled
[D][mumble:791]: I2S bus -> mic
[I][mumble:1054]: Voice TX started
Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled.
EXCVADDR: 0x00000000
A2: 0x00000000  A3: 0x3ffdb740  A4: 0x00000080
```

`EXCVADDR` 0 with `A2` 0 is a write through a NULL destination. `PC` `0x4000c2e4` is the ESP32 ROM `memcpy`. There is no `Voice pkt` log, which is printed only after `opus_encode` returns and the packet is sent. `Voice TX started` is the line immediately before `flush_preroll()` / `encode_and_send()`.

The three `CryptState initialized` lines before the self-test are the live OCB2 state plus the two temporary states inside `mumble_ocb2_selftest`. The UDP ping echo after that shows encryption is working.

## Cause

micro-opus is built with `NONTHREADSAFE_PSEUDOSTACK` and `GLOBAL_STACK_SIZE=120000`. The first `opus_encode` or `opus_decode` calls `opus_alloc_scratch(120000)`. That used to prefer PSRAM and fall back to internal RAM. The Atom Echo has no PSRAM. Internal RAM is already fragmented by Wi-Fi and the Mumble TLS session, so the 120 KB block is not available and the call returns NULL.

`ALLOC_STACK` stored that NULL in `global_stack`. The next scratch allocation is placed at address 0, and the first copy into it is the `memcpy` in the panic.

This is not a bad microphone and not specific to one Atom Echo. Any board without a contiguous 120 KB free block hits it on the first encode. An ESP32-S3 with PSRAM often does not, because the same call succeeds in PSRAM. A second Atom Echo will panic the same way until it runs a build that contains this fix.

## Fix

The scratch buffer is now a static 32 KB block (`lib/micro-opus/src/opus_scratch.c`). It does not depend on the heap at encode time.

32 KB is enough for the codec profile this firmware uses. A host build of the same fixed-point sources, tracking the pseudostack high-water mark, peaked at:

- 20064 bytes for 16 kHz mono encode (complexity 0 through 5), including DTX and packet-loss concealment
- 25504 bytes when a 16 kHz decoder decoded 40 ms and 60 ms frames that were encoded at 48 kHz (what desktop Mumble sends)

`GLOBAL_STACK_SIZE` is 32768 in `library.json`, `CMakeLists.txt`, and the `arch.h` default. `PUSH` aborts with `pseudostack overflow` if a later change needs more than that, instead of writing past the buffer. `ALLOC_STACK` aborts if the scratch pointer is still NULL, instead of writing to address 0.

Encoder and decoder state are unchanged: they still prefer PSRAM and fall back to internal RAM. Those allocations already succeed on the Atom Echo (otherwise `Voice TX started` would never be logged, because capture returns early when the encoder failed to init).

## Verification

- Host high-water measurement above, same `FIXED_POINT` / `DISABLE_FLOAT_API` sources the firmware compiles.
- `esphome compile esphome/m5stack-atom-echo.yaml` succeeded (ESPHome 2026.9.1, Arduino-ESP32 3.3.11). Image RAM use is 101744 of 327680 bytes (31%). The 32 KB scratch buffer is in `.bss` (85488 bytes total) and the link still has heap left.
- `esphome compile esphome/esp32-s3-box.yaml` succeeded with the same scratch object.

## What to ship

The Atom Echo needs a firmware build that contains this change. A second board, or a generic ESP32-S3 with PSRAM, does not tell you whether this panic is fixed: the S3 can satisfy the old 120 KB request from PSRAM and never take the NULL path.

## References

- `lib/micro-opus/src/opus_scratch.c`
- `lib/micro-opus/src/celt/stack_alloc.h` — `ALLOC_STACK` / `PUSH`
- `components/mumble/mumble_component.cpp` — `Voice TX started` then `encode_and_send()`
