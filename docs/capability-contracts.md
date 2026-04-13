# Capability Contract Spec

## Shared conventions

### Auth levels
- `standard`
- `privileged`

### Cost classes
- `LOW`
- `MEDIUM`
- `HIGH`

### Common result envelope

Success:
```json
{
  "ok": true,
  "tool": "get_device_info",
  "result": {}
}
```

Failure:
```json
{
  "ok": false,
  "tool": "refresh_display",
  "error": {
    "code": "ERR_LOW_BATTERY",
    "message": "Operation blocked by low-battery policy.",
    "retryable": true
  }
}
```

## Initial tools

- `get_device_info`
- `get_battery_status`
- `get_wifi_status`
- `scan_wifi`
- `connect_wifi`
- `disconnect_wifi`
- `list_assets`
- `get_config`
- `set_config`
- `render_text`
- `render_bitmap`
- `render_layout`
- `refresh_display`
- `sleep_now`
- `restart_device`
- `reset_network`

## Initial resources

- `device://status`
- `device://power/battery`
- `device://network/wifi`
- `device://config`
- `device://display/capabilities`
- `device://storage/assets`
- `device://render/last-job`

## High-value contract details

### `connect_wifi`
- auth: `privileged`
- cost: `MEDIUM`
- side effects:
  - changes network state
  - may persist credentials
  - may interrupt current network reachability

### `render_text`
- auth: `standard`
- cost: low if framebuffer-only, higher if committed
- side effects:
  - updates framebuffer
  - may trigger display refresh if `commit=true`

### `refresh_display`
- auth: `standard`
- cost:
  - partial = `MEDIUM`
  - full = `HIGH`

### `sleep_now`
- auth: `privileged`
- side effects:
  - device becomes unreachable until wake

### `reset_network`
- auth: `privileged`
- side effects:
  - clears Wi-Fi credentials
  - reboots into provisioning mode

## Policy gates

### Low battery
Block or restrict:
- full refresh
- large bitmap render
- repeated scans
- prolonged AP/network transitions

### Busy display
Only one display refresh at a time

### Provisioning state
MCP normally unavailable until connected mode
