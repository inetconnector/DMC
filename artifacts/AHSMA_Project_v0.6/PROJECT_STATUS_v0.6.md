# AHSMA Project Status v0.6

Phase 7 implements and validates a sequence-aware KV index lifecycle reference.

Validated:
- multiple logical sequences;
- logical position to physical KV-cell mapping;
- local/global/retrieved selection;
- route reuse;
- invalidation on remove/copy/keep/shift/divide/clear;
- C++17 build and CTest pass.

Still blocked by runtime networking:
- complete upstream llama.cpp checkout and full patched build.

Next implementation gate:
- adapt llama_kv_cache cell/slot metadata into the validated TokenRef interface;
- update routing summaries after successful K-cache writes;
- build selected logical-to-physical spans for graph input.
