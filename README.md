# DMC

Deterministic Multiresolution Context (DMC) is the active clean-room target for
this repository.

## Current State

- The project notes live in `docs/`.
- The active design notes describe a deterministic multiresolution context
  path, exact attention over selected spans, and conservative integration
  boundaries.
- The design emphasizes ultra-long context with minimal algorithmic
  complexity.
- The intended outcome is strong long-context performance with high reasoning
  quality.
- Nothing in this repository should be read as a patent clearance opinion or
  as a guarantee that the implementation is free to publish in every
  jurisdiction.

## Start Here

1. Read `docs/STATE.md` for the active status and next steps.
2. Read `docs/TARGET_STATE.md` for the concise capability target.
3. Read `docs/LPS.md` for the project priorities.
4. Read `docs/VALIDATION.md` for the measurement order and checks.
5. Read `docs/PUBLICATION_READINESS.md` for the release posture and checklist.
6. Read `docs/ARCHITECTURE.md` for the clean-room target architecture.
7. Use `dmc/` for the Python reference implementation and `tests/` for the
   active Python validation suite.
8. Use `cpp/` for the C++ reference implementation and build contract.
9. Read `docs/TOOLCHAIN_SETUP.md` for the exact Windows setup needed to run
   `pytest` and the C++ build.
10. The root `pytest` config is scoped to `tests/`, so `python -m pytest -q`
    only runs the active suite.
11. Read `docs/INSTANCE.md` for the current runtime instance definition.
12. Read `docs/LLAMA_CPP_LAN.md` if you want the local llama.cpp web UI on
    your laptop and phone.
13. Use `scripts/windows/prepare-dev.ps1` to validate and run the local setup.

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.
- Keep the release narrative conservative and technically defensible.
