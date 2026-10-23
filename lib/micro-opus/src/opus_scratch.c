/* Static pseudostack for NONTHREADSAFE_PSEUDOSTACK.
 *
 * Encode and decode in this project run one at a time on the ESPHome loop
 * task, so one buffer is enough. It has to exist before the first encode:
 * asking the heap for it then fails on an ESP32 with no PSRAM (the Atom Echo)
 * because Wi-Fi and the Mumble TLS session have already fragmented internal
 * RAM. Opus treated that NULL as the base of the scratch buffer and wrote
 * address 0 (StoreProhibited, EXCVADDR 0) as soon as voice TX started.
 *
 * Peak use for the profile we actually run — 16 kHz mono encode at complexity
 * 0 through 5, and decode of 10/20/40/60 ms frames including 48 kHz input
 * resampled to 16 kHz — is 25504 bytes. 32 KB leaves margin. PUSH() aborts
 * if a future path needs more, instead of writing past the buffer.
 */

#include <stddef.h>

#ifndef GLOBAL_STACK_SIZE
#define GLOBAL_STACK_SIZE 32768
#endif

static unsigned char opus_scratch_mem[GLOBAL_STACK_SIZE] __attribute__((aligned(16)));

void *opus_alloc_scratch(size_t size) {
  if (size > sizeof(opus_scratch_mem))
    return NULL;
  return opus_scratch_mem;
}
