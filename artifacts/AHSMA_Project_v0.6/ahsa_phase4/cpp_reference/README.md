# Phase 4 C++ Block-Index Reference

Backend-neutral C++17 reference for persistent block summaries, top-k remote block
selection, local/global safety spans and route reuse.

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/ahsma_bench 32768 128 100
```

This is not yet linked to llama.cpp and does not include the fused attention kernel.
It establishes the data structures and routing behavior needed before integration.

On multi-config generators such as Visual Studio, use:

```bash
ctest --test-dir build -C Debug --output-on-failure
```
