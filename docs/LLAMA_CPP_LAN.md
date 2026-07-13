# llama.cpp LAN Setup

This repository keeps the DMC reference code separate from the local model
runtime. If you want a laptop-hosted web UI that your phone can reach on the
same network, use the official `llama.cpp` server binary and the Qwen GGUF
model below.

See `docs/INSTANCE.md` for the active LAN instance definition.

## Current active model

The launcher prefers the already installed local Qwythos GGUF blob first:

- `hf.co/empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF:Q5_K_M`

Why this one:

- It is already present locally on this machine.
- Ollama metadata reports a `context_length` of `1048576`.
- It is the strongest local context candidate currently available in this repo
  setup.

If that blob is not present, the launcher falls back to:

- `Qwen/Qwen2.5-7B-Instruct-GGUF:Q4_K_M`

Why the fallback exists:

- It is the 7B instruction-tuned GGUF variant from the official Qwen repo.
- The model card documents long-context support up to 128K tokens.
- It is small enough to be practical on a 16 GB VRAM laptop GPU.
- The repository license is Apache 2.0.

## Existing local Ollama models

This machine already has these local Ollama entries:

- `qwen2.5-coder:7b`
- `qwen2.5-coder:14b`
- `gpt-oss:20b`
- `gemma4:e2b`
- `hf.co/empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF:Q5_K_M`

If you want to try the fallback model or override the local candidate, pass the
desired source as `-ModelId` to `scripts/windows/start-llama-lan.ps1`.

## Recommended runtime

Use the official `llama.cpp` Windows CUDA binary release and its matching CUDA
runtime bundle.

The server exposes a built-in web UI and an OpenAI-compatible API.

## Start script

From the repo root:

```powershell
.\scripts\windows\start-llama-lan.ps1
```

The script:

- downloads the latest official Windows CUDA `llama.cpp` release if needed
- stores the Hugging Face cache under `runtime\huggingface`
- starts `llama-server.exe`
- binds the server to `0.0.0.0`
- assigns a neutral local alias instead of exposing the blob path
- disables reasoning output by default so the UI stays direct
- uses an already installed local Ollama GGUF first when one is available
- falls back to the Qwen GGUF download only if no local GGUF is found

For a persistent background run on Windows, start the script with
`-StayAlive`, or register it in Task Scheduler as `AHSMA-LlamaLAN`.

Example:

```powershell
.\scripts\windows\start-llama-lan.ps1 -StayAlive
```

## Phone access

After the server starts, open:

```text
http://<laptop-ip>:8080/
```

Replace `<laptop-ip>` with the laptop's LAN IPv4 address. The script prints the
addresses it finds.

## Long-context smoke test

To make the server verify a long prompt after startup:

```powershell
.\scripts\windows\start-llama-lan.ps1 -SmokeTest
```

The smoke test sends a long prompt through the local OpenAI-compatible endpoint
and checks that the server answers. Increase `-SmokeTestTokens` if you want a
heavier context check.

## Practical note

For a 16 GB VRAM laptop GPU, 32K context is a reasonable first target. If your
driver, GPU, or thermal limits are tighter, lower `-ContextSize` to 16384 and
try again.
