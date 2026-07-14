# Instance Definition

This repository uses the term `instance` for one concrete runtime setup:

- one local model source
- one server binary
- one alias presented in the UI and API
- one context size
- one port
- one network exposure profile

## Current LAN instance

- Instance name: `dmc-local`
- Runtime: `llama.cpp` server
- Host: `0.0.0.0`
- Port: `8080`
- Context: `32768`
- Reasoning mode: `off`
- Default model source: `hf.co/empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF:Q5_K_M`
- Default source note: Ollama metadata reports a `context_length` of
  `1048576` for this blob
- Fallback model source: `Qwen/Qwen2.5-7B-Instruct-GGUF:Q4_K_M`
- Larger-context variant: start the launcher with `-Use64KContext`

## Why this matters

The instance definition keeps the assistant identity separate from the model
blob name. That makes the LAN UI clearer and prevents the API from exposing an
opaque local file path as the main model label.

The runtime still exposes `dmc-local` as the public model identity, even when
it loads the Qwythos GGUF blob underneath.

## Operational rule

If the runtime changes, update the instance definition first, then update the
launcher script to match it.
