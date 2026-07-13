# Publication Readiness

This repository can be prepared for publication, but no file in the repo can
guarantee patent clearance, freedom to operate, or permission to publish in all
jurisdictions.

## Release posture

- Follow LPS: long context first, quality second, speed third.
- Keep non-current material outside the public narrative.
- Treat `docs/` as the public-facing technical narrative.
- Use conservative wording for every performance, quality, and legal claim.
- Do not present implementation details as a patent clearance argument.

## Patent-sensitive boundaries

The active DMC reference is intentionally written to avoid several common sparse
attention risk patterns:

- no learned router;
- no query-dependent top-k block retrieval;
- no semantic ANN or cluster index over KV data;
- no dynamic cache pruning;
- no runtime prefetch based on content similarity;
- no block-table-style paging as the core reference model;
- no claim that the design is patent clear.

## Technical publication checklist

Before publishing, verify:

- The public README matches the actual active architecture.
- Non-current material is clearly separated from the public narrative.
- Benchmark numbers are separated from claims about model quality.
- No document says the project is "patent safe" or "FTO cleared".
- No document claims a legal right to ship without jurisdiction-specific review.
- The final implementation is consistent with the target architecture naming.

## Recommended wording

Use:

- "clean-room design target"
- "reference implementation"
- "reference material"
- "publication-ready after legal review"

Avoid:

- "patent-free"
- "FTO cleared"
- "guaranteed publishable"
- "cannot infringe"
