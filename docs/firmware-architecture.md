# Firmware Architecture Spec

## Purpose

Custom firmware for the reTerminal E1001 that replaces the stock SenseCraft-oriented runtime with a low-power ESP-IDF stack exposing MCP-compatible capabilities.

## Core principles

- Layered architecture
- Power-aware by default
- Capability-oriented design
- Deterministic behavior
- Board portability

## Layers

```text
MCP -> Services -> Platform -> Board/ESP-IDF
```

### Core
- app_main
- app_controller
- capabilities
- errors
- types

### Platform
- board
- display_epaper
- wifi
- ble
- battery
- power
- buttons
- storage_sd
- storage_nvs
- system

### Services
- render_service
- config_service
- asset_service
- status_service
- sleep_service
- job_service
- network_service
- provisioning_service

### MCP
- mcp_server
- mcp_dispatch
- mcp_auth
- mcp_registry

## Key architecture decisions

- **Provisioning** is separate from MCP
- **Initial Wi-Fi setup** uses AP mode
- **Connected mode** uses DHCP + mDNS
- **MCP only starts after** valid network connection and policy checks
- **Render pipeline** updates framebuffer first, refresh second
- **Power policy** can block high-cost operations
- **Config** is versioned and stored in NVS
- **Assets** come from SD-backed storage

## Initial capability inventory

- get device info
- get battery status
- get Wi-Fi status
- scan Wi-Fi
- connect Wi-Fi
- disconnect Wi-Fi
- list assets
- get config
- set config
- render text
- render bitmap
- render layout
- refresh display
- sleep now
- restart device
- reset network
