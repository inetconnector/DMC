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
5. Use `scripts/windows/start-llama-lan.ps1` to start the local server.
6. Use `-Use64KContext` for the practical larger-context preset.
7. Use `-Use128KContext` only as an experiment.
8. Read `docs/VALIDATION.md` if you want the test and benchmark order.
9. Read `docs/PUBLICATION_READINESS.md` for the release posture and checklist.
10. Use `dmc/` and `cpp/` if you want the reference implementations.
11. Use `scripts/windows/prepare-dev.ps1` to validate and run the local setup.

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.
- Keep the release narrative conservative and technically defensible.
