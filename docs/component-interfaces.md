# Component Interface Spec

## Platform components

### `board`
Immutable hardware constants and pin mappings.

### `display_epaper`
Public responsibilities:
- init
- status
- clear framebuffer
- draw text
- draw bitmap
- draw layout
- refresh full
- refresh partial

### `wifi`
Public responsibilities:
- init
- AP start/stop
- STA connect/disconnect
- scan
- get status
- set hostname

### `battery`
Public responsibilities:
- init
- get status

### `power`
Public responsibilities:
- init
- get wake reason
- enter deep sleep
- get policy
- helper for operation allowance

### `buttons`
Public responsibilities:
- init
- pressed state
- handler registration

### `storage_sd`
Public responsibilities:
- init
- mount
- unmount
- list
- exists
- read

### `storage_nvs`
Public responsibilities:
- init
- write blob
- read blob
- erase key
- key exists

## Service components

### `config_service`
- init
- get config
- get redacted JSON
- patch config
- set/clear Wi-Fi creds
- set/get auth token
- reset defaults

### `status_service`
- init
- get consolidated snapshot
- get device/battery/Wi-Fi/provisioning/display
- get snapshot JSON

### `asset_service`
- init
- list
- exists
- read

### `render_service`
- init
- render text
- render bitmap
- render layout
- refresh
- get last render record

### `network_service`
- init
- connect from config
- disconnect
- scan
- get status

### `provisioning_service`
- init
- start/stop
- active?
- get status
- submit provisioning data
- reset

### `job_service`
- init
- create
- mark running/completed/failed
- get record
- get latest

### `sleep_service`
- init
- request now
- prepare
- enter

## MCP components

### `mcp_auth`
- init
- validate token
- configured?

### `mcp_models`
- normalized request/response carrier structs

### `mcp_dispatch`
- init
- handle request

### `mcp_registry`
- static metadata for tools/resources

### `mcp_server`
- init
- start
- stop
- get status

## Control-plane component

### `app_controller`
Owns:
- boot flow
- state transitions
- provisioning vs connecting routing
- MCP enable/disable lifecycle
- recovery paths
- sleep transitions
