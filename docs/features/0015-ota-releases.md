# 0015 – OTA Updates from GitHub Releases

## Summary

Ship SemVer-tagged firmware releases with per-board manifests and binaries on **GitHub Pages**, attach the same binaries to **GitHub Releases**, and wire ESPHome's `http_request` managed-update path so devices can OTA from Home Assistant.

## Context

CI was failing on `clang-format` (unformatted C++ under `components/mumble/`). Separately, there was no release pipeline or device-facing update channel: users had to compile and flash locally.

GitHub Releases `latest/download` URLs redirect to long object-storage URLs that exceed ESPHome's default HTTP buffer, so manifests and OTA binaries are published on GitHub Pages instead (stable short URLs). Release assets are still attached for humans and archival download.

## Release model

| Item | Choice |
|------|--------|
| Trigger | Git tag `vX.Y.Z` (SemVer); optional `workflow_dispatch` with version input |
| Workflow | `.github/workflows/release.yml` |
| Version source | `esphome.project.version` rewritten from `"dev"` → tag (without leading `v`) before build |
| Project id | `dchote.esp32-mumble` |
| Hosting | GitHub Pages (`https://dchote.github.io/esp32-mumble/`) + GitHub Release assets |
| ESPHome pin | `2026.7.1` (matches `requirements.txt` / `build.yml`) |

### Pages layout

```
https://dchote.github.io/esp32-mumble/
  index.html                          # ESP Web Tools installer
  <slug>/manifest.json                # complete ESP-Web-Tools + update manifest
  <slug>/<name>.factory.bin           # USB / web-tools first flash
  <slug>/<name>.ota.bin               # http_request OTA payload
```

Board slugs match YAML basenames: `esp32-s3-box`, `esp32-s3-box3`, `generic-esp32s3`, `m5stack-atom-echo`, `home-assistant-voice-pe`.

### Device config

Every board YAML includes:

- `esphome.project` with `version: "dev"` on `main`
- `http_request` (`verify_ssl: false`, trusted-LAN posture)
- `ota` platforms: `esphome` (local/dashboard) + `http_request`
- `update` platform `http_request` pointing at that board's Pages manifest

Devices compare `esphome.project.version` to the manifest `version`. Builds from `main` stay at `"dev"` and do not self-offer spurious updates against published SemVer tags in a confusing way (a `"dev"` device will see any tagged release as newer).

## One-time repo setup

Enable GitHub Pages for this repository:

1. **Settings → Pages → Build and deployment → Source:** GitHub Actions
2. First successful `Release` workflow deploy creates the `github-pages` environment

## How to cut a release

```bash
git tag v1.0.0
git push origin v1.0.0
```

Or run **Actions → Release → Run workflow** and enter `1.0.0`.

After the workflow finishes:

- Open https://dchote.github.io/esp32-mumble/ for USB install
- In Home Assistant, use the device's **Firmware Update** entity to install OTA

## Notes / caveats

- First flash after switching framework to ESP-IDF (Box / Box-3 / Voice PE) must be via USB, not OTA.
- Native `ota: platform: esphome` remains for local ESPHome dashboard / passworded OTA.
- Prefer Pages URLs for device update `source:`; do not point devices at `github.com/.../releases/latest/download/...` (redirect buffer issue).
- API encryption and OTA password are optional on every board (commented out by default). Uncomment and set the secrets if you want them enabled.
