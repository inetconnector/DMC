# AHSMA

Adaptive Hierarchical Sparse Memory Attention (AHSMA) is a speed-first long-context
attention architecture targeting llama.cpp integration.

## Goals
- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.

See `docs/STATE.md` for current progress.
