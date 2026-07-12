# Phase 6 — llama.cpp Compile-Plumbing Patchset

Baseline: `ggml-org/llama.cpp@e3546c7948e3af463d0b401e6421d5a4c2faf565`.

This patchset performs only the first upstream-facing integration checkpoint:

- registers the new source file;
- adds an AHSMA fused-op identifier;
- adds internal context parameters;
- constructs an optional persistent index;
- keeps normal dense execution unchanged.

Enablement is temporarily controlled by `LLAMA_AHSMA=1` to avoid a public ABI
change before the implementation is useful. Even when enabled, this checkpoint
does **not** replace attention or claim a speedup.

## Apply

```bash
./apply_to_llama_cpp.sh /path/to/llama.cpp
cmake -S /path/to/llama.cpp -B /path/to/llama.cpp/build -DGGML_NATIVE=ON
cmake --build /path/to/llama.cpp/build -j
```

## Validation status

The added AHSMA translation unit compiles independently, patch-contract tests
pass, and the patchset now applies cleanly to a fresh pinned `llama.cpp` clone.
The upstream checkout at `e3546c7948e3af463d0b401e6421d5a4c2faf565` also
builds with the compile-plumbing changes in place.
