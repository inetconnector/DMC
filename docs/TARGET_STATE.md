# Target State

DMC is the active clean-room target for this repository.

## What It Should Deliver

- Ultra-long context handling with minimal algorithmic complexity.
- Strong reasoning quality on long-context tasks.
- High utility for models that need to keep more of the conversation in working memory.
- Deterministic multiresolution context selection.
- Exact attention over selected spans.
- A dense-default path that remains available for baseline comparison.
- Long-context handling through fixed replay levels, not query-driven retrieval.
- Reproducible validation in Python and C++.

## What It Deliberately Avoids

- Learned routing.
- Query-dependent top-k block retrieval.
- Semantic ANN or cluster indexes over KV data.
- Dynamic cache pruning.
- Runtime content-based prefetching.
- Claims that the design is patent cleared or free to publish everywhere.

## Publication Posture

The target is publication-oriented, but publication still requires final legal
review. The repository should describe the technical design clearly and
conservatively, without overstating patent status or commercial readiness.

## Success Criteria

- Exact attention matches dense attention when all tokens are selected.
- Selection is deterministic for the same logical state.
- Long-context quality remains strong enough to justify the added complexity.
- The design scales through hierarchy and span reuse rather than ad hoc
  retrieval heuristics or learned routing.
- The implementation stays simple enough to reason about, validate, and port.
- Documentation and code naming remain aligned with the DMC target.
