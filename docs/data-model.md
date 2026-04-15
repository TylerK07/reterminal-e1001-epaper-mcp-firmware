# Message and Data Model Spec

## Principles

- distinct models for runtime, persistence, and wire payloads
- compact bounded structs
- redaction by default
- stable contracts

## Core config model

Top-level persisted config includes:
- schema_version
- device_name
- auth_token
- wifi
- network
- sleep
- provisioning
- display

## Runtime snapshot includes

- device identity
- battery status
- Wi-Fi status
- display status
- provisioning status

## Render models

- text render request
- bitmap render request
- bounded layout request
- render result
- last render record

## Job model

Useful even if execution is mostly synchronous at first:
- job id
- kind
- state
- timestamps
- error info

## Wire model conventions

- external enums serialize as strings
- absent optional values use `null`
- secrets never returned raw

## Redacted config view

Must never expose:
- Wi-Fi password
- auth token
- future secrets/API keys
