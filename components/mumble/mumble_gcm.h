// AES-256-GCM for Mumble secure voice encryption (go-mumble-server secure tier).
// Packet format: 12-byte nonce || ciphertext || 16-byte tag. Nonce = 4 zero + 8-byte big-endian counter.
#pragma once

#include "mumble_crypt.h"
#include <cstddef>
#include <cstdint>
#include <mbedtls/gcm.h>

namespace esphome {
namespace mumble {

static constexpr size_t GCM_KEY_LEN = 32;
static constexpr size_t GCM_NONCE_LEN = 12;
static constexpr size_t GCM_TAG_LEN = 16;
static constexpr size_t GCM_OVERHEAD = GCM_NONCE_LEN + GCM_TAG_LEN;  // 28 bytes

class MumbleCryptStateGcm : public MumbleCryptStateBase {
 public:
  MumbleCryptStateGcm();
  ~MumbleCryptStateGcm();

  bool set_key(const uint8_t *key, size_t key_len, const uint8_t *client_nonce, size_t cn_len,
               const uint8_t *server_nonce, size_t sn_len) override;

  bool encrypt(const uint8_t *src, uint8_t *dst, size_t plain_len) override;
  bool decrypt(const uint8_t *src, uint8_t *dst, size_t cipher_len) override;

  bool is_valid() const override { return initialized_; }
  size_t overhead() const override { return GCM_OVERHEAD; }

  uint32_t good() const override { return good_; }
  uint32_t late() const override { return late_; }
  uint32_t lost() const override { return lost_; }

 private:
  mbedtls_gcm_context ctx_;
  bool initialized_{false};
  uint64_t enc_counter_{0};
  uint64_t dec_max_{0};
  uint8_t dec_bitmap_[8];
  uint32_t good_{0};
  uint32_t late_{0};
  uint32_t lost_{0};
};

}  // namespace mumble
}  // namespace esphome
