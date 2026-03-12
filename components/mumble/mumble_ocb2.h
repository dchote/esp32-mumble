// OCB2-AES128 for Mumble legacy voice encryption.
// Ported from the official Mumble CryptStateOCB2 (BSD license) to use mbedTLS.
// https://www.cs.ucdavis.edu/~rogaway/ocb/license.htm

#pragma once

#include <cstddef>
#include <cstdint>
#include <mbedtls/aes.h>

namespace esphome {
namespace mumble {

static constexpr size_t AES_BLOCK = 16;
static constexpr size_t AES_KEY_LEN = 16;
static constexpr size_t OCB2_TAG_LEN = 3;
static constexpr size_t OCB2_OVERHEAD = 1 + OCB2_TAG_LEN;  // nonce byte + truncated tag

class MumbleCryptState {
 public:
  MumbleCryptState();
  ~MumbleCryptState();

  bool set_key(const uint8_t *key, size_t key_len,
               const uint8_t *client_nonce, size_t cn_len,
               const uint8_t *server_nonce, size_t sn_len);

  bool set_decrypt_iv(const uint8_t *iv, size_t len);
  const uint8_t *get_encrypt_iv() const { return encrypt_iv_; }

  // Encrypt plaintext into dst. dst must have room for plain_len + OCB2_OVERHEAD bytes.
  bool encrypt(const uint8_t *src, uint8_t *dst, size_t plain_len);

  // Decrypt ciphertext into dst. Returns false on auth failure or invalid packet.
  bool decrypt(const uint8_t *src, uint8_t *dst, size_t cipher_len);

  bool is_valid() const { return initialized_; }
  size_t overhead() const { return OCB2_OVERHEAD; }

  uint32_t good() const { return good_; }
  uint32_t late() const { return late_; }
  uint32_t lost() const { return lost_; }

 private:
  bool ocb_encrypt(const uint8_t *plain, uint8_t *encrypted, unsigned int len,
                   const uint8_t *nonce, uint8_t *tag);
  bool ocb_decrypt(const uint8_t *encrypted, uint8_t *plain, unsigned int len,
                   const uint8_t *nonce, uint8_t *tag);

  void aes_encrypt_block(const void *src, void *dst);
  void aes_decrypt_block(const void *src, void *dst);

  mbedtls_aes_context aes_enc_;
  mbedtls_aes_context aes_dec_;
  uint8_t raw_key_[AES_KEY_LEN];
  uint8_t encrypt_iv_[AES_BLOCK];
  uint8_t decrypt_iv_[AES_BLOCK];
  uint8_t decrypt_history_[256];
  bool initialized_{false};

  uint32_t good_{0};
  uint32_t late_{0};
  uint32_t lost_{0};
};

}  // namespace mumble
}  // namespace esphome
