# Patch Checklist

- [ ] Add `LLM_FUSED_OP_AHSMA_ROUTE` beside existing fused op identifiers.
- [ ] Register `llama-ahsma.cpp` in `src/CMakeLists.txt`.
- [ ] Add disabled-by-default runtime parameters.
- [ ] Construct and destroy `llama_ahsma_index` with the llama context.
- [ ] Invalidate index state on KV clear, sequence removal, shifts and state load.
- [ ] Update summaries only after successful KV writes.
- [ ] Preserve unified/multi-stream sequence semantics.
- [ ] Add selected logical-position to physical-cell mapping.
- [ ] Add graph-reuse checks for selected-ID tensor shapes.
- [ ] Add CPU numerical reference.
- [ ] Add unit tests for cache shifts, partial blocks, sequence copy/removal and route reuse.
- [ ] Add CUDA kernel only after CPU correctness passes.
- [ ] Keep dense behavior bit-for-bit unchanged when AHSMA is disabled.
