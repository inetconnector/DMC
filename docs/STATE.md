# STATE

This file is the concise target summary. The live development log is
`../state.md`.

## Target summary
The DMC reference stack is in place.
See `docs/TARGET_STATE.md` for the concise capability target.
See `docs/VALIDATION.md` for the measurement order and checks.

## What is now available
- Python DMC reference implementation
- C++ DMC reference implementation
- Exact attention oracle for full and selected token sets
- Deterministic multiresolution span selection
- Active Python and C++ tests
- Android runtime integration using the shared C++ DMC selector
- Canonical Android token history with a 16K physical KV window and the
  model-trained logical context limit
- Native DMC self-test plus an APK marker gate that prevents publishing a
  non-DMC ARM64 build
- Unlimited-by-default Android generation with explicit API limits preserved
- DMC generation continuation when the 16K physical KV window fills, instead
  of silently truncating output at 512 tokens or at the KV boundary
- Local `llama.cpp` LAN instance with `dmc-local` alias
- Runtime presets for 32K, 64K, and experimental 128K and 256K context
- Automatic fallback from the largest working runtime preset down to 32K
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
  128K and 256K are experimental. The launcher now tries the largest working
  preset first and falls back automatically.
- Speed is important, but it follows long-context capability and quality.

## Next steps
- Install the latest DMC APK and verify model load, a normal streamed turn, and
  a context-pressure rebuild on the connected Samsung device.
- Add the equivalent runtime adapter to the Windows LAN launcher; it still uses
  standard dense llama.cpp context today.
- Extend validation coverage and add optional backend adapter layers.
- Keep the public wording aligned with the active DMC code.
- Keep the Continue integration aligned with the active local runtime.
- Prepare publication review material.

Pinned upstream:
ggml-org/llama.cpp
Commit: e3546c7948e3af463d0b401e6421d5a4c2faf565
