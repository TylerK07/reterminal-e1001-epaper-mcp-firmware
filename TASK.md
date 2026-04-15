# TASK.md

Build the MVP firmware skeleton for a reTerminal E1001 ESP32-S3 device using ESP-IDF.

## Read first

- `docs/firmware-architecture.md`
- `docs/capability-contracts.md`
- `docs/state-machine.md`
- `docs/data-model.md`
- `docs/component-interfaces.md`

## Deliverables

- ESP-IDF project structure
- shared enums, error codes, and base types
- platform module headers/source stubs
- service module headers/source stubs
- MCP module headers/source stubs
- `app_controller` skeleton
- `app_main.c`
- compile-oriented placeholder implementations
- clearly marked TODOs for board-specific details

## Constraints

- no heap-heavy public APIs
- services use normalized structs, not raw JSON
- MCP does not directly touch hardware
- secrets are never emitted in public JSON
- provisioning AP mode is separate from normal MCP mode
- DHCP + mDNS + on-screen status after successful connection

## Milestone target

End state for this milestone:
- repo can be opened in Codex or VS Code
- structure is coherent
- interfaces match docs
- core files are ready for implementation work
