# Security

## Threat model

ESP32-Mumble targets **trusted LAN** deployments (home or small office network). The default configuration assumes the LAN and Mumble server operator are trusted. It is not hardened for use on untrusted or public networks.

## Mumble control channel (TCP/TLS)

- The device connects to the Mumble server over TLS.
- By default, **server certificate verification is disabled** (`ca_cert` empty; ESP-IDF/Arduino insecure TLS mode). This avoids friction with self-signed Murmur / go-mumble-server certs on a LAN.
- To enable verification, set the Mumble component `ca_cert` option to a PEM-encoded CA (or server) certificate. When `ca_cert` is set, the client verifies the server certificate.

## UDP voice crypto

| Mode | UDP treatment | Notes |
|------|---------------|-------|
| **Legacy** (default) | OCB2-AES128 | Compatible with standard Mumble servers |
| **Lite** | Cleartext | Trusted LAN only; lower CPU |
| **Secure** | AES-256-GCM | Accepted if the server negotiates it; client-initiated Secure (client cert / TLS 1.3) is not implemented |

Do not use Lite mode on networks you do not trust.

## Home Assistant API and OTA

Example board configs do **not** require API encryption or an OTA password (to avoid breaking existing deployments). Both are recommended on untrusted LANs:

- `api.encryption.key: !secret api_encryption_key` — encrypts the ESPHome native API
- `ota.password: !secret ota_password` — password-protects OTA updates

Generate an API key with `openssl rand -base64 32`, add the secrets, and uncomment the matching keys in your board YAML. Keep `esphome/secrets.yaml` out of version control (ignored by `esphome/.gitignore`).

## Credentials at rest

Mumble username/password and other settings may be stored in NVS and exposed as Home Assistant entities. Physical access to a flashed device can recover NVS contents. Treat devices as trusted endpoints on the LAN.

## Reporting issues

Please open a private security report or a GitHub issue describing the vulnerability without publishing exploit details until a fix is available. Include affected boards, ESPHome version, and crypto mode when relevant.
