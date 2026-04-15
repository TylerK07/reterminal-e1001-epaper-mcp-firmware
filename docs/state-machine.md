# State Machine and Boot Flow Spec

## Top-level runtime states

- `BOOTING`
- `UNPROVISIONED`
- `PROVISIONING`
- `CONNECTING`
- `CONNECTED_IDLE`
- `CONNECTED_ACTIVE`
- `SLEEP_PREP`
- `SLEEPING`
- `ERROR_RECOVERY`

## Core lifecycle

### First boot
```text
BOOTING -> UNPROVISIONED -> PROVISIONING
```

### Successful provisioning
```text
PROVISIONING -> CONNECTING -> CONNECTED_IDLE
```

### Active request handling
```text
CONNECTED_IDLE -> CONNECTED_ACTIVE -> CONNECTED_IDLE
```

### Sleep
```text
CONNECTED_IDLE -> SLEEP_PREP -> SLEEPING -> BOOTING
```

### Recovery on network failure
```text
CONNECTING -> ERROR_RECOVERY -> UNPROVISIONED -> PROVISIONING
```

## MCP enablement rules

MCP may start only when:
- connected to Wi-Fi in station mode
- IP assigned
- auth config valid
- provisioning not active
- power policy allows it

MCP must be disabled when:
- booting
- unprovisioned
- provisioning
- connecting
- sleep prep
- sleeping
- recovery suppresses online services

## Provisioning flow

- Start AP mode
- Host local setup UI
- Display AP SSID and setup IP (`192.168.4.1`)
- Accept:
  - SSID
  - password
  - device name
  - auth token
- Persist config
- Transition to connecting

## Connected flow

- connect via STA
- obtain DHCP lease
- advertise mDNS hostname
- show SSID, IP, and hostname on display
- start MCP server

## Recovery behavior

- bounded retries only
- repeated failures fall back to provisioning or sleep
- invalid config should not brick the device
