# reTerminal E1001 MCP Firmware

Spec-first starter bundle for building a custom ESP-IDF firmware for the Seeed reTerminal E1001.

## Goal

Replace the stock SenseCraft-oriented runtime with a custom firmware that:

- exposes device capabilities through an MCP-compatible interface
- supports provisioning via AP mode on first boot
- uses DHCP + mDNS in connected mode
- respects low-power ePaper behavior
- cleanly separates platform, services, and MCP transport

## What is included

- architecture and decision docs in `docs/`
- Codex-oriented handoff files:
  - `AGENTS.md`
  - `TASK.md`
- starter firmware tree under `firmware/`
- headers and source stubs for:
  - core
  - platform
  - services
  - mcp
  - app controller

## Current design decisions

- **Firmware base:** ESP-IDF on ESP32-S3
- **Provisioning:** AP mode on first boot / after network reset
- **Discovery after provisioning:** DHCP + mDNS + render IP/hostname on display
- **MCP transport:** HTTP-based server while connected and policy-allowed
- **MCP exposure:** tools + resources, with auth and power gating
- **Power model:** deep sleep aware, low-battery restrictions, bounded active windows
- **Display model:** framebuffer updates separate from physical refresh
- **Config model:** versioned config in NVS with redacted public views
- **Storage:** SD-backed assets, NVS for config/state
- **Safety:** no raw secrets exposed over MCP

## Suggested first implementation milestone

1. Create a compilable ESP-IDF skeleton
2. Implement shared enums, errors, and models
3. Implement boot/provision/connect state controller
4. Add MCP metadata registry and dispatch stubs
5. Add minimal status/config/render/provisioning services
6. Add TODO placeholders for board-specific hardware details

## Recommended Codex prompt

See `TASK.md`.



## Setting up github with worktrees:

Because I always forget how worktrees (and github) work... here are instructions for everyone!

Go to your local `reterminal-e1001-epaper-mcp-firmware` folder

Bare Repository & Main Setup:
```sh
git clone --bare git@github.com:TylerK07/reterminal-e1001-epaper-mcp-firmware.git reterminal-e1001-epaper-mcp-firmware.git
cd reterminal-e1001-epaper-mcp-firmware.git
git fetch origin
git worktree add ../trees/main main
```

Branch Setup:
```sh
git worktree add -b [branchname: firstlast/feature-name] [file location of the branch ../trees/firstlast-feature-name] main
git push --set-upstream origin [branchname]
```

Commit:
```sh
git add . // add files
git status // see which files are going to be committed
git commit -m "message" // commit locally
git push // push to the cloud on github
git merge main // pull the latest from the main branch into my worktree
```