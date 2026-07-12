# AHSMA Phase 3: Speed-First Persistent Index

This phase removes block-summary construction from the per-token critical path.

Implemented:
- persistent block summaries;
- incremental update of the final/new blocks;
- route reuse for adjacent decoding steps;
- separate timings for index build, fresh routing, reused routing, sparse attention and dense attention;
- directly runnable tests.

Run:

```bash
python -m pytest -q
python benchmark_speed.py --tokens 32768 --head-dim 128 --device cpu
```

CUDA remains a PyTorch reference. The intended llama.cpp implementation must avoid
`index_select`/gather materialization and use a fused block-list attention kernel.
