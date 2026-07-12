# Architecture

Pipeline

Query
 -> Routing projection
 -> Persistent block index
 -> Top-k block selection
 -> Local + global safety path
 -> Exact attention over selected blocks
 -> Output

Core modules:
- Router
- Persistent KV index
- Route cache
- Selected-block attention
- GGML operator
- CUDA backend
