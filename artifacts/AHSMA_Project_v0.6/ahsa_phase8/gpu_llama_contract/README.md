# Phase 8 - GPU llama.cpp Integration Contract

This package binds the phase-8 KV bridge to the CUDA selected-attention
reference in a llama.cpp-shaped adapter.

## What it does

- wraps route selection in a contract object
- exposes CPU and GPU exact attention entry points
- validates GPU output against the CPU oracle
- measures the selection, CPU, and GPU paths together

## What it is not

- it is not the upstream `llama.cpp` tree
- it is not the final fused production kernel
- it is not a training implementation

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_COMPILER=...
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

On this machine the contract was validated on an RTX 3080 Laptop GPU with
16 GiB VRAM.

## Expected behavior

The GPU path should match the CPU oracle on both dense and selected-token
routes, and the batched GPU path should become faster than CPU for larger query
counts.

## Verified Benchmarks

Measured on this machine:

- `32768 tokens`, `64 head_dim`, `32 route_dim`, `128 queries`, `5 repeats`
- `cpu_selected_ms`: `27.355`
- `gpu_selected_ms`: `4.386`
- `gpu_selected_kernel_ms`: `0.576`
- `gpu_span_ms`: `4.210`
- `cpu_vs_gpu_max_abs_diff`: `0.000`
- `gpu_vs_span_max_abs_diff`: `0.000`
- `dense_vs_gpu_full_max_abs_diff`: `0.000`

This gives the contract a real GPU-backed route, not just a CPU-shaped wrapper.

Follow-up runs make the batch-size trade-off explicit:

- `1 query`: `cpu_selected_ms = 0.470`, `gpu_selected_ms = 6.984`
- `128 queries`: `cpu_selected_ms = 47.941`, `gpu_selected_ms = 5.031`

That is exactly what we want from a real GPU path: small requests can still be
cheaper on CPU, but batched attention moves decisively to the GPU while staying
bit-exact against the CPU oracle.
