# Phase 8 - GPU Selected Attention

This package is a CUDA reference path for exact selected-block attention.

## What it does

- runs attention on the GPU
- accepts token selections from the phase-8 bridge
- checks GPU output against the CPU oracle
- reports kernel timing and numerical error

## What it is not

- it is not the final fused llama.cpp kernel
- it is not optimized for maximum throughput
- it is not a training implementation

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -C Debug --output-on-failure
```

If CMake cannot find `nvcc`, pass the full compiler path with
`-DCMAKE_CUDA_COMPILER=...`.

Use an NVIDIA GPU with CUDA support. The current machine has an RTX 3080 Laptop
GPU with 16 GiB VRAM, which is enough for the reference sizes used here.

## Verified Benchmarks

Measured on this machine:

- `32768 tokens`, `64 head_dim`, `32 route_dim`, `128 queries`, `5 repeats`
- `cpu_selected_ms`: `21.655`
- `gpu_total_ms`: `3.017`
- `gpu_kernel_ms`: `0.541`
- `cpu_vs_gpu_max_abs_diff`: `0.000`
- `dense_vs_gpu_full_max_abs_diff`: `0.000`

This is a reference implementation, not the final fused llama.cpp kernel, but it
does prove the GPU path is real, numerically correct, and materially faster for
batched queries.

Recent follow-up runs on the same RTX 3080 Laptop GPU show the expected
trade-off:

- `1 query`: `cpu_selected_ms = 0.470`, `gpu_selected_ms = 6.984`
- `128 queries`: `cpu_selected_ms = 47.941`, `gpu_selected_ms = 5.031`

So the GPU path is not a universal win for tiny batches, but it does become
meaningfully faster once there is enough work to amortize kernel launch and copy
overhead.
