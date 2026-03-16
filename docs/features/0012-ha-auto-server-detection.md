# 0012 – Home Assistant Auto-Detect Mumble Server Address

## Summary

Defaults the Mumble server address to empty (unset) instead of `192.168.1.100`. When the device is adopted by Home Assistant, the server address is auto-populated with the HA server IP (via ESPHome's `on_client_connected` API trigger and `client_address`). This supports the common deployment where users run go-mumble-server as a Home Assistant addon on the same host. A manually set server is never overwritten; it persists in NVS. Reset Config now resets server to empty, so the next HA connection re-applies the HA IP.

## Resolution flow

- **Device boots**: If the server text entity has a value in NVS (user-set or previously auto-detected), that value is used. Otherwise server is empty.
- **HA connects**: `api.on_client_connected` fires with `client_address` (HA server IP). If the server text entity is still empty, it is set to `client_address` and persisted to NVS. The Mumble component's `loop()` sees the change and connects.
- **Manual override**: User can set the server in YAML or in the HA UI at any time; that value is stored and never overwritten by auto-detection.

## Implemented

### YAML configs (all four board configs)

- **Server text entity**: `initial_value` changed from `"192.168.1.100"` to `""`.
- **Mumble block**: `server:` changed from `"192.168.1.100"` to `""` with comment: empty by default; auto-filled from HA IP when adopted, or set manually.
- **API**: `on_client_connected` trigger added. If `mumble_server_host` state is empty, set it to `client_address` and log. Does not overwrite existing values.

### C++ component

- **`reset_config()`**: Server is reset to `""` instead of `"192.168.1.100"`. Next HA connection will re-apply the HA IP if desired.
- **`publish_empty_text_defaults()`**: Now also force-publishes empty string for `server_text_` when it has no state, so HA shows `""` instead of "unknown" when server is unset on first boot.

### Documentation

- **product-overview.md**: Server entity description notes default empty and auto-population from HA when adopted; Reset Config description notes server resets to empty (re-triggers auto-detection).
- **technical-overview.md**: New "Home Assistant server auto-detection" paragraph; example YAML updated to `initial_value: ""`, `server: ""`, and `api.on_client_connected` lambda; config table `server` row updated.
- **build.md**: Configuration section updated to describe server default empty and HA auto-detection.

## Files changed

- `esphome/esp32-s3-box.yaml`, `esphome/esp32-s3-box3.yaml`, `esphome/generic-esp32s3.yaml`, `esphome/m5stack-atom-echo.yaml`: Server default to empty; `api.on_client_connected` lambda.
- `components/mumble/mumble_component.cpp`: `reset_config()` server to `""`; `publish_empty_text_defaults()` includes `server_text_`.
- `docs/product-overview.md`, `docs/technical-overview.md`, `docs/build.md`: Descriptions and examples updated.

## Edge cases

- **Non-HA users**: Server stays empty until set manually in YAML or via another API client; device does not connect until then.
- **Multiple API clients**: First connecting client sets the server if empty; in practice this is HA. User can always override.
- **Existing users upgrading**: NVS already contains a server value; it is restored on boot and auto-detection does not overwrite it.
