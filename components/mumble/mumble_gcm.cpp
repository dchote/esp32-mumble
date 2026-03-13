// AES-256-GCM for Mumble secure tier. Matches go-mumble-server packet format and replay window.
#include "mumble_gcm.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace mumble {

static const char *const TAG = "mumble.gcm";

// Replay window size (match go-mumble-server)
static constexpr size_t DECRYPT_WINDOW = 64;

static void write_be64(uint8_t *out, uint64_t val) {
  out[0] = (val >> 56) & 0xff;
  out[1] = (val >> 48) & 0xff;
  out[2] = (val >> 40) & 0xff;
  out[3] = (val >> 32) & 0xff;
  out[4] = (val >> 24) & 0xff;
  out[5] = (val >> 16) & 0xff;
  out[6] = (val >> 8) & 0xff;
  out[7] = val & 0xff;
}

static uint64_t read_be64(const uint8_t *in) {
  return (static_cast<uint64_t>(in[0]) << 56) | (static_cast<uint64_t>(in[1]) << 48) |
         (static_cast<uint64_t>(in[2]) << 40) | (static_cast<uint64_t>(in[3]) << 32) |
         (static_cast<uint64_t>(in[4]) << 24) | (static_cast<uint64_t>(in[5]) << 16) |
         (static_cast<uint64_t>(in[6]) << 8) | static_cast<uint64_t>(in[7]);
}

MumbleCryptStateGcm::MumbleCryptStateGcm() {
  mbedtls_gcm_init(&ctx_);
  memset(dec_bitmap_, 0, sizeof(dec_bitmap_));
}

MumbleCryptStateGcm::~MumbleCryptStateGcm() {
  mbedtls_gcm_free(&ctx_);
}

bool MumbleCryptStateGcm::set_key(const uint8_t *key, size_t key_len, const uint8_t *client_nonce,
                                  size_t cn_len, const uint8_t *server_nonce, size_t sn_len) {
  (void) client_nonce;
  (void) cn_len;
  (void) server_nonce;
  (void) sn_len;
  if (key == nullptr || key_len != GCM_KEY_LEN) {
    return false;
  }
  int ret = mbedtls_gcm_setkey(&ctx_, MBEDTLS_CIPHER_ID_AES, key, static_cast<unsigned int>(key_len * 8));
  if (ret != 0) {
    ESP_LOGE(TAG, "GCM setkey failed: %d", ret);
    return false;
  }
  enc_counter_ = 0;
  dec_max_ = 0;
  memset(dec_bitmap_, 0, sizeof(dec_bitmap_));
  good_ = late_ = lost_ = 0;
  initialized_ = true;
  ESP_LOGI(TAG, "CryptState GCM initialized (AES-256-GCM)");
  return true;
}

bool MumbleCryptStateGcm::encrypt(const uint8_t *src, uint8_t *dst, size_t plain_len) {
  if (!initialized_) return false;

  enc_counter_++;
  uint8_t nonce[GCM_NONCE_LEN];
  memset(nonce, 0, 4);
  write_be64(nonce + 4, enc_counter_);

  uint8_t tag_buf[GCM_TAG_LEN];
  int ret = mbedtls_gcm_crypt_and_tag(&ctx_, MBEDTLS_GCM_ENCRYPT, plain_len, nonce, GCM_NONCE_LEN,
                                      nullptr, 0, src, dst + GCM_NONCE_LEN, GCM_TAG_LEN, tag_buf);
  if (ret != 0) {
    return false;
  }
  memcpy(dst, nonce, GCM_NONCE_LEN);
  memcpy(dst + GCM_NONCE_LEN + plain_len, tag_buf, GCM_TAG_LEN);
  return true;
}

bool MumbleCryptStateGcm::decrypt(const uint8_t *src, uint8_t *dst, size_t cipher_len) {
  if (!initialized_ || cipher_len < GCM_OVERHEAD) {
    return false;
  }

  const uint8_t *nonce = src;
  const uint8_t *ct = src + GCM_NONCE_LEN;
  size_t ct_len = cipher_len - GCM_NONCE_LEN - GCM_TAG_LEN;
  const uint8_t *tag = src + GCM_NONCE_LEN + ct_len;

  uint64_t counter = read_be64(nonce + 4);

  // Save state before modification so we can restore on auth failure
  uint64_t save_dec_max = dec_max_;
  uint8_t save_bitmap[8];
  memcpy(save_bitmap, dec_bitmap_, sizeof(save_bitmap));
  uint32_t save_good = good_, save_late = late_, save_lost = lost_;

  if (counter <= dec_max_) {
    if (dec_max_ - counter >= DECRYPT_WINDOW) {
      return false;  // Replay or too old
    }
    size_t idx = 63 - (dec_max_ - counter);
    size_t byte_idx = idx / 8;
    size_t bit_idx = idx % 8;
    if (dec_bitmap_[byte_idx] & (1u << bit_idx)) {
      return false;  // Duplicate
    }
    dec_bitmap_[byte_idx] |= static_cast<uint8_t>(1u << bit_idx);
    late_++;
  } else {
    uint64_t shift = counter - dec_max_;
    if (shift > DECRYPT_WINDOW) {
      memset(dec_bitmap_, 0, sizeof(dec_bitmap_));
      shift = DECRYPT_WINDOW;
    } else {
      uint64_t b = (static_cast<uint64_t>(dec_bitmap_[0]) << 56) |
                  (static_cast<uint64_t>(dec_bitmap_[1]) << 48) |
                  (static_cast<uint64_t>(dec_bitmap_[2]) << 40) |
                  (static_cast<uint64_t>(dec_bitmap_[3]) << 32) |
                  (static_cast<uint64_t>(dec_bitmap_[4]) << 24) |
                  (static_cast<uint64_t>(dec_bitmap_[5]) << 16) |
                  (static_cast<uint64_t>(dec_bitmap_[6]) << 8) |
                  static_cast<uint64_t>(dec_bitmap_[7]);
      b = (b << shift) | (1ULL << 63);
      dec_bitmap_[0] = static_cast<uint8_t>((b >> 56) & 0xff);
      dec_bitmap_[1] = static_cast<uint8_t>((b >> 48) & 0xff);
      dec_bitmap_[2] = static_cast<uint8_t>((b >> 40) & 0xff);
      dec_bitmap_[3] = static_cast<uint8_t>((b >> 32) & 0xff);
      dec_bitmap_[4] = static_cast<uint8_t>((b >> 24) & 0xff);
      dec_bitmap_[5] = static_cast<uint8_t>((b >> 16) & 0xff);
      dec_bitmap_[6] = static_cast<uint8_t>((b >> 8) & 0xff);
      dec_bitmap_[7] = static_cast<uint8_t>(b & 0xff);
    }
    dec_max_ = counter;
    if (shift > 1) {
      lost_ += static_cast<uint32_t>(shift - 1);
    }
    good_++;
  }

  int ret = mbedtls_gcm_auth_decrypt(&ctx_, ct_len, nonce, GCM_NONCE_LEN, nullptr, 0, tag,
                                    GCM_TAG_LEN, ct, dst);
  if (ret != 0) {
    // Restore replay state on auth failure to prevent forged packets from
    // poisoning the window (matching the OCB2 save/restore pattern)
    dec_max_ = save_dec_max;
    memcpy(dec_bitmap_, save_bitmap, sizeof(dec_bitmap_));
    good_ = save_good;
    late_ = save_late;
    lost_ = save_lost;
    return false;
  }
  return true;
}

}  // namespace mumble
}  // namespace esphome
