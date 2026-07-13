# Architecture

Target architecture

LPS order:
Long context first, quality second, speed third.

Query
 -> Local exact attention window
 -> Deterministic multiresolution context selection
 -> Fixed replay spans from older levels
 -> Exact attention over selected spans
 -> Output

Core modules:
- `dmc/` Python reference implementation
- `cpp/` C++ reference implementation
- Small deterministic span planner
- Exact selected-span attention
- Deterministic multiresolution hierarchy
- Fixed span builder
- Minimal adapter layer for future backend integration

Notes:
- This is the clean-room target, not a legal clearance statement.
- The architecture is optimized for long-context capability and quality before
  speed tuning.
- Publication should use the target terminology above, not patent claims.
