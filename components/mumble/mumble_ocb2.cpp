// OCB2-AES128 for Mumble legacy voice encryption.
// Ported from mumble-voip/grumble (BSD license) to use mbedTLS.
// Byte layout must match grumble: block[0]=MSB, poly 0x87.
// https://eprint.iacr.org/2019/311 (XEX* countermeasure)

#include "mumble_ocb2.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.ocb2";

// Byte-oriented GF(2^128) ops matching grumble exactly (block[0]=MSB).
static inline void xor_block(uint8_t *dst, const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < 16; i++)
    dst[i] = a[i] ^ b[i];
}

static inline void times2(uint8_t *block) {
  uint8_t carry = (block[0] >> 7) & 0x01;
  for (int i = 0; i < 15; i++)
    block[i] = (block[i] << 1) | ((block[i + 1] >> 7) & 0x01);
  block[15] = (block[15] << 1) ^ (carry * 135);
}

static inline void times3(uint8_t *block) {
  uint8_t carry = (block[0] >> 7) & 0x01;
  for (int i = 0; i < 15; i++)
    block[i] ^= (block[i] << 1) | ((block[i + 1] >> 7) & 0x01);
  block[15] ^= (block[15] << 1) ^ (carry * 135);
}

MumbleCryptState::MumbleCryptState() {
  mbedtls_aes_init(&aes_enc_);
  mbedtls_aes_init(&aes_dec_);
  memset(raw_key_, 0, sizeof(raw_key_));
  memset(encrypt_iv_, 0, sizeof(encrypt_iv_));
  memset(decrypt_iv_, 0, sizeof(decrypt_iv_));
  memset(decrypt_history_, 0, sizeof(decrypt_history_));
}

MumbleCryptState::~MumbleCryptState() {
  mbedtls_aes_free(&aes_enc_);
  mbedtls_aes_free(&aes_dec_);
}

bool MumbleCryptState::set_key(const uint8_t *key, size_t key_len,
                               const uint8_t *client_nonce, size_t cn_len,
                               const uint8_t *server_nonce, size_t sn_len) {
  if (key_len != AES_KEY_LEN || cn_len != AES_BLOCK || sn_len != AES_BLOCK)
    return false;

  memcpy(raw_key_, key, AES_KEY_LEN);
  memcpy(encrypt_iv_, client_nonce, AES_BLOCK);
  memcpy(decrypt_iv_, server_nonce, AES_BLOCK);
  memset(decrypt_history_, 0, sizeof(decrypt_history_));

  mbedtls_aes_setkey_enc(&aes_enc_, raw_key_, 128);
  mbedtls_aes_setkey_dec(&aes_dec_, raw_key_, 128);

  good_ = late_ = lost_ = resync_ = 0;
  initialized_ = true;
  ESP_LOGI(TAG, "CryptState initialized (OCB2-AES128)");
  return true;
}

bool MumbleCryptState::set_decrypt_iv(const uint8_t *iv, size_t len) {
  if (len != AES_BLOCK) return false;
  memcpy(decrypt_iv_, iv, AES_BLOCK);
  return true;
}

void MumbleCryptState::aes_encrypt_block(const void *src, void *dst) {
  mbedtls_aes_crypt_ecb(&aes_enc_, MBEDTLS_AES_ENCRYPT,
                         reinterpret_cast<const unsigned char *>(src),
                         reinterpret_cast<unsigned char *>(dst));
}

void MumbleCryptState::aes_decrypt_block(const void *src, void *dst) {
  mbedtls_aes_crypt_ecb(&aes_dec_, MBEDTLS_AES_DECRYPT,
                         reinterpret_cast<const unsigned char *>(src),
                         reinterpret_cast<unsigned char *>(dst));
}

bool MumbleCryptState::encrypt(const uint8_t *source, uint8_t *dst, size_t plain_length) {
  if (!initialized_) return false;

  uint8_t tag[AES_BLOCK];

  // Increment encrypt_iv (little-endian counter)
  for (int i = 0; i < (int) AES_BLOCK; i++)
    if (++encrypt_iv_[i])
      break;

  if (!ocb_encrypt(source, dst + 4, static_cast<unsigned int>(plain_length), encrypt_iv_, tag))
    return false;

  dst[0] = encrypt_iv_[0];
  dst[1] = tag[0];
  dst[2] = tag[1];
  dst[3] = tag[2];
  return true;
}

bool MumbleCryptState::decrypt(const uint8_t *source, uint8_t *dst, size_t cipher_length) {
  if (!initialized_) return false;
  if (cipher_length < OCB2_OVERHEAD) return false;

  unsigned int plain_length = static_cast<unsigned int>(cipher_length - OCB2_OVERHEAD);

  uint8_t saveiv[AES_BLOCK];
  uint8_t ivbyte = source[0];
  bool restore = false;
  uint8_t tag[AES_BLOCK];

  int lost = 0;
  int late = 0;

  memcpy(saveiv, decrypt_iv_, AES_BLOCK);

  if (((decrypt_iv_[0] + 1) & 0xFF) == ivbyte) {
    // In order as expected
    if (ivbyte > decrypt_iv_[0]) {
      decrypt_iv_[0] = ivbyte;
    } else if (ivbyte < decrypt_iv_[0]) {
      // Wraparound (e.g. 0xFF -> 0x00)
      decrypt_iv_[0] = ivbyte;
      for (int i = 1; i < (int) AES_BLOCK; i++)
        if (++decrypt_iv_[i])
          break;
    } else {
      return false;
    }
  } else {
    int diff = ivbyte - decrypt_iv_[0];
    if (diff > 128)
      diff -= 256;
    else if (diff < -128)
      diff += 256;

    if ((ivbyte < decrypt_iv_[0]) && (diff > -30) && (diff < 0)) {
      // Late packet, no wraparound
      late = 1;
      lost = -1;
      decrypt_iv_[0] = ivbyte;
      restore = true;
    } else if ((ivbyte > decrypt_iv_[0]) && (diff > -30) && (diff < 0)) {
      // Late packet with wraparound (e.g. last was 0x02, got 0xFF from previous round)
      late = 1;
      lost = -1;
      decrypt_iv_[0] = ivbyte;
      for (int i = 1; i < (int) AES_BLOCK; i++)
        if (decrypt_iv_[i]--)
          break;
      restore = true;
    } else if ((ivbyte > decrypt_iv_[0]) && (diff > 0)) {
      // Lost packets, no wraparound
      lost = ivbyte - decrypt_iv_[0] - 1;
      decrypt_iv_[0] = ivbyte;
    } else if ((ivbyte < decrypt_iv_[0]) && (diff > 0)) {
      // Lost packets with wraparound
      lost = 256 - decrypt_iv_[0] + ivbyte - 1;
      decrypt_iv_[0] = ivbyte;
      for (int i = 1; i < (int) AES_BLOCK; i++)
        if (++decrypt_iv_[i])
          break;
    } else {
      return false;
    }

    // Replay detection
    if (decrypt_history_[decrypt_iv_[0]] == decrypt_iv_[1]) {
      memcpy(decrypt_iv_, saveiv, AES_BLOCK);
      return false;
    }
  }

  bool ocb_success = ocb_decrypt(source + 4, dst, plain_length, decrypt_iv_, tag);

  if (!ocb_success || memcmp(tag, source + 1, OCB2_TAG_LEN) != 0) {
    memcpy(decrypt_iv_, saveiv, AES_BLOCK);
    return false;
  }
  decrypt_history_[decrypt_iv_[0]] = decrypt_iv_[1];

  if (restore)
    memcpy(decrypt_iv_, saveiv, AES_BLOCK);

  good_++;
  if (late > 0)
    late_ += static_cast<uint32_t>(late);
  if (lost > 0)
    lost_ += static_cast<uint32_t>(lost);

  return true;
}

bool MumbleCryptState::ocb_encrypt(const uint8_t *plain, uint8_t *encrypted, unsigned int len,
                                   const uint8_t *nonce, uint8_t *tag) {
  uint8_t checksum[16], delta[16], tmp[16], pad[16];
  memset(checksum, 0, 16);

  aes_encrypt_block(nonce, delta);

  while (len > AES_BLOCK) {
    // XEX* attack countermeasure (section 9, https://eprint.iacr.org/2019/311)
    bool flip_a_bit = false;
    if (len - AES_BLOCK <= AES_BLOCK) {
      uint8_t sum = 0;
      for (int i = 0; i < (int) AES_BLOCK - 1; ++i)
        sum |= plain[i];
      if (sum == 0)
        flip_a_bit = true;
    }

    times2(delta);
    xor_block(tmp, delta, plain);
    if (flip_a_bit)
      tmp[0] ^= 1;
    aes_encrypt_block(tmp, tmp);
    xor_block(encrypted, delta, tmp);
    for (int i = 0; i < 16; i++)
      checksum[i] ^= plain[i];
    if (flip_a_bit)
      checksum[0] ^= 1;

    len -= AES_BLOCK;
    plain += AES_BLOCK;
    encrypted += AES_BLOCK;
  }

  // Final (possibly partial) block - length at bytes 14-15, big-endian (per grumble)
  times2(delta);
  memset(tmp, 0, 16);
  uint32_t num = len * 8;
  tmp[14] = (num >> 8) & 0xff;
  tmp[15] = num & 0xff;
  xor_block(tmp, tmp, delta);
  aes_encrypt_block(tmp, pad);
  memcpy(tmp, plain, len);
  memcpy(tmp + len, pad + len, AES_BLOCK - len);
  for (int i = 0; i < 16; i++)
    checksum[i] ^= tmp[i];
  for (int i = 0; i < (int) len; i++)
    tmp[i] = pad[i] ^ tmp[i];
  memcpy(encrypted, tmp, len);

  // Tag
  times3(delta);
  xor_block(tmp, delta, checksum);
  aes_encrypt_block(tmp, tag);

  return true;
}

bool MumbleCryptState::ocb_decrypt(const uint8_t *encrypted, uint8_t *plain, unsigned int len,
                                   const uint8_t *nonce, uint8_t *tag) {
  uint8_t checksum[16], delta[16], tmp[16], pad[16];
  memset(checksum, 0, 16);

  aes_encrypt_block(nonce, delta);

  while (len > AES_BLOCK) {
    times2(delta);
    xor_block(tmp, delta, encrypted);
    aes_decrypt_block(tmp, tmp);
    xor_block(plain, delta, tmp);
    for (int i = 0; i < 16; i++)
      checksum[i] ^= plain[i];
    len -= AES_BLOCK;
    plain += AES_BLOCK;
    encrypted += AES_BLOCK;
  }

  // Final (possibly partial) block - length at bytes 14-15, big-endian (per grumble)
  times2(delta);
  memset(tmp, 0, 16);
  uint32_t num = len * 8;
  tmp[14] = (num >> 8) & 0xff;
  tmp[15] = num & 0xff;
  xor_block(tmp, tmp, delta);
  aes_encrypt_block(tmp, pad);
  memset(tmp, 0, 16);
  memcpy(tmp, encrypted, len);
  xor_block(tmp, tmp, pad);
  xor_block(checksum, checksum, tmp);
  memcpy(plain, tmp, len);

  // XEX* attack countermeasure: detect forged last block
  bool success = (memcmp(tmp, delta, 15) != 0);

  // Tag
  times3(delta);
  xor_block(tmp, delta, checksum);
  aes_encrypt_block(tmp, tag);

  return success;
}

}  // namespace mumble
}  // namespace esphome
