#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace mumble {

/** Run once at boot: diagnose byte-order, IP format, and sockaddr patterns. */
void mumble_diag_run_boot();

/** Log up to max_bytes of data as hex. Tag used for ESP_LOG*. */
void mumble_log_hex(const char *tag, const char *prefix, const uint8_t *data, size_t len,
                    size_t max_bytes = 32);

}  // namespace mumble
}  // namespace esphome
