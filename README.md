# DMC

Deterministic Multiresolution Context (DMC) is the active clean-room target for
this repository.

In plain terms: DMC is a design for a local AI system that can keep much more
conversation history in working memory, stay deterministic, and remain easier
to validate than a more ad hoc retrieval system.

## Current State

- The project notes live in `docs/`.
- The code in `dmc/` and `cpp/` is the reference implementation.
- The local runtime in `scripts/windows/` runs `llama.cpp` on your own
  machine.
- The design focuses on long context first, quality second, speed third.
- The repository is licensed under MIT.
- Nothing in this repository should be read as a patent clearance opinion or
  as a guarantee that the implementation is free to publish in every
  jurisdiction.

## What You Get

- A local AI stack that can run on your own computer.
- Much larger practical conversation memory than a plain short-context setup.
- A clean structure for code, docs, tests, and a browser-accessible model UI.
- Optional editor integration through Continue.
- A phone-friendly LAN interface if you want to use the model from the same
  network.

## Fast Start

If you just want the chat UI, run `run.bat` from the repository root.
If you want phone access on the same Wi-Fi, run `run-phone.bat` from the
repository root.

What it does:

1. Starts the local `llama.cpp` server.
2. Waits for the local API to be ready.
3. Tries the largest context that starts successfully.
4. Opens the browser UI at `http://127.0.0.1:8080/`.

`run-phone.bat` also sets up the Windows firewall for the local network, then
starts the same server.
It prints the primary LAN IPv4 address that the phone can use.

If you want to check the launch without starting the server, use:

```powershell
.\scripts\windows\start-llama-lan.ps1 -DryRun
```

## Why It Matters

- You can keep more project context without constantly repeating yourself.
- Long chats, coding sessions, and large documents become easier to handle.
- The local setup gives you control over the model, the context size, and the
  runtime.
- The project is organized so the public documentation matches the actual
  implementation.

## How It Works

1. The model runs locally in `llama.cpp`.
2. The launcher starts a model instance with a chosen context size.
3. DMC selects the important spans of history in a deterministic way.
4. Exact attention is applied over the selected tokens.
5. The answer is returned through the local API, the browser UI, or an editor
   integration.

## Start Here

1. Read `docs/STATE.md` for the current status and next steps.
2. Read `docs/INSTANCE.md` for the active runtime setup.
3. Read `docs/LLAMA_CPP_LAN.md` if you want the browser UI on your laptop or
   phone.
4. Read `docs/CONTINUE_SETUP.md` if you want editor integration in VS Code or
   JetBrains.
5. Use `run.bat` for the fastest out-of-the-box start.
6. Use `run-phone.bat` if you want to reach the chat UI from a phone on the
   same network.
7. Use `scripts/windows/start-llama-lan.ps1` if you want to pass explicit
   runtime options.
8. Use `-Use64KContext`, `-Use128KContext`, or `-Use256KContext` only when you
   want to force a specific preset.
9. Read `docs/VALIDATION.md` if you want the test and benchmark order.
10. Read `docs/PUBLICATION_READINESS.md` for the release posture and checklist.
11. Use `dmc/` and `cpp/` if you want the reference implementations.
12. Use `scripts/windows/prepare-dev.ps1` to validate and run the local setup.

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.
- Keep the release narrative conservative and technically defensible.
