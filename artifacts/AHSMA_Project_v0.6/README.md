# AHSMA Project v0.6 Artifact Snapshot

This directory is a cleaned import of the important source and validation
materials from `AHSMA_Project_v0.6.zip`.

## Included

- `PROJECT_STATUS_v0.3.md` through `PROJECT_STATUS_v0.6.md`
- `ahsa_phase1/README_PHASE1.txt`
- `ahsa_phase2/code/`: PyTorch correctness reference, benchmark, tests
- `ahsa_phase3/code/`: persistent-index speed reference, benchmark, tests
- `ahsa_phase4/cpp_reference/`: backend-neutral C++ reference and tests
- `ahsa_phase5/llama_cpp_integration/`: `llama.cpp` integration scaffold
- `ahsa_phase6/llama_cpp_patchset/`: ordered patchset and contract tests
- `ahsa_phase7/kv_index_lifecycle/`: KV lifecycle reference and tests
- `ahsa_phase8/kv_cache_bridge/`: bridge-oriented graph-input reference and tests
- `ahsa_phase8/cpu_selected_attention/`: CPU exact attention oracle and tests
- `ahsa_phase8/gpu_selected_attention/`: CUDA exact selected-attention reference and tests
- `ahsa_phase8/gpu_llama_contract/`: llama.cpp-shaped GPU integration contract
- `ahsa_phase8/llama_cpp_contract/`: llama.cpp-shaped integration contract
- `ahsa_phase8/README.md`: Phase 8 overview

## Omitted

- DOCX/PDF design documents
- rendered PNG/PDF review artifacts
- `__pycache__`, `.pytest_cache`, and build directories
- compiled objects such as `llama-ahsma.o`

## Notes

- The Python references require `torch` and `pytest`.
- The C++ references build with CMake and a C++17 compiler.
- The current integration target remains `ggml-org/llama.cpp` at
  `e3546c7948e3af463d0b401e6421d5a4c2faf565`.
