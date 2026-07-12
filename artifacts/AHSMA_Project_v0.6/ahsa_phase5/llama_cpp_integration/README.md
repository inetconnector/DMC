# Phase 5 — Online llama.cpp Baseline and Integration Scaffold

The direct repository archive could not be downloaded by the execution runtime.
Current upstream source files were inspected through the GitHub connector and
pinned to a concrete commit. This folder contains an integration scaffold and a
strict checkpoint plan, not a claim that the complete upstream project has been
vendored.

The two C++ files are deliberately conservative. They define ownership and API
boundaries and provide a safe local/global fallback. Backend tensor access,
persistent summaries, GPU top-k and fused selected-block attention remain the
next implementation checkpoints.
