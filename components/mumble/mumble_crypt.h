// Abstract base for Mumble UDP crypto (Legacy OCB2 and Secure GCM).
#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace mumble {

class MumbleCryptStateBase {
 public:
  virtual ~MumbleCryptStateBase() = default;

  virtual bool set_key(const uint8_t *key, size_t key_len, const uint8_t *client_nonce, size_t cn_len,
                       const uint8_t *server_nonce, size_t sn_len) = 0;
  virtual bool set_decrypt_iv(const uint8_t *iv, size_t len) { return true; }
  virtual const uint8_t *get_encrypt_iv() const { return nullptr; }

  virtual bool encrypt(const uint8_t *src, uint8_t *dst, size_t plain_len) = 0;
  virtual bool decrypt(const uint8_t *src, uint8_t *dst, size_t cipher_len) = 0;

  virtual bool is_valid() const = 0;
  virtual size_t overhead() const = 0;

  virtual uint32_t good() const { return 0; }
  virtual uint32_t late() const { return 0; }
  virtual uint32_t lost() const { return 0; }
  virtual uint32_t resync() const { return 0; }
};

}  // namespace mumble
}  // namespace esphome
