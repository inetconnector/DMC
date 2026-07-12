# Phase 7 — KV Index Lifecycle

This reference models the lifecycle obligations imposed by llama.cpp's KV cache:

- clear;
- sequence removal;
- sequence keep;
- sequence copy;
- position add/shift;
- position division;
- route-cache invalidation;
- partial blocks;
- multiple sequences sharing a physical cache.

It uses logical sequence/position metadata and returns physical KV cell IDs. This
is the key distinction required for a correct llama.cpp integration.

The implementation is intentionally independent of ggml backend storage. The next
patch checkpoint will adapt `llama_kv_cache::slot_info` and cell metadata into
`TokenRef` records and update summaries after KV writes.

On multi-config generators such as Visual Studio, run tests with:

```bash
ctest --test-dir build -C Debug --output-on-failure
```
