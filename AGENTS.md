# AGENTS.md

## Project intent

Build a spec-driven ESP-IDF firmware skeleton for a reTerminal E1001 device that exposes capabilities via an MCP-compatible interface.

## Rules

- Follow the docs in `docs/` before changing structure or APIs.
- Do not collapse layers:
  - MCP -> Services -> Platform
- Do not put raw JSON parsing into services.
- Do not let MCP handlers call ESP-IDF hardware APIs directly.
- Do not expose secrets in public JSON/resource outputs.
- Prefer fixed-size structs and bounded buffers in public interfaces.
- Keep display refresh separate from framebuffer mutation.
- Keep provisioning separate from normal MCP runtime.

## Coding style

- C for firmware
- snake_case for fields and functions
- `_t` suffix for public types
- stable `error_code_t` returns for public APIs
- clear TODO markers for unknown hardware specifics

## Phase-1 priorities

- project skeleton
- headers and stubs compile cleanly where possible
- app controller state machine skeleton
- provisioning model
- status/config/render service stubs
- MCP registry/dispatch/server stubs

## Avoid

- guessing undocumented pin mappings without labeling them as placeholders
- heap-heavy public interfaces
- feature creep beyond MVP
- inventing new contracts that contradict `docs/`
