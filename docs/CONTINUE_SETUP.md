# Continue Setup

This repository can drive Continue against the local `llama.cpp` instance.

## What this gives you

- Chat inside VS Code or JetBrains
- Edit and apply flows against the local model
- Inline autocomplete using the same local backend

## Local config

Continue reads its local config from:

- `C:\Users\frede\.continue\config.yaml`

Use the helper script in this repo to write the config:

```powershell
.\scripts\windows\setup-continue.ps1
```

That config points Continue at the active LAN instance:

- `http://127.0.0.1:8080/v1`
- model name: `dmc-local`

## Notes

- Start the local `llama.cpp` server first.
- The same model is used for chat and autocomplete.
- The config enables tool use for agent-style workflows; if your Continue
  version or model does not support it cleanly, switch off agent mode or use a
  model with native tool calling.
- For long coding sessions, start the server with `-Use64KContext`; use
  `-Use128KContext` only for stress testing.
- Use `-Use256KContext` only for very large-memory stress tests, and prefer
  `-DryRun` first if you only want to verify the launcher arguments.

## Where to start

1. Start the LAN server with `scripts/windows/start-llama-lan.ps1`.
2. Run `scripts/windows/setup-continue.ps1`.
3. Open Continue in VS Code with `Ctrl+L` or in JetBrains with `Ctrl+J`.
