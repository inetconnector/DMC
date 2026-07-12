# AHSMA Speed-First Reference Prototype

This is a correctness-oriented PyTorch prototype for block-sparse long-context attention.
It is not a production kernel and does not yet provide trained routing weights.

## Features

- 64-token block summaries
- local and global safety tokens
- query-dependent block retrieval
- optional adaptive block budget
- exact softmax attention over selected tokens
- dense-versus-sparse microbenchmark
- unit tests

## Run

```bash
python benchmark_ahsma.py --tokens 32768 --head-dim 128 --device cpu
pytest -q
```

For CUDA:

```bash
python benchmark_ahsma.py --tokens 131072 --head-dim 128 --device cuda --dtype float16
```

The reference selector recomputes summaries on every call. A real llama.cpp backend must persist
block summaries, update them incrementally, reuse routes, and fuse selected-block attention.
