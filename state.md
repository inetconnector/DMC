# State

Last updated: 2026-07-21

This file is the live development log for the repository.
Update this file and `README.md` together whenever behavior changes.

## Snapshot

- Repository root: `C:\Users\frede\Projekte\AHSMA`
- Branch: `develop`
- Commit: use `git rev-parse --short HEAD` for the current revision; this file
  deliberately avoids embedding a hash that becomes stale in its own commit.
- Tracked state: Android dictation/SSE fixes, reproducible Web UI integration,
  native DMC runtime and continuation, DMC regression gates, and refreshed
  documentation are integrated on `develop`. Generated APK/AAB files remain in
  the ignored `publish/` directory and on the DiskStation.
- Active target: DMC (Deterministic Multiresolution Context)

## Verified Working

- The DMC reference stack exists in `dmc/` and `cpp/`.
- The Android native library directly includes `cpp/dmc_reference.hpp`; DMC is
  now part of the inference runtime rather than only a project/app name.
- Android retains canonical logical token history and uses a 16384-token
  physical llama.cpp KV window. For the current Gemma model, the reported
  logical window remains the model-trained 131072 tokens.
- Short turns remain dense. When the physical prompt budget is exceeded, DMC
  selects a global prefix, the recent 2048 tokens, and seven deterministic
  replay levels, sorts them causally, clears the physical KV cache, and
  rehydrates exact selected-token attention without recursive summarization.
- A selected KV state is reused incrementally across later turns and rebuilt
  only when the physical budget fills again; DMC does not re-prefill the same
  selected history for every short follow-up.
- Model preparation executes a native deterministic DMC self-test. The Android
  build also verifies DMC markers inside the ARM64 `libai-chat.so` packaged in
  the APK and fails if they are absent.
- The standalone C++ regression test passes, including deterministic
  131072-to-4096 planning, chronological ordering, newest-token retention, and
  the dense/full-selection attention oracle.
- The Android NDK build completed successfully for `arm64-v8a` and `x86_64`,
  and the generated APK passed the binary DMC marker check.
- Normal chat requests now default to unlimited output (`-1`) instead of 512
  tokens. Explicit positive API limits and explicit zero-token warmups are
  still honored.
- Full physical KV windows no longer end a visible answer. The generator
  performs a DMC continuation rebuild with fresh output space and continues
  until model EOG or an explicit caller limit.
- The APK gate now also verifies the compiled DMC generation-continuation and
  generation-limit markers, preventing publication of a stale 512-capped
  native library.
- The local `llama.cpp` runtime and browser UI are wired through the repo
  scripts.
- The web UI source can be rebuilt with `npm run build` in
  `upstream/llama.cpp/tools/ui`.
- The rebuilt `dist/` output has been synced into
  `upstream/llama.cpp/tools/server/public`, which is what the Android APK
  copies.
- The standalone Android app exists in `android/llama.android/`.
- The Android build/install helpers rebuild the Svelte UI and synchronize it
  into the Android assets before Gradle runs, preventing stale UI bundles.
- The DMC-enabled debug APK was rebuilt and installed as a data-preserving
  update on Samsung `SM-S931B` on 2026-07-21.
- The microphone and normal submit button are independently visible on Android.
- A nonempty native dictation result is passed directly to the normal submit
  handler and sent immediately; an empty result or cancellation sends nothing.
- SSE chat responses are not GZIP-compressed, so WebView receives and renders
  generated text token by token rather than as one buffered response.
- Device logs have confirmed streamed `/v1/chat/completions` requests ending
  normally with visible response text.
- The SSE producer no longer closes NanoHTTPD's reader side of the pipe. A
  device-side completion test returned incremental chunks, `finish_reason:
  "stop"`, and `[DONE]` with HTTP 200 and no replay request, incomplete chunked
  encoding, or network error after completion.
- The debug APK path is
  `android/llama.android/app/build/outputs/apk/debug/app-debug.apk`.
- The signed release APK and AAB were rebuilt on 2026-07-21 and copied to
  `\\diskstation.fritz.box\Dani`. The local and remote release APK SHA-256 is
  `B7CEEA47A02F3DC21ED9DEA2F6FA37693B006BE7BB3E0C06F1CDF2035840FCF2`.
- The Android WebView bootstrap now exposes native dictation support to the UI
  by setting `window.__DMC_NATIVE_DICTATION__ = true` and emitting
  `dmc-native-dictation-ready`.
- Image attachments are no longer discarded when the same user message also
  contains a question. The Android completion path now resolves multipart
  `image_url` content, decodes local base64 image data, reuses the on-device ML
  Kit OCR/image-labeling analyzer, and appends the result to the original user
  text before inference. Gallery and camera attachments share this API path.
- A device-side `/v1/chat/completions` test with a real JPEG confirmed
  `images=1`, `analyzedImages=1`, OCR output, and an 843-character enriched
  native prompt instead of the previous text-only prompt.

## Current Open Issues To Recheck

- Reconnect the Samsung `SM-S931B`, install the current DMC APK, and capture the
  `DMC_RUNTIME self-test passed` and `DMC_RUNTIME enabled=1` model-load markers.
- Run a context-pressure request large enough to produce a `DMC_RUNTIME
  rebuild=` marker and verify that streaming still terminates with `[DONE]`.
- The Windows LAN launcher still runs standard dense llama.cpp context. The
  DMC adapter implemented here is currently Android-specific.
- Python was unavailable in the current shell, so the Python reference suite
  could not be rerun. The C++ test and both Android ABIs compiled successfully.

- The first request after a fresh model download previously showed reconnecting
  and stream-resume failures. It still needs a clean-device regression test
  after the SSE buffering fix.
- Mixed-language or untranslated UI text has been observed in parts of the
  Android flow and needs a full localization audit.
- Attachment flow changes, especially the camera/photo action in the add menu,
  need device QA.
- The image transport and local analysis path is verified, but the current
  Gemma test still answered as if the image were unavailable despite receiving
  OCR context. Prompt behavior and OCR latency/quality therefore still require
  an end-user regression pass; transport must not be misdiagnosed again as the
  remaining model-response issue.
- Any black-screen or stuck-processing behavior after app start or after model
  load must be treated as a regression and reproduced before fixing.
- The stop/pause button state after sending a message should be verified after
  each release build.

## Recent Change

- Removed the hidden Android 512-token fallback that cut responses in the
  middle of sentences. Missing output limits now mean model-EOG generation.
- Replaced the physical-position stop condition with an independent explicit
  output-token counter, so DMC KV rebuilds cannot accidentally alter API limit
  semantics.
- Added generation-time DMC rehydration when the 16K physical window fills;
  rebuild failure is raised as an error instead of being reported as a normal,
  silently truncated completion.
- Rebuilt both debug and signed release variants. The ARM64/x86_64 builds,
  release R8/lint tasks, APK DMC marker gate, signing validation, and remote
  SHA-256 comparison completed successfully.

- Git history was audited across all local/remote refs and the original
  `llama.cpp-android` repository. There is no release branch in this repository.
  Commit `362d9b1` deleted the older AHSMA v0.6 artifact stack before Android
  was imported. That older stack contained validated lifecycle/attention
  references and compile plumbing, but its own status explicitly listed the
  real llama.cpp KV-cache integration as unfinished.
- `cpp/dmc_reference.hpp` now exposes a shared deterministic runtime plan. It
  leaves short histories dense and returns bounded, chronological selections
  for histories above the physical token budget.
- `ai_chat.cpp` now owns canonical DMC token history, uses a 16K physical
  context, reports the trained logical context, rebuilds selected KV state on
  context pressure, and logs explicit self-test/enablement/rebuild markers.
- `scripts/windows/verify-android-dmc.ps1` inspects the built APK and
  `build-android.bat` refuses to publish an ARM64 artifact without DMC markers.
- The pre-existing C++ attention test was corrected to compare dense attention
  against selection of all four source tokens rather than only two.

- `MainActivity.kt` tells the WebView bootstrap that native dictation is
  available, disables GZIP only for `text/event-stream` responses, and leaves
  the stream reader owned by NanoHTTPD until all final bytes are delivered.
- `MainActivity.kt` now treats the latest multipart user turn atomically:
  question text and attached images are combined instead of selecting text and
  skipping the image fallback. It accepts up to four local image data URLs,
  limits encoded/context sizes, logs metadata only, and removes temporary image
  files after local analysis.
- `ChatForm.svelte` refreshes dictation availability after WebView init and
  submits the recognized text as an explicit message value.
- `ChatScreenForm.svelte` accepts that explicit value, so auto-submit does not
  depend on UI-state propagation timing.
- `ChatFormActions.svelte` receives the real `canSubmit` value and renders the
  Android microphone separately from the submit/stop action.
- `ChatFormActionRecord.svelte` now renders the mic button with explicit cyan
  styling and a white icon, so it is visible again on the phone UI.
- `build-android.bat` and `install-android.bat` call
  `scripts/windows/prepare-android-webui.ps1` to regenerate and synchronize the
  UI automatically unless `ANDROID_SKIP_WEBUI_BUILD=1` is explicitly set.
- The UI build was regenerated, synced into `tools/server/public`, and the
  Android APK was rebuilt and installed on Samsung `SM-S931B`.
- The installed build loaded bundle `bundle.B0xuRrF3.js`; model initialization
  completed with a 131072-token context window.
- The repository docs now have a live root `state.md`, and `README.md` points to
  it as the current resume document.
- The latest debug APK, including the explicit-value dictation submit change,
  was rebuilt and installed successfully. A final spoken-input test still
  requires speaking into the device recognizer.

## Planned Checks

- Treat removal, bypass, or silent fallback of DMC in Android as a
  release-blocking regression. Do not replace it with plain large-context
  allocation, recursive summarization, or UI-only branding.
- Rebuild and reinstall the APK after any source change.
- Verify dictation, streaming, model download resume, image attachment, and
  localized text on the phone.
- Keep this file updated whenever a behavior changes, a regression is found, or
  a fix is confirmed.

## Notes

- `docs/TARGET_STATE.md` describes the architectural target.
- `docs/STATE.md` is the older concise summary.
- `README.md` and this file are the living documents for the repository.
