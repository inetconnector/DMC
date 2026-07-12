# Phase 8 - CPU Selected-Block Attention

This package provides a pure C++ reference for exact attention over selected
tokens and selected spans.

## What it verifies

- dense attention
- exact selected-token attention
- span-to-token expansion
- numerical agreement between full selection and dense attention

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

## Why it matters

Phase 8 needs a CPU oracle before any llama.cpp kernel work. This reference is
the correctness check that later fused or backend-specific implementations must
match.
