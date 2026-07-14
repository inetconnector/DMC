# STATE

## Current status
The DMC reference stack is in place.
See `docs/TARGET_STATE.md` for the concise capability target.
See `docs/VALIDATION.md` for the measurement order and checks.

## What is now available
- Python DMC reference implementation
- C++ DMC reference implementation
- Exact attention oracle for full and selected token sets
- Deterministic multiresolution span selection
- Active Python and C++ tests
- Local `llama.cpp` LAN instance with `dmc-local` alias
- Runtime presets for 32K, 64K, and experimental 128K and 256K context
- Continue setup for local editor integration
- A target direction focused on both long-context capacity and quality
- An explicit LPS priority order: long context, quality, speed

## Release posture
- The repository is not presented as patent cleared.
- Publication requires final wording review and jurisdiction-specific legal review.

## Performance note
- Exact selected attention is fastest only after enough work amortizes
  overhead.
- Tiny requests can still favor CPU, while larger batches can favor GPU.
- 32K is the conservative baseline, 64K is the practical stretch target, and
  128K and 256K are experimental.
- Speed is important, but it follows long-context capability and quality.

## Next steps
- Extend validation coverage and add optional adapter layers.
- Keep the public wording aligned with the active DMC code.
- Keep the Continue integration aligned with the active local runtime.
- Prepare publication review material.

Pinned upstream:
ggml-org/llama.cpp
Commit: e3546c7948e3af463d0b401e6421d5a4c2faf565
