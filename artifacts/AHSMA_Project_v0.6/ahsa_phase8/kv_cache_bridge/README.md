# Phase 8 - KV Cache Bridge

This reference package turns the Phase 7 lifecycle model into a bridge-oriented
shape for the next llama.cpp integration gate.

## What it demonstrates

- logical-to-physical token mapping via `TokenRef`
- causal route selection for one sequence at a time
- route reuse across decode steps
- contiguous selected spans for graph input materialization
- cache invalidation on sequence mutation and clear

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

## Why this matters

Phase 8 is the adapter layer that makes the validated lifecycle usable as graph
input for a llama.cpp integration. The production integration will still need to
wire this logic into `llama_kv_cache`, but the selection and span logic now has
an executable reference shape.
