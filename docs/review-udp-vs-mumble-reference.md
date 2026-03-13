# UDP Implementation Review vs. Mumble Reference

Comparison of the ESP32 Mumble client against:
- **Mumble C++ client/server** (`/Users/daniel/Projects/New/go-mumble-server/research/mumble`)
- **go-mumble-server** (`/Users/daniel/Projects/New/go-mumble-server`)

## Summary

The protocol and crypto implementation **align with** the reference. No obvious wire-format mismatches.

**Framework finding (2025-03)**: With **Arduino** (WiFiUDP), UDP packets reach the server and ping echo works. With **ESP-IDF** (BSD sockets), `sendto()` returns success but unicast UDP packets did not leave the device. **Fix**: Use lwIP **netconn** API (`netconn_sendto`) instead of BSD sockets for UDP; netconn uses the tcpip task path. ESP32-S3 Box now uses ESP-IDF with netconn for UDP.

**Voice transmit (2025-03)**: Device sends voice via UDP; server receives it via UDPTunnel (TCP) when UDP path is missing. Server logs may show `SendAudio no UDP path` when relaying to the ESP32 — server-side UDP address handling for the device is not fully validated yet.

---

## 1. OCB2 Wire Format ✓

**Mumble C++ (CryptStateOCB2.cpp)** and **go-mumble-server (cryptstate.go)**:
- Header: 4 bytes = `dst[0]` (IV low byte) + `dst[1..3]` (3-byte tag)
- Rest: ciphertext (same length as plaintext)

**ESP32 (mumble_ocb2.cpp)**:
- Same layout: `dst[0]=encrypt_iv_[0]`, `dst[1..3]=tag`, ciphertext at `dst+4`.
- `OCB2_OVERHEAD = 4` matches `legacyOverhead` in go-mumble-server.

## 2. OCB2 Core Algorithm ✓

- **GF(2^128) multiplication**: All use `times2` / `times3` with polynomial `0x87` (135), `block[0]` = MSB.
- **Nonce increment**: LSB-first (byte 0), consistent across implementations.
- **Partial block length**: Last two bytes (14–15) big-endian `len*8`, same as reference.

## 3. CryptSetup Nonce Mapping ✓

- Server sends: `ClientNonce` = server’s decrypt nonce = client’s encrypt nonce  
- Client `set_key(key, client_nonce, server_nonce)`  
  - `encrypt_iv = client_nonce` → encrypt to server  
  - `decrypt_iv = server_nonce` → decrypt from server  

Mapping matches go-mumble-server and Mumble.

## 4. UDP Ping Format ✓

- Legacy ping: header byte `0x20` (codec type 1 = Ping).
- Data: varint-encoded timestamp.
- Varint format matches `PacketDataStream` / `go-mumble-server pkg/mumble/audio/varint.go`.

## 5. UDP Port and Binding ✓

- Client binds to port `0` (ephemeral).
- Server uses Mumble port (64738) for both TCP and UDP.
- No change needed.

## 6. UDP Target Address ✓

- Uses TLS peer IP via `get_peer_ip()` for UDP target, avoiding DNS/IP mismatch.

---

## Areas to Verify

### A. UDP Socket Usage

ESP-IDF uses `UdpSocketNetconn::end_packet()` with `netconn_sendto()`. Target IP:port and `netconn_sendto` err are logged on failure. Transport text sensor shows UDP vs TCP.

### B. OCB2 XEX* Attack Countermeasure

The Mumble C++ client flips one bit in rare cases (XEX* mitigation, section 9 of eprint.iacr.org/2019/311). The ESP32 OCB2 does the same. go-mumble-server’s OCB2 does **not** implement this; it will still decrypt correctly, but the resulting plaintext may differ by one bit in rare cases. This is unlikely to affect pings and is acceptable for voice.

### C. go-mumble-server Trial-Decrypt Order

`HandleUDP` tries Secure → Legacy → Lite. The ESP32 uses Legacy; the server should successfully decrypt with the Legacy CryptState when iterating connections.

### D. UDP Echo Timing

The server echoes pings to the address it received from. If the client uses an unexpected source port or IP, replies might not be routable (NAT, firewall). The symptom (“UDP ping echo not received”) is consistent with echo packets never reaching the client.

---

## Recommended Next Steps

1. **ESP32-S3 Box**: ESP-IDF with lwIP netconn for UDP — works. **Box-3 and others**: Arduino (WiFiUDP) — works.

2. **netconn fix** (implemented): ESP-IDF UDP now uses `netconn_sendto()` instead of BSD `sendto()`. `UdpSocketNetconn` in mumble_socket.cpp.

3. **Decrypt test**
   - On a dev machine, use a test harness (e.g. from go-mumble-server `cryptstate_test.go`) to ensure ESP32-encrypted packets decrypt correctly with the same key/nonce setup.
