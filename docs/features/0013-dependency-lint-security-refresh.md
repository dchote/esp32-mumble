# 0013 – Dependency, Lint, and Security Refresh

## Summary

Bump the project’s real dependency surface (ESPHome 2026.7.1 + vendored micro-opus 0.4.1), add lightweight lint/CI gates, and harden example configs and a few protocol DoS edges without changing the intentional trusted-LAN TLS default.

## Context

This repo has almost no traditional manifests. Dependencies that matter:

| Surface | Previous | Target |
|---------|----------|--------|
| ESPHome CLI / `min_version` | Docs 2024.x; YAML `2025.5.0` | Host + CI **2026.7.1**; YAML `min_version: "2026.7.0"` |
| Vendored Opus | `lib/micro-opus/` 0.3.4 | Refresh from esphome-libs/micro-opus **v0.4.1** |
| ESP-IDF / PlatformIO | Unpinned (float with ESPHome) | Leave floating; re-resolve via ESPHome 2026.7.1 |
| CI | Removed | Restore compile + lint workflow on ESPHome 2026.7.1 |
| Lint / SAST | None | pre-commit (ruff + clang-format) |

Product constraints: trusted LAN; Legacy default; TLS verify-off when `ca_cert` empty is intentional. This work does **not** flip TLS insecure defaults.

## Changes

### Host tooling and docs

- Root `requirements.txt` with `esphome==2026.7.1`
- Install guidance updated for Python 3.12+ and pinned ESPHome
- YAML `min_version` bumped to `2026.7.0` on all four board configs
- All boards set `esp32.toolchain: platformio` (native ESP-IDF toolchain cannot use local `symlink://` micro-opus or the mbedtls pre-script)

### Config hardening

- HA API encryption and OTA password documented as **optional** (not enabled by default, so existing deployments keep working)
- `secrets.example.yaml` lists optional keys as comments
- Short YAML comments pointing at optional `ca_cert` for verified TLS
- Removed deprecated `bits_per_channel` from Box/Box-3 I2S microphone configs (ESPHome 2026.7)

### Vendored Opus

- `lib/micro-opus/` refreshed to upstream v0.4.1 (sources already matched staged opus; `opus_custom.h` + support files synced; version metadata bumped)

### Protocol DoS hardening

- Hard caps (128) on `channels_` / `users_` growth in the Mumble client

### Component / build fixes for 2026.7

- All `register_action` calls set `synchronous=` explicitly
- `scripts/patch_mbedtls_requires.py` creates `src/CMakeLists.txt` with `REQUIRES mbedtls` when missing (clean PlatformIO builds)

### Lint and CI

- `.pre-commit-config.yaml` (ruff + clang-format), `.clang-format`, `pyproject.toml`
- `.github/workflows/build.yml` restored (lint + compile all four configs with dummy secrets)

### Security docs

- Root `SECURITY.md` (threat model, TLS/`ca_cert`, Lite UDP, API/OTA secrets)
- Stale CI note in feature `0002` corrected

## Verification

Compiled successfully with ESPHome 2026.7.1:

- `esphome/esp32-s3-box.yaml`
- `esphome/esp32-s3-box3.yaml`
- `esphome/generic-esp32s3.yaml`
- `esphome/m5stack-atom-echo.yaml`

## Files changed

- New: `docs/features/0013-dependency-lint-security-refresh.md`, `requirements.txt`, `SECURITY.md`, `.pre-commit-config.yaml`, `.clang-format`, `pyproject.toml`, `.github/workflows/build.yml`
- Update: `lib/micro-opus/**`, four `esphome/*.yaml`, `esphome/secrets.example.yaml`, `components/mumble/*`, `README.md`, `docs/build.md`, `docs/technical-overview.md`, `docs/features/0002-initial-code-framework.md`

## Out of scope

- Enabling TLS verification by default / requiring `ca_cert`
- Secure-tier client certs (v1 non-goal)
- clang-tidy, CodeQL, Dependabot/Renovate
- Updating gitignored `research/` trees
