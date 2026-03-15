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
void MsgChannelState::marshal(std::vector<uint8_t> &out) const {
  if (channel_id != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, channel_id);
  }
  if (has_parent || parent != 0) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, parent);
  }
  if (!name.empty()) proto_append_string(out, 3, name);
  for (uint32_t link : links) {
    proto_append_tag(out, 4, WIRE_VARINT);
    proto_append_varint(out, link);
  }
  if (temporary) {
    proto_append_tag(out, 8, WIRE_VARINT);
    proto_append_varint(out, 1);
  }
  if (position != 0) {
    proto_append_tag(out, 9, WIRE_VARINT);
    proto_append_varint(out, static_cast<uint64_t>(static_cast<int64_t>(position)));
  }
  if (max_users != 0) {
    proto_append_tag(out, 11, WIRE_VARINT);
    proto_append_varint(out, max_users);
  }
}

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
void MsgChannelRemove::marshal(std::vector<uint8_t> &out) const {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, channel_id);
}

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
      case 14: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        comment.assign(reinterpret_cast<const char *>(s), sl);
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

void MsgUserState::marshal(std::vector<uint8_t> &out) const {
  if (session != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, session);
  }
  if (actor != 0) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, actor);
  }
  if (!name.empty()) proto_append_string(out, 3, name);
  if (user_id != 0) {
    proto_append_tag(out, 4, WIRE_VARINT);
    proto_append_varint(out, user_id);
  }
  if (channel_id != 0) {
    proto_append_tag(out, 5, WIRE_VARINT);
    proto_append_varint(out, channel_id);
  }
  if (mute) proto_append_bool(out, 6, true);
  if (deaf) proto_append_bool(out, 7, true);
  if (suppress) proto_append_bool(out, 8, true);
  if (self_mute) proto_append_bool(out, 9, true);
  if (self_deaf) proto_append_bool(out, 10, true);
  if (!comment.empty()) proto_append_string(out, 14, comment);
}

void MsgUserState::marshal_channel_change(std::vector<uint8_t> &out, uint32_t session_id,
                                          uint32_t channel_id) {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, session_id);
  proto_append_tag(out, 5, WIRE_VARINT);
  proto_append_varint(out, channel_id);
}

// MsgUserRemove
void MsgUserRemove::marshal(std::vector<uint8_t> &out) const {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, session);
  if (actor != 0) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, actor);
  }
  if (!reason.empty()) proto_append_string(out, 3, reason);
  if (ban) proto_append_bool(out, 4, true);
}

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
        reason.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        ban = (v != 0);
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

// MsgTextMessage
void MsgTextMessage::marshal(std::vector<uint8_t> &out) const {
  if (actor != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, actor);
  }
  for (uint32_t v : session) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  for (uint32_t v : channel_id) {
    proto_append_tag(out, 3, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  for (uint32_t v : tree_id) {
    proto_append_tag(out, 4, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  if (!message.empty()) proto_append_string(out, 5, message);
}

bool MsgTextMessage::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  session.clear();
  channel_id.clear();
  tree_id.clear();
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
        actor = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_id.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        tree_id.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        message.assign(reinterpret_cast<const char *>(s), sl);
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

// MsgPermissionDenied
bool MsgPermissionDenied::unmarshal(const uint8_t *data, size_t len) {
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
        permission = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        reason.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        type = static_cast<DenyType>(v);
        p += n;
        rem -= n;
        break;
      }
      case 6: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        name.assign(reinterpret_cast<const char *>(s), sl);
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

// MsgACL
void MsgACL::marshal(std::vector<uint8_t> &out) const {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, channel_id);
  if (!inherit_acls) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, 0);
  }
  if (query) {
    proto_append_tag(out, 5, WIRE_VARINT);
    proto_append_varint(out, 1);
  }
}

bool MsgACL::unmarshal(const uint8_t *data, size_t len) {
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
        inherit_acls = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        query = (v != 0);
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

// MsgQueryUsers
void MsgQueryUsers::marshal(std::vector<uint8_t> &out) const {
  for (uint32_t v : ids) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  for (const std::string &s : names) {
    proto_append_string(out, 2, s);
  }
}

bool MsgQueryUsers::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  ids.clear();
  names.clear();
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
        ids.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        names.emplace_back(reinterpret_cast<const char *>(s), sl);
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

// MsgBanEntry (unmarshal from embedded bytes)
static bool unmarshal_ban_entry(const uint8_t *data, size_t len, MsgBanEntry &be) {
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
        be.address.assign(d, d + dl);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        be.mask = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        be.name.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        be.hash.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        be.reason.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 6: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        be.start.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 7: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        be.duration = static_cast<uint32_t>(v);
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

// MsgBanList
void MsgBanList::marshal(std::vector<uint8_t> &out) const {
  for (const MsgBanEntry &be : bans) {
    std::vector<uint8_t> eb;
    if (!be.address.empty()) proto_append_bytes(eb, 1, be.address.data(), be.address.size());
    proto_append_tag(eb, 2, WIRE_VARINT);
    proto_append_varint(eb, be.mask);
    if (!be.name.empty()) proto_append_string(eb, 3, be.name);
    if (!be.hash.empty()) proto_append_string(eb, 4, be.hash);
    if (!be.reason.empty()) proto_append_string(eb, 5, be.reason);
    if (!be.start.empty()) proto_append_string(eb, 6, be.start);
    if (be.duration != 0) {
      proto_append_tag(eb, 7, WIRE_VARINT);
      proto_append_varint(eb, be.duration);
    }
    proto_append_bytes(out, 1, eb.data(), eb.size());
  }
  if (query) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, 1);
  }
}

bool MsgBanList::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  bans.clear();
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
        MsgBanEntry be;
        if (!unmarshal_ban_entry(d, dl, be)) return false;
        bans.push_back(std::move(be));
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        query = (v != 0);
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

// MsgContextActionModify
bool MsgContextActionModify::unmarshal(const uint8_t *data, size_t len) {
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
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        action.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        text.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        context = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        operation = static_cast<uint32_t>(v);
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

// MsgContextAction
void MsgContextAction::marshal(std::vector<uint8_t> &out) const {
  if (session != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, session);
  }
  proto_append_tag(out, 2, WIRE_VARINT);
  proto_append_varint(out, channel_id);
  if (!action.empty()) proto_append_string(out, 3, action);
}

bool MsgContextAction::unmarshal(const uint8_t *data, size_t len) {
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
        channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        action.assign(reinterpret_cast<const char *>(s), sl);
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

// MsgUserList - repeated User (field 1 = length-delimited embedded user with 1=user_id, 2=name, 3=last_seen, 4=last_channel)
static bool unmarshal_user_list_user(const uint8_t *data, size_t len, MsgUserListUser &u) {
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
        u.user_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        u.name.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        u.last_seen.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        u.last_channel = static_cast<int32_t>(v);
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

bool MsgUserList::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  users.clear();
  while (rem > 0) {
    int fn, wt;
    int n = proto_read_tag(p, rem, &fn, &wt);
    if (n == 0) return false;
    p += n;
    rem -= n;
    if (fn == 1) {
      const uint8_t *d;
      size_t dl;
      n = proto_read_length_delimited(p, rem, &d, &dl);
      if (n == 0) return false;
      MsgUserListUser u;
      if (!unmarshal_user_list_user(d, dl, u)) return false;
      users.push_back(std::move(u));
      p += n;
      rem -= n;
    } else {
      n = proto_skip_field(p, rem, wt);
      if (n == 0) return false;
      p += n;
      rem -= n;
    }
  }
  return true;
}

// MsgVoiceTargetTarget (unmarshal from embedded bytes)
static bool unmarshal_voice_target_target(const uint8_t *data, size_t len, MsgVoiceTargetTarget &t) {
  const uint8_t *p = data;
  size_t rem = len;
  t.session.clear();
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
        t.session.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        t.channel_id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        t.group.assign(reinterpret_cast<const char *>(s), sl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        t.links = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 5: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        t.children = (v != 0);
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

// MsgVoiceTarget
void MsgVoiceTarget::marshal(std::vector<uint8_t> &out) const {
  if (id != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, id);
  }
  for (const MsgVoiceTargetTarget &t : targets) {
    std::vector<uint8_t> tb;
    for (uint32_t s : t.session) {
      proto_append_tag(tb, 1, WIRE_VARINT);
      proto_append_varint(tb, s);
    }
    proto_append_tag(tb, 2, WIRE_VARINT);
    proto_append_varint(tb, t.channel_id);
    if (!t.group.empty()) proto_append_string(tb, 3, t.group);
    if (t.links) {
      proto_append_tag(tb, 4, WIRE_VARINT);
      proto_append_varint(tb, 1);
    }
    if (t.children) {
      proto_append_tag(tb, 5, WIRE_VARINT);
      proto_append_varint(tb, 1);
    }
    if (!tb.empty()) proto_append_bytes(out, 2, tb.data(), tb.size());
  }
}

bool MsgVoiceTarget::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  targets.clear();
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
        id = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        const uint8_t *d;
        size_t dl;
        n = proto_read_length_delimited(p, rem, &d, &dl);
        if (n == 0) return false;
        MsgVoiceTargetTarget t;
        if (!unmarshal_voice_target_target(d, dl, t)) return false;
        targets.push_back(std::move(t));
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

// MsgUserStats
void MsgUserStats::marshal(std::vector<uint8_t> &out) const {
  if (session != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, session);
  }
  if (stats_only) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, 1);
  }
}

bool MsgUserStats::unmarshal(const uint8_t *data, size_t len) {
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
        stats_only = (v != 0);
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

// MsgRequestBlob
void MsgRequestBlob::marshal(std::vector<uint8_t> &out) const {
  for (uint32_t v : session_texture) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  for (uint32_t v : session_comment) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  for (uint32_t v : channel_description) {
    proto_append_tag(out, 3, WIRE_VARINT);
    proto_append_varint(out, v);
  }
}

bool MsgRequestBlob::unmarshal(const uint8_t *data, size_t len) {
  const uint8_t *p = data;
  size_t rem = len;
  session_texture.clear();
  session_comment.clear();
  channel_description.clear();
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
        session_texture.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        session_comment.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        channel_description.push_back(static_cast<uint32_t>(v));
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

// MsgSuggestConfig
bool MsgSuggestConfig::unmarshal(const uint8_t *data, size_t len) {
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
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        positional = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        push_to_talk = (v != 0);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        version_v2 = v;
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

// MsgPluginDataTransmission
void MsgPluginDataTransmission::marshal(std::vector<uint8_t> &out) const {
  if (sender_session != 0) {
    proto_append_tag(out, 1, WIRE_VARINT);
    proto_append_varint(out, sender_session);
  }
  for (uint32_t v : receiver_sessions) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, v);
  }
  if (!data.empty()) proto_append_bytes(out, 3, data.data(), data.size());
  if (!data_id.empty()) proto_append_string(out, 4, data_id);
}

bool MsgPluginDataTransmission::unmarshal(const uint8_t *buf, size_t len) {
  const uint8_t *p = buf;
  size_t rem = len;
  receiver_sessions.clear();
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
        sender_session = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 2: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        receiver_sessions.push_back(static_cast<uint32_t>(v));
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        const uint8_t *d;
        size_t dl;
        n = proto_read_length_delimited(p, rem, &d, &dl);
        if (n == 0) return false;
        data.assign(d, d + dl);
        p += n;
        rem -= n;
        break;
      }
      case 4: {
        const uint8_t *s;
        size_t sl;
        n = proto_read_length_delimited(p, rem, &s, &sl);
        if (n == 0) return false;
        data_id.assign(reinterpret_cast<const char *>(s), sl);
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

// MsgPermissionQuery
void MsgPermissionQuery::marshal(std::vector<uint8_t> &out) const {
  proto_append_tag(out, 1, WIRE_VARINT);
  proto_append_varint(out, channel_id);
  if (permissions != 0) {
    proto_append_tag(out, 2, WIRE_VARINT);
    proto_append_varint(out, permissions);
  }
  if (flush) {
    proto_append_tag(out, 3, WIRE_VARINT);
    proto_append_varint(out, 1);
  }
}

bool MsgPermissionQuery::unmarshal(const uint8_t *data, size_t len) {
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
        permissions = static_cast<uint32_t>(v);
        p += n;
        rem -= n;
        break;
      }
      case 3: {
        uint64_t v;
        n = proto_read_varint(p, rem, &v);
        if (n == 0) return false;
        flush = (v != 0);
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
