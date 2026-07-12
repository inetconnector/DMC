# llama.cpp Integration Plan — Phase 5

Upstream baseline: `ggml-org/llama.cpp` commit
`e3546c7948e3af463d0b401e6421d5a4c2faf565`.

## Why this integration point

The inspected upstream already separates graph construction from specialized KV
cache implementations and exposes `get_k_storage()`, `get_k()`, and `get_v()`.
It also defines a fused-operation category for a Lightning Indexer. AHSMA should
therefore be introduced as:

1. an optional persistent index owned by the llama context;
2. graph input tensors containing selected block/token IDs;
3. a new fused selected-block attention operation;
4. model-native routing tensors loaded only for compatible GGUF models.

It should not initially replace `llama_kv_cache`.

## Checkpoint A — compile-only plumbing

Add:

- `src/llama-ahsma.h`
- `src/llama-ahsma.cpp`
- source entry in `src/CMakeLists.txt`
- `LLM_FUSED_OP_AHSMA_ROUTE`
- optional `llama_ahsma_params` in context parameters
- an index pointer in the runtime context

Behavior remains dense unless explicitly enabled.

## Checkpoint B — local/global reference selection

Create graph input tensors for selected IDs. Route only the local window and
global/sink tokens. This is a correctness test of logical-to-physical KV mapping,
not a performance or quality mode.

## Checkpoint C — persistent summaries

After KV writes, update block summaries from canonical K storage. The update must
recompute only:

- the previous incomplete block;
- newly completed blocks;
- the current incomplete block.

No full-cache repooling is allowed in steady-state decode.

## Checkpoint D — flat GPU router

Keep routing summaries on the backend. Project one routing query per head group,
score block summaries, perform bounded top-k, and return sorted block IDs without
CPU synchronization.

## Checkpoint E — fused selected-block attention

The final kernel accepts native KV-cache descriptors and selected block spans.
It performs QK scoring, online softmax, and V accumulation directly. It must not
materialize gathered K/V tensors.

## Checkpoint F — model-native GGUF

Introduce versioned metadata and route tensors. Standard GGUF models may use only
the explicitly experimental inference-only selector; production quality requires
a router-trained checkpoint.

## Non-negotiable benchmark reporting

Report separately:

- index update;
- routing projection;
- block scoring/top-k;
- route reuse hit rate;
- selected attention kernel;
- total layer time;
- end-to-end prefill and decode.

No kernel-only number may be presented as model speed.
