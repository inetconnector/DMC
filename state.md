# State

Last updated: 2026-07-22

This file is the live development log for the repository.
Update this file and `README.md` together whenever behavior changes.

## Snapshot

- Repository root: `C:\Users\frede\Projekte\AHSMA`
- Active branch: `offline-knowledge-modules`, created from `develop` for the
  licence-compliant offline-reference implementation.
- Commit: use `git rev-parse --short HEAD` for the current revision; this file
  deliberately avoids embedding a hash that becomes stale in its own commit.
- Tracked state: Android dictation/SSE fixes, reproducible Web UI integration,
  native DMC runtime and continuation, DMC regression gates, and refreshed
  documentation are integrated on `master` and `develop`. Generated APK/AAB
  files remain in the ignored `publish/` directory, on the DiskStation, and as
  downloadable GitHub Release assets.
- Active target: DMC (Deterministic Multiresolution Context)
- `docs/LARGE_CONTEXT.md` and `docs/GROSSER_KONTEXT.md` document the implemented
  Android long-context pipeline in accessible English and German, including
  the logical/physical context split, deterministic span selection, KV
  rehydration, continuation, diagnostics, and current limitations.
- `docs/OFFLINE_WISSENSMODULE.md` documents the implemented offline module
  format, ICD-10-GM/ICD-11 flows, retrieval behavior, and licensing boundary in
  German and English.

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
- Native DMC/KV state is now scoped to the Web UI conversation ID. The expected
  next turn in the same conversation remains incremental. New/switched chats,
  retries, edited branches, stateless API calls, model changes, and failed or
  cancelled generations rebuild from the complete request transcript. This
  prevents an ICD result or any other prior prompt from appearing in a new chat.
- Android startup now restores the last opened IndexedDB conversation, falling
  back to the newest stored chat when no last-chat marker exists. Selecting an
  older conversation loads its complete active message branch; its next request
  therefore rebuilds native DMC/KV state from that chat's own full transcript.
  The explicit **New chat** route remains empty and isolated.
- Three focused Web UI unit tests pass for restoration of an existing last chat,
  fallback from a stale marker to the newest chat, and an empty database. The
  data-preserving `adb install -r` kept the device WebView IndexedDB at 319 KB
  before and after installation, confirming that the APK update did not clear
  the stored conversation database.
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
  update on Samsung `SM-S931B` on 2026-07-22.
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
- The signed release APK and AAB were rebuilt from the V1.0 source on
  2026-07-21. Android `apksigner` verifies the APK with v2 signing and the
  expected InetConnector certificate fingerprint
  `649A7EC870A7D18E5AF0AF12F0AC63B27F15DB864E28FECA9DA5FCF94AB8EC0F`.
  The release APK SHA-256 is
  `E32DB4D728D53F0631428D4D2A7D801A1415F80DCC97525EE30B0EEFA9F732C7`;
  the AAB SHA-256 is
  `3AD275C6DDD6D86C4D674A938812337F6FB4C3F03CDB3CE184DC51B336D78F6E`;
  and the debug APK SHA-256 is
  `9C241211C8260B93C2B66CB36403E89C4EE59DB337AC6D40940F37E2E5970A63`.
  All three versioned artifacts were copied to
  `\\diskstation.fritz.box\Dani`, then read back and hash-verified.
- The public Android V1.0 release uses tag `android-v1.0.0` because the older
  repository tag `v1.0.0` already identifies a pre-Android project milestone
  and must not be moved. The release page is
  `https://github.com/inetconnector/DMC/releases/tag/android-v1.0.0`.
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
- Android now has a generic offline knowledge-module subsystem. It stores
  module metadata and records in a private SQLite database, uses FTS4 plus
  exact-code boosting, and searches enabled modules only.
- The app directly imports ICD-10-GM ClaML XML or ZIP archives. ICD-11 and
  generic modules use the validated `.dmcknowledge` ZIP format with explicit
  version, language, jurisdiction, source, license, record count, and SHA-256.
- Imports are size/path/schema checked and transactional. A failed import cannot
  leave a partial active index, and a retrieval failure falls back to ordinary
  chat generation.
- Retrieved passages are appended after the latest user question as bounded,
  attributed, untrusted evidence. This places current evidence in DMC's recent
  local window without altering the canonical conversation history, image path,
  or SSE streaming path.
- Retrieval now removes multilingual greetings/filler words, requires whole
  words for short terms, allows prefix matching only from seven characters, and
  applies a relevance threshold before adding evidence. This prevents casual
  chat from activating large ICD prompt blocks.
- Settings > Import/Export now exposes a localized Offline Knowledge manager.
  Installed modules can be imported, enabled, disabled, and removed separately.
- `scripts/knowledge/build_knowledge_module.py` produces deterministic ICD-11
  or generic packages from lawful local JSONL. For ICD-11 it requires unchanged
  string fields `id`, `code`, `title`, and the official `id.who.int`
  `uri`; unofficial hosts, wrong licence metadata, and missing explicit
  unchanged-content confirmation are rejected.
- All nine Android locale XML files parse and contain the same 22
  offline-reference keys. The complete Svelte production build passes and its
  output is synchronized to the Android server assets.
- A dependency-free Python smoke test confirms deterministic package bytes,
  unchanged record values, and rejection of an unofficial source and wrong
  licence. The repository pytest suite could not run in this session because
  the bundled Python runtime has no pytest package and network installation is
  disabled.
- Android instrumentation coverage now exists for import, exact-code and FTS
  retrieval, enable/disable, rollback, and deletion. It has not been executed
  for this branch.
- The `offline-knowledge-modules` branch debug APK was built successfully on
  2026-07-22. Its SHA-256 is
  `10E119B8F6CE5E8C26C4F1B7332DEE5705C7D712604E9D317F3073F1CE06E86B`.
  The APK gate found the compiled DMC runtime markers, and direct ZIP inspection
  found both the native knowledge bridge/store classes and the offline-reference
  Web UI. The signed release hashes above still describe the V1.0 artifacts.
  The branch APK was copied to
  `\\diskstation.fritz.box\Dani\offline-knowledge-modules\com.inetconnector.dmc-1.0.0+1-offline-knowledge-modules-debug.apk`
  and its SHA-256 was verified again after copying.
- Fixed the official-reference confirmation dialog after device screenshots
  showed that `AlertDialog.setMessage()` suppressed the multi-choice checkbox
  and left **Continue** permanently disabled. The dialog now uses explicit
  message and `CheckBox` views; checking the visible confirmation enables the
  file picker. The rebuilt APK and Android test APK compile successfully, and
  the branch APK on the DiskStation was replaced and hash-verified.
- Installed that corrected APK on Samsung `SM-S931B` with `adb install -r` on
  2026-07-22. Android reports the unchanged first-install timestamp and a new
  update timestamp, confirming a data-preserving update. `MainActivity` gained
  focus and the clean post-launch Logcat contained no fatal exception.
- The offline-reference file picker now opens CX File Explorer directly when
  `com.cxinventor.file.explorer` is installed and accepts the ZIP/XML document
  intent; otherwise it falls back to Android DocumentsUI. A device-side intent
  smoke test resolved CX's picker activity and brought CX to the foreground.
  The resulting APK was installed data-preservingly, copied to the branch
  folder on the DiskStation, and hash-verified with no fatal Logcat entry.
- CX preference is now centralized across every file-opening path: local model
  and model-archive import, offline references, images, audio, text, PDF, and
  general WebView attachments. Device-side resolver checks passed for all seven
  MIME/action combinations. Android DocumentsUI remains the automatic fallback
  and continues to handle save/export because CX version 276 does not resolve
  `CREATE_DOCUMENT`; camera capture remains separate and unchanged.
- Verified the importer against the user's official
  `icd10gm2026syst-claml.zip` from the Samsung Downloads folder. The archive has
  18 entries; recursive content detection correctly selects
  `Klassifikationsdateien/icd10gm2026syst_claml_20250912.xml` and ignores the
  sibling `ClaML2.0.0.xsd`, PDFs, and TXT files. Added and compiled an Android
  regression test for this nested ZIP/XSD-before-XML layout. The temporary PC
  test copy was deleted; the user's original phone download was not modified.
- Root-caused the reported `Hallo` -> L40.2 behavior on the real phone. It was a
  fresh FTS prefix collision: `hallo*` matched the ICD title term `Hallopeau`.
  Common words such as `mit` could likewise retrieve broad, irrelevant records,
  append up to 12000 evidence characters, and cause a long CPU prefill. Short
  terms are now exact-only, greetings/filler words are ignored, and scored
  results must pass an explicit relevance gate.
- Added defense-in-depth conversation isolation as well. The OpenAI-compatible
  endpoint had resolved only the latest user text while native
  `g_dmc_token_history`, `chat_msgs`, sampler state, and KV state were global.
  The engine can now atomically replace a conversation from all request messages;
  `ConversationStateTracker` permits incremental reuse only when conversation
  ID, message progression, and a SHA-256 signature of visible history all match.
- Nine local regression tests cover greetings/medical query terms, first
  request, same-chat continuation, new-chat isolation, edited history,
  retry/edit isolation, and stateless API behavior. Kotlin,
  JNI ARM64/x86_64 compilation and `:app:assembleDebug` pass, and the APK passes
  the DMC binary marker gate. It was installed with `adb install -r`; startup
  Logcat confirmed the existing Gemma model, 16384 physical context, 131072
  logical context, and successful DMC self-test.
- A final real-device replay against the installed APK sent a one-message
  `Hallo` request under a fresh conversation ID. Logcat confirmed
  `mode=REBUILD`, one message, and only 10 logical/physical prompt tokens; there
  was no ICD evidence or `Hallopeau`/`L40.2` occurrence. The non-streaming API
  completed in about one second with `Hallo! Wie kann ich dir heute helfen?`.
- The replaced DiskStation branch APK has SHA-256
  `10E119B8F6CE5E8C26C4F1B7332DEE5705C7D712604E9D317F3073F1CE06E86B`,
  byte-for-byte matching the local build.

## Current Open Issues To Recheck

- Run a context-pressure request large enough to produce a `DMC_RUNTIME
  rebuild=` marker and verify that streaming still terminates with `[DONE]`.
- Import a real licensed ICD-10-GM ClaML archive and a real licensed ICD-11
  package on the Samsung device, verify enable/disable isolation, and compare
  exact-code and natural-language retrieval against the source editions.
- Run the Android instrumentation tests on an emulator or disposable install;
  do not run them on the user's model-bearing phone because test deployment can
  clear private app data. Do not run them on the user's installed Samsung app.
- The Windows LAN launcher still runs standard dense llama.cpp context. The
  DMC adapter implemented here is currently Android-specific.
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
- The Web UI release build succeeds but currently reports non-fatal warnings
  for duplicate `MCP Resources` translation keys, missing ARIA roles on swipe
  containers, one Svelte state-capture warning, and a Rollup circular re-export.
  These warnings should be cleaned up before the next release.
- The published `android-v1.0.0` tag predates the checked-in llama.cpp
  submodule pointer. Do not move that public tag. Publish the next Android
  patch release from the reproducible submodule-based source state.
- Run connected Android instrumentation only on an emulator or disposable test
  installation. Gradle's test deployment can clear private target-app data,
  including downloaded GGUF models; this happened during the module test and
  the recommended Gemma 4 E2B model was subsequently restored successfully.
- A final visual restart check of last-chat restoration still requires the S25
  to be unlocked. Its secure keyguard prevented automated UI inspection; source
  tests, the production Web UI build, APK build, data-preserving installation,
  and unchanged on-device IndexedDB size are already verified.

## Recent Change

- Added strict conversation isolation without disabling DMC. The server now
  reconstructs native chat-template/DMC state from the complete API transcript
  whenever the conversation identity or branch progression changes, while an
  ordinary same-chat follow-up still reuses the existing fast KV state.
- Added durable Android chat restoration. The Web UI remembers the last opened
  conversation ID, restores it only for the native app-start URL, falls back to
  the newest IndexedDB chat, and preserves the explicit new-chat action.
- Fixed the real ICD greeting collision: `Hallo` can no longer prefix-match
  `Hallopeau`, and generic conversational words no longer trigger large offline
  evidence injections.
- Native prompt-processing failures now raise an error rather than silently
  completing with zero characters, and sampler state resets at every assistant
  turn so generation penalties cannot bleed across responses.
- Added the generic Android offline knowledge-module store, secure importer,
  ICD-10-GM ClaML parser, `.dmcknowledge` ICD-11/generic package reader,
  deterministic package builder, settings bridge, localized manager, and
  automatic attributed retrieval before local generation.
- Corrected Android FTS4 prefix construction after a real-device integration
  test exposed that exact code lookup worked while free text returned no hits.
  ZIP import now detects the actual ClaML root instead of selecting the first
  XML file in an archive.
- No ICD classification content or WHO OAuth secret is bundled. This keeps
  source edition and licensing explicit and avoids silently training mutable
  clinical facts into model weights.
- Hardened the import path for licence-compliant offline use. ICD-10-GM is
  imported only as the user-selected official ClaML XML/ZIP and retains the
  prescribed BfArM attribution. ICD-11 packages require WHO source/licence
  hosts, CC BY-ND 3.0 IGO metadata, World Health Organization attribution,
  unchanged-content confirmation, and an official URI for every code/title.
- Added an explicit pre-import confirmation, official BfArM/WHO source links,
  source/licence/checksum details, enable/disable/delete controls, and complete
  translations for all nine supported Android/Web UI languages.
- Added bilingual `docs/OFFLINE_WISSENSMODULE.md`, deterministic package
  builder tests, Android integration tests, and optional `dmcNdkPath` Gradle
  support for restricted/offline build environments.

- The formerly ignored `upstream/llama.cpp` working tree was converted into a
  real Git submodule. It is pinned to commit
  `404affccbd730d2fb9a2ad20e4c66f0c46ea1809` on the `dmc-android` branch of
  `https://github.com/inetconnector/llama.cpp.git`; that repository is a public
  fork of `ggml-org/llama.cpp` and preserves the complete DMC Android diff.
- `build-android.bat` now initializes and validates that exact submodule commit
  before doing any UI or native build work. A dirty checkout, wrong origin, or
  missing fork marker fails the build instead of producing an unreproducible
  APK.
- The root ignore rules now retain both the submodule and the required
  `com/arm/aichat` Kotlin sources, which had been hidden accidentally by a
  broad Visual Studio `Arm/` pattern.
- The licensing audit confirmed that DMC's own code and llama.cpp can be
  distributed under MIT, but the binary is not exclusively MIT: AndroidX,
  Material and Kotlin carry Apache 2.0 terms, NanoHTTPD is BSD 3-Clause, and
  Google ML Kit is governed by its own service/API terms. Exact notices are in
  `THIRD_PARTY_NOTICES.md` and `third_party_licenses/`.
- Android builds now generate notices for every installed Web UI package and
  package all DMC, llama.cpp, Apache, BSD, and Web UI license material under
  `assets/licenses/` in the APK.
- The submodule-based debug APK was rebuilt, installed with preserved app data,
  and launched on Samsung `SM-S931B` on 2026-07-21. Visual inspection confirmed
  the chat UI, model selector, microphone, and send button; the process remained
  alive with `MainActivity` focused and no fatal exception or black screen.
- The current debug APK was installed again as a data-preserving update because
  the existing device installation uses the debug certificate. Installing the
  separately signed release over it would require uninstalling the app and
  deleting its private model and settings data.
- Device Logcat confirmed `DMC_RUNTIME self-test passed`, `enabled=1`, a 16384
  physical context, and a 131072 logical context while loading the existing
  Gemma model. Direct device API checks returned `OK` with `finish_reason=stop`;
  SSE delivered the content incrementally, one terminal stop chunk, and exactly
  one `[DONE]` marker without reconnect, resume, or network errors.
- Android can reclaim DMC under device-wide memory pressure when another large
  local-model app is active at the same time. With the competing model app
  stopped, DMC starts normally, remains alive, and loads the installed Gemma
  model. This is a device memory limit, not a DMC fatal exception.

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
