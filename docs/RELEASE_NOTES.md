# Release Notes

## Public release summary

This repository now publishes the DMC reference stack as the active target.
The historical AHSMA artifact archive has been removed from the main working
tree and replaced by a cleaner, publication-oriented layout.

### What changed

- Added the DMC Python reference implementation and regression tests.
- Added the DMC C++ reference implementation and build contract.
- Defined the active runtime instance in `docs/INSTANCE.md`.
- Added the local `llama.cpp` LAN launch path for laptop and phone use.
- Added `Continue` wiring for local editor-based coding workflows.
- Added 64K and experimental 128K runtime context presets.
- Added publication-readiness guidance and conservative legal wording.
- Added a repository license and release metadata.
- Removed the imported historical archive from the public project surface.

### Verification

- Python tests pass.
- C++ reference tests pass.
- The local `llama.cpp` LAN instance responds under the `dmc-local` alias.
- The 64K preset boots successfully on this machine.
- The experimental 128K preset boots successfully on this machine.

### Release posture

- This repository is publication-oriented, not legally cleared for all
  jurisdictions.
- No file in the repo claims patent clearance, freedom to operate, or a
  guarantee that publication is risk-free.
