# 0016 — Arduino TLS read disconnect after Authenticate

## Summary

Arduino-framework boards (M5Stack Atom Echo, generic ESP32-S3, Onju Voice) dropped the Mumble TCP session a few milliseconds after sending Authenticate. The server had already accepted the user. ESP-IDF boards (ESP32-S3 Box, Box-3, Voice PE) were unaffected. The client was treating “no decrypted bytes yet” from `WiFiClientSecure::read()` as a fatal TLS error.

## Report

Seen on an M5Stack Atom Echo against both Debian Murmur and go-mumble-server. The control channel completed the TLS handshake, the server Version arrived, the client sent Version and Authenticate, and then:

```
[mumble.client]: Server Version: go-mumble-server, crypto=0x7
[mumble.client]: Sent Version (crypto=0x1)
[mumble.client]: Sent Authenticate (user=esp32-C885414D2BA0)
[mumble.client]: TLS read error, disconnecting = -1
[mumble.client]: disconnect
```

The server log for the same attempt was authenticate, then disconnect about 1 ms later. The same loop repeated on every reconnect. `crypto=0x1` is the lite bit only. A legacy client advertises `0x3` (lite | legacy). The server’s `0x7` means it offers lite, legacy, and secure. The server still authenticated the user, so the mode bits were not what closed the socket. Lite is whatever the Crypto select was set to; legacy is the setting that negotiates OCB2.

## Cause

`MumbleClient::loop()` must call `read()` even when `available()` is 0. On ESP-IDF, `esp_tls_get_bytes_avail()` only counts bytes mbedTLS has already decrypted, so the read is what pulls the next TLS record. `TlsClientEspIdf::read()` maps `ESP_TLS_ERR_SSL_WANT_READ` / `WANT_WRITE` to `0`, and the loop waits for the next iteration.

On Arduino 3.x, `WiFiClientSecure` is `NetworkClientSecure`. Its socket is `O_NONBLOCK`. `read()` does this before any `mbedtls_ssl_read` into the caller’s buffer:

```cpp
int avail = available();
if ((!buf && size) || avail <= 0)
  return -1;
```

`available()` calls `mbedtls_ssl_read(..., NULL, 0)`. When the peer has not sent the next record, that returns `MBEDTLS_ERR_SSL_WANT_READ` and `available()` returns 0, so `read()` returns `-1`. `MumbleClient` treats `n < 0` as fatal and disconnects.

That poll runs in the same loop turn as `on_version()`, which sends Version and Authenticate and then immediately reads again. CryptSetup, channel state, and ServerSync are still in flight, so the read is empty, returns `-1`, and the client closes. Murmur and go-mumble-server both show a client disconnect because the ESP32 closed the TCP connection after a successful authenticate.

A second bug in the same backend made the TCP connect budget 10 ms. `NetworkClientSecure::connect(host, port, timeout)` stores that timeout in milliseconds (`_timeout`, default 30000). `setHandshakeTimeout()` takes seconds and multiplies by 1000. The Arduino connect path passed `timeout_ms / 1000` (10) into `connect()`, so the non-blocking TCP `select` waited 10 ms. The handshake timeout happened to be correct (10 s). A fast LAN could still complete TCP; a slower path could fail the connect before the handshake.

## Fix

`TlsClientArduino::read()` now matches the ESP-IDF contract: `0` means no application data yet, `-1` means the session is dead. It calls `available()` first (that pumps mbedTLS). A real TLS error makes `available()` call `stop()`, and the wrapper then returns `-1`. An idle poll returns `0`.

`connect(..., timeout_ms)` stores milliseconds in `NetworkClient::_timeout`. `Stream::setTimeout()` writes a different field and is not used. `setHandshakeTimeout(timeout_ms / 1000)` is unchanged (that API takes seconds).

The fatal-read log includes the return code: `TLS read error, disconnecting (%d)`.

## Verification

- `esphome compile esphome/m5stack-atom-echo.yaml` succeeded with the Arduino backend (ESPHome 2026.9.1, Arduino-ESP32 3.3.10).
- A Home Assistant Voice PE on USB (`esp32-mumble-voice-pe-093ba4`, ESP-IDF) against an existing go-mumble-server sent `crypto=0x3`, received CryptSetup (legacy OCB2), reached ServerSync, and confirmed UDP with a ping echo. The first attempt after reset was `Rejected: Username in use` from the session that had not dropped yet; the retry stayed up. It never logged a TLS read error.
- A local `go-mumble-server` on `:64738` accepted a host test client through ServerSync (Root channel). The following server “disconnected” line was the test client exiting.

## What to ship

Arduino boards need a firmware build that includes this fix. An SDK bump or a different ESP32 module does not change `NetworkClientSecure::read()`. ESP-IDF targets already waited on `WANT_READ`.

## References

- `components/mumble/mumble_socket.cpp` — `TlsClientArduino`
- `components/mumble/mumble_client.cpp` — read loop around `tls_client_->read()`
- Arduino-ESP32 `libraries/NetworkClientSecure/src/NetworkClientSecure.cpp` (`read()`, `connect(..., timeout)`)
- [0003-mumble-connection-protocol.md](0003-mumble-connection-protocol.md)
