# Phase 8 - llama.cpp Integration Contract

This package is a standalone integration contract for the phase-8 bridge and CPU
oracle.

## What it does

- wraps the KV bridge in a llama.cpp-shaped adapter
- exposes selected token IDs and selected spans
- evaluates exact attention through the CPU oracle
- reports dense-vs-selected numerical agreement

## What it is not

- it is not the upstream `llama.cpp` source tree
- it is not a production fused kernel
- it is not a patch application script

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

The contract is intentionally conservative so it can later map to the real
`llama_kv_cache` integration points without changing the validation logic.
