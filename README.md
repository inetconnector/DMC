# AHSMA

Adaptive Hierarchical Sparse Memory Attention (AHSMA) is a speed-first long-context
attention architecture targeting `llama.cpp` integration.

## Current State

- The project notes live in `docs/`.
- The cleaned artifact snapshot from `AHSMA_Project_v0.6.zip` now lives in
  `artifacts/AHSMA_Project_v0.6/`.
- Phase 7 is the latest validated reference stage; Phase 8 is the next integration gate.
- Phase 8 now has a bridge-oriented reference package, a CPU attention oracle,
  a CUDA selected-attention reference, and a GPU llama.cpp-shaped integration contract.
- The upstream `llama.cpp` clone in `upstream/llama.cpp` now also has live
  AHSMA cache-refresh plumbing that reads real KV cache data.
- The upstream `llama.cpp` attention graph now also consumes a live per-layer
  AHSMA selection mask, so the route path is wired into graph construction.

## Start Here

1. Read `docs/STATE.md` for the current phase and next gate.
2. Open `artifacts/AHSMA_Project_v0.6/README.md` for the imported artifact map.
3. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase4/cpp_reference` and
   `artifacts/AHSMA_Project_v0.6/ahsa_phase7/kv_index_lifecycle` for the current
   C++ reference implementations.
4. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase8/kv_cache_bridge` for the bridge
   layer that turns selection into graph-friendly spans.
5. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase8/cpu_selected_attention` for the
   exact CPU attention oracle.
6. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase8/gpu_selected_attention` for the
   CUDA reference path that runs the selected attention on GPU.
7. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase8/gpu_llama_contract` for the
   llama.cpp-shaped GPU contract that ties bridge and CUDA attention together.
8. Use `artifacts/AHSMA_Project_v0.6/ahsa_phase8/llama_cpp_contract` for the
   CPU adapter layer that ties bridge and oracle together in a llama.cpp-shaped form.

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.

Python reference scripts under `artifacts/AHSMA_Project_v0.6/ahsa_phase2/code`
and `artifacts/AHSMA_Project_v0.6/ahsa_phase3/code` require `torch` and `pytest`
to run locally.
