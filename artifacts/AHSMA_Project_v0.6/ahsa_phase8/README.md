# Phase 8

Phase 8 is the bridge between validated lifecycle handling and llama.cpp
integration.

## Packages

- `kv_cache_bridge/`: token mapping, route reuse, and selected spans
- `cpu_selected_attention/`: dense and exact selected attention reference
- `gpu_selected_attention/`: CUDA exact selected attention reference
- `gpu_llama_contract/`: llama.cpp-shaped GPU adapter that wires the bridge to CUDA
- `llama_cpp_contract/`: llama.cpp-shaped CPU adapter that keeps the reference oracle

## Expected outcome

By the end of this phase, the project should have:

- a route-selection adapter that can feed graph inputs
- a CPU attention oracle that proves the selected-token path is exact
- a CUDA path that proves the selected-token path on GPU
- a clear next step into `llama_kv_cache` integration

The GPU path is exact, but it is not guaranteed to beat CPU for tiny batches.
The validated benchmarks show the usual crossover behavior: tiny requests can
still be CPU-faster, while larger batches move decisively to GPU.
