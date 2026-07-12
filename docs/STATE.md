# STATE

## Current phase
Phase 8 reference stack is in place.

## What is now available
- Phase 7 validated KV lifecycle reference
- Phase 8 bridge-oriented KV cache reference with graph-input spans
- Phase 8 CPU exact attention oracle with dense-vs-selected validation
- Phase 8 CUDA selected-attention reference validated on RTX 3080 Laptop GPU
- Phase 8 GPU llama.cpp-shaped contract validated on RTX 3080 Laptop GPU
- Phase 8 llama.cpp-shaped integration contract
- Phase 6 AHSMA patchset now applies cleanly to a fresh pinned `llama.cpp` clone
- Upstream `llama.cpp` now builds with live AHSMA cache-refresh plumbing that
  reads real KV data and rebuilds the index from the cache
- Upstream `llama.cpp` attention graph now consumes a live per-layer AHSMA
  selection mask during graph construction

## Performance note
- GPU selected attention is exact, but it is only faster than CPU once the
  request is large enough to amortize kernel-launch overhead
- On this machine, `1 query` still favors CPU, while `128 queries` strongly
  favors GPU

## Next phase
Phase 8 integration target:
- Hook KV lifecycle into `llama_kv_cache`
- CPU Selected-Block Attention reference
- CUDA Selected-Block Attention reference
- Numerical validation against dense attention
- Wire the live AHSMA route into the final selected-block attention path

Pinned upstream:
ggml-org/llama.cpp
Commit: e3546c7948e3af463d0b401e6421d5a4c2faf565
