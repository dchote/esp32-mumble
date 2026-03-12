#include "mumble_messages.h"
#include <cstring>

namespace esphome {
namespace mumble {

// Float32 bit cast helpers (IEEE 754)
static uint32_t float_to_bits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}
static float bits_to_float(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

// MsgVersion
void MsgVersion::marshal(std::vector<uint8_t> &out) const {
  if (version_v1 != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, version_v1);
  }
  if (!release.empty()) proto_append_string(out, 2, release);
  if (!os.empty()) proto_append_string(out, 3, os);
  if (!os_version.empty()) proto_append_string(out, 4, os_version);
  if (version_v2 != 0) {
    proto_append_tag(out, 5, WIRE_VARINT);
    proto_append_varint(out, version_v2);
  }
  if (crypto_modes != 0) {
    proto_append_tag(out, 6, WIRE_VARINT);
    proto_append_varint(out, crypto_modes);
  }
}

bool MsgVersion::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        version_v1 = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        release.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        os.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        os_version.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        version_v2 = v;
        p += n;
        rem -= n;
        break;
      }
      case 6: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        crypto_modes = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgAuthenticate
void MsgAuthenticate::marshal(std::vector<uint8_t> &out) const {
  if (!username.empty()) proto_append_string(out, 1, username);
  if (!password.empty()) proto_append_string(out, 2, password);
  if (opus) proto_append_bool(out, 5, true);
  if (client_type != 0) {
    proto_append_tag(out, 6, WIRE_VARINT);
    proto_append_varint(out, static_cast<uint64_t>(static_cast<int32_t>(client_type)));
  }
}

// MsgCryptSetup
void MsgCryptSetup::marshal(std::vector<uint8_t> &out) const {
  out.clear();
  if (!key.empty())
    proto_append_bytes(out, 1, key.data(), key.size());
  if (!client_nonce.empty())
    proto_append_bytes(out, 2, client_nonce.data(), client_nonce.size());
  if (!server_nonce.empty())
    proto_append_bytes(out, 3, server_nonce.data(), server_nonce.size());
}

bool MsgCryptSetup::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        const uint8_t *d;
        size_t dl;
        n = proto_read_length_delimited(p, rem, &d, &dl);
        if (n == 0) return false;
        key.assign(d, d + dl);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *d;
        size_t dl;
        n = proto_read_length_delimited(p, rem, &d, &dl);
        if (n == 0) return false;
        client_nonce.assign(d, d + dl);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *d;
        size_t dl;
        n = proto_read_length_delimited(p, rem, &d, &dl);
        if (n == 0) return false;
        server_nonce.assign(d, d + dl);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgServerSync
bool MsgServerSync::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        max_bandwidth = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        welcome_text.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        permissions = v;
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgChannelState
bool MsgChannelState::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        parent = static_cast<uint32_t>(v);
        has_parent = true;
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        name.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        links.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 8: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        temporary = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 9: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        position = static_cast<int32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 11: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        max_users = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgChannelRemove
bool MsgChannelRemove::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgUserState
bool MsgUserState::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        actor = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        name.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        user_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 6: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        mute = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 7: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        deaf = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 9: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        self_mute = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 10: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        self_deaf = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

void MsgUserState::marshal_channel_change(std::vector<uint8_t> &out, uint32_t session_id,
                                          uint32_t channel_id) {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, session_id);
  proto_append_tag(out, 5, WIRE_VARINT);
  proto_append_varint(out, channel_id);
}

// MsgUserRemove
bool MsgUserRemove::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        reason.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgPing
void MsgPing::marshal(std::vector<uint8_t> &out) const {
  if (timestamp != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, timestamp);
  }
  if (good != 0) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, good);
  }
  if (late != 0) {
    proto_append_tag(out, 3, WIRE_VARINT);
    proto_append_varint(out, late);
  }
  if (lost != 0) {
    proto_append_tag(out, 4, WIRE_VARINT);
    proto_append_varint(out, lost);
  }
  if (resync != 0) {
    proto_append_tag(out, 5, WIRE_VARINT);
    proto_append_varint(out, resync);
  }
  if (udp_packets != 0) {
    proto_append_tag(out, 6, WIRE_VARINT);
    proto_append_varint(out, udp_packets);
  }
  if (tcp_packets != 0) {
    proto_append_tag(out, 7, WIRE_VARINT);
    proto_append_varint(out, tcp_packets);
  }
  if (udp_ping_avg != 0) {
    proto_append_tag(out, 8, WIRE_FIXED32);
    proto_append_fixed32(out, float_to_bits(udp_ping_avg));
  }
  if (udp_ping_var != 0) {
    proto_append_tag(out, 9, WIRE_FIXED32);
    proto_append_fixed32(out, float_to_bits(udp_ping_var));
  }
  if (tcp_ping_avg != 0) {
    proto_append_tag(out, 10, WIRE_FIXED32);
    proto_append_fixed32(out, float_to_bits(tcp_ping_avg));
  }
  if (tcp_ping_var != 0) {
    proto_append_tag(out, 11, WIRE_FIXED32);
    proto_append_fixed32(out, float_to_bits(tcp_ping_var));
  }
}

bool MsgPing::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        timestamp = v;
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        good = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        late = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        lost = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        resync = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 6: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        udp_packets = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 7: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        tcp_packets = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 8:
        if (rem >= 4) {
          udp_ping_avg = bits_to_float(proto_read_fixed32(p));
          p += 4;
          rem -= 4;
        }
        break;
      case 9:
        if (rem >= 4) {
          udp_ping_var = bits_to_float(proto_read_fixed32(p));
          p += 4;
          rem -= 4;
        }
        break;
      case 10:
        if (rem >= 4) {
          tcp_ping_avg = bits_to_float(proto_read_fixed32(p));
          p += 4;
          rem -= 4;
        }
        break;
      case 11:
        if (rem >= 4) {
          tcp_ping_var = bits_to_float(proto_read_fixed32(p));
          p += 4;
          rem -= 4;
        }
        break;
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgReject
bool MsgReject::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        type = static_cast<RejectType>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        reason.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgCodecVersion
bool MsgCodecVersion::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        opus = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

// MsgServerConfig
bool MsgServerConfig::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    switch (fn) {
      case 1: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        max_bandwidth = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        welcome_text.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        max_users = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      default:
        n = proto_skip_field(p, rem, wt);
        if (n == 0) return false;
        p += n;
        rem -= n;
        break;
    }
  }
  return true;
}

}  // namespace mumble
}  // namespace esphome
