# InetMind / DMC

**InetMind** is the consumer-facing Android app in this repository.
**Deterministic Multiresolution Context (DMC)** is its native long-context
engine and the clean-room technical target of the project.

In plain terms: DMC is a design for a local AI system that can keep much more
conversation history in working memory, stay deterministic, and remain easier
to validate than a more ad hoc retrieval system.

## Current State

- `README.md` and `state.md` are the living project documents.
- [`docs/LARGE_CONTEXT.md`](docs/LARGE_CONTEXT.md) and
  [`docs/GROSSER_KONTEXT.md`](docs/GROSSER_KONTEXT.md) explain the Android
  long-context architecture in accessible English and German, including its
  algorithm, runtime limits, and trade-offs.
- [`docs/OFFLINE_WISSENSMODULE.md`](docs/OFFLINE_WISSENSMODULE.md) explains the
  bilingual offline knowledge-module architecture, ICD-10-GM/ICD-11 import,
  package format, retrieval path, attribution, and safety boundaries.
- `docs/TARGET_STATE.md` is the concise capability target.
- The code in `dmc/` and `cpp/` is the reference implementation.
- The Android native runtime now compiles that same C++ DMC selector into
  `libai-chat.so`; it is no longer only an app name or a separate demo.
- The local runtime in `scripts/windows/` runs `llama.cpp` on your own machine.
- The Android app lives in `android/llama.android/` and is built from the root
  helper scripts.
- The Android package remains `com.inetconnector.dmc`; the visible app and
  Google Play name is **InetMind - Local AI** / **InetMind - Lokale KI**.
- The current release candidate is `1.0.1 (2)`. Android V1.0 remains available
  as a signed APK from GitHub Releases.
- The design focuses on long context first, quality second, speed third.
- DMC's original code is licensed under MIT. Bundled third-party components
  retain their own licenses and terms; see `THIRD_PARTY_NOTICES.md`.
- Nothing in this repository should be read as a patent clearance opinion or
  as a guarantee that the implementation is free to publish in every
  jurisdiction.

## What You Get

- A local AI stack that can run on your own computer.
- Much larger practical conversation memory than a plain short-context setup.
- A clean structure for code, docs, tests, and a browser-accessible model UI.
- Optional editor integration through Continue.
- A phone-friendly LAN interface if you want to use the model from the same
  network.

## Fast Start

If you just want the chat UI, run `run.bat` from the repository root.
If you want phone access on the same Wi-Fi, run `run-phone.bat` from the
repository root.

What it does:

1. Starts the local `llama.cpp` server.
2. Waits for the local API to be ready.
3. Tries the largest context that starts successfully.
4. Opens the browser UI at `http://127.0.0.1:8080/`.

`run-phone.bat` also sets up the Windows firewall for the local network, then
starts the same server.
It prints the primary LAN IPv4 address that the phone can use.

## Android APK

The standalone Android app now lives in `android/llama.android/` and is
checked in alongside the DMC stack.

For Google Play automation, the repository includes a non-secret package pin
at `.gplay/config.yaml` and the InetMind/DMC-specific setup guide
[`docs/GOOGLE_PLAY_DMC.md`](docs/GOOGLE_PLAY_DMC.md). Service-account JSON keys
must remain outside the repository; use
`scripts/windows/setup-gplay-dmc.ps1` to register and verify the app-scoped
`dmc` profile without printing the private key.

The complete version-controlled Google Play source set lives under `play/`:
nine localized listings and release notes, five reviewed German phone
screenshots, two English phone screenshots, a 512x512 RGB icon, a 1024x500 RGB
feature graphic, and the editable SVG artwork. The screenshots are direct,
privacy-checked 1080x1920 captures from a Samsung S25. Subjects and suggested
accessibility text are documented in
[`play/screenshots/README.md`](play/screenshots/README.md).

Version `1.0.1 (2)` is uploaded, processed, and active in Google Play's
internal test track. It adds the InetMind branding, an in-app generated-content
reporting path, privacy/support links, conservative privacy disclosures, and
disabled Android backup. The shared service account can read the app and manage
releases, but Google still returns `403 forbidden` when it commits store text;
the app-specific **Manage store presence** permission must be corrected or
allowed time to propagate. The remotely visible English title therefore
remains the old draft name until that permission is fixed. Pricing (`4.99
EUR`), Data safety, health/content declarations, target audience, and the final
production submission remain Play Console steps.

The maintained Play submission sources are:

- [`play/DATA_SAFETY.md`](play/DATA_SAFETY.md) for the exact Data safety answer
  matrix;
- [`docs/BRAND_AND_PLAY_REVIEW.md`](docs/BRAND_AND_PLAY_REVIEW.md) for the name,
  trademark-screening limits, and policy review;
- [`privacy/index.html`](privacy/index.html), published at
  <https://inetconnector.github.io/DMC/privacy/>, for the bilingual privacy
  policy.

**Download:** [DMC Android V1.0 release](https://github.com/inetconnector/DMC/releases/tag/android-v1.0.0)
or [download the signed APK directly](https://github.com/inetconnector/DMC/releases/download/android-v1.0.0/com.inetconnector.dmc-1.0.0%2B1-release.apk).

The APK requires Android 13 or newer. It contains no AI model; install the APK,
allow installation from the selected browser or file manager when Android asks,
then import or download a compatible local GGUF model inside the app. Large
models require several gigabytes of free storage.

On Android, every file-opening flow prefers CX File Explorer when
`com.cxinventor.file.explorer` is installed: model files and archives,
offline-reference ZIP/XML files, images, audio, text, PDF, and general WebView
attachments. If CX is absent or cannot handle a requested MIME type, InetMind uses
Android DocumentsUI automatically. Save/export dialogs remain on DocumentsUI
because the current CX version does not implement Android's `CREATE_DOCUMENT`
contract; camera capture is unchanged.

Use these helpers from the repository root:

1. `build-android.bat` to build the APK on Windows.
2. `install-android.bat` to build, install, and launch the app on a connected
   device.

Both helpers rebuild the Svelte Web UI and synchronize the resulting `dist/`
files into the Android assets before Gradle runs. This prevents an APK from
silently containing an older UI bundle. Set `ANDROID_SKIP_WEBUI_BUILD=1` only
for a deliberate native-only rebuild when the synchronized UI is already
current.

The Android project is wired against the pinned `upstream/llama.cpp` Git
submodule. It points to the DMC Android branch of the public
[`inetconnector/llama.cpp`](https://github.com/inetconnector/llama.cpp) fork,
so a normal clone can reproduce the native runtime and imported Web UI instead
of relying on an untracked local directory. Clone with `--recurse-submodules`
or run `git submodule update --init --recursive` after cloning. The build helper
verifies the exact recorded submodule commit and refuses a dirty or mismatched
checkout.

`build-android.bat` also regenerates the exact Web UI dependency notices and
packages the DMC, llama.cpp, Apache 2.0, NanoHTTPD, and generated Web UI license
files into the APK under `assets/licenses/`. The repository's MIT license
covers DMC's original code only. In particular, Google ML Kit remains subject
to Google's separate terms, and downloaded GGUF models retain their publishers'
licenses. See `THIRD_PARTY_NOTICES.md` for the complete scope.

The Android inference path is DMC-enabled. It keeps the canonical token history
separately from a 16384-token physical llama.cpp KV window. Short conversations
remain on the unchanged dense path. When the physical prompt budget is reached,
the shared deterministic selector from `cpp/dmc_reference.hpp` keeps the global
prefix, the recent 2048-token window, and seven fixed replay levels, then
rehydrates those selected tokens in chronological order. For a Gemma model that
reports 131072 trained context tokens, the UI therefore retains a 131072-token
logical context while avoiding a 131072-token mobile KV allocation. This path
does not recursively summarize the conversation. The selected KV state is
reused for subsequent turns and rebuilt only when the physical budget fills
again, avoiding repeated long prefills.

The Android API also isolates that state by conversation. A normal next turn
with the same `X-Conversation-Id` keeps the fast incremental DMC/KV path. A new
chat, switched chat, edited/retried turn, app/model reset, or request without a
conversation ID rebuilds the native state from the complete OpenAI-compatible
`messages` transcript before generation. This prevents offline-reference
evidence, assistant output, and internal helper requests from leaking into a
different chat while preserving fast continuation inside the active chat.

Chat metadata and every message branch are stored persistently in the WebView's
private IndexedDB. On Android startup, the app restores the last opened
conversation (or the newest stored conversation if no last-chat marker exists)
and reloads its selected message branch. Choosing **New chat** still opens an
empty, isolated conversation. When an older chat is continued, its complete
stored branch is sent with its stable conversation ID, so the native runtime
rehydrates that chat's own DMC/KV context instead of borrowing state from the
previously active chat.

Normal chat generation has no implicit 512-token output cap. If a request does
not explicitly provide `max_tokens`, `max_completion_tokens`, or `n_predict`,
Android generates until the model emits its end-of-generation token. When an
answer fills the physical KV window, DMC rehydrates a selected context with
fresh output space and continues the same generation. Explicit positive API
limits and the explicit zero-token cache-warm request remain supported.

Every model preparation runs a deterministic native DMC self-test. In addition,
`build-android.bat` opens the generated APK and refuses to publish it unless the
ARM64 native library contains the DMC enablement, self-test, and rebuild markers.
Removing or bypassing DMC from the Android inference path is a release-blocking
regression.

On Android, native dictation inserts the recognized text and immediately sends
it as a normal chat message. The microphone and manual send button remain
separate controls. Chat responses use uncompressed server-sent events so text
appears token by token instead of arriving as one buffered block. The Android
SSE producer closes only its writer after the final `[DONE]` marker; NanoHTTPD
owns the reader until delivery completes, preventing false reconnect banners
after an otherwise successful answer.

Image attachments from both the file picker and the camera use the same local
multimodal bridge. Android decodes the Web UI's base64 image part, runs the
existing on-device ML Kit OCR and image-labeling pipeline, and appends that
analysis to the user's original question before DMC/Gemma inference. Raw image
data never leaves the device and temporary files are deleted immediately.

Android also supports installable offline knowledge modules. In Settings under
`Import/Export`, the module manager imports ICD-10-GM ClaML ZIP/XML files or
versioned `.dmcknowledge` packages for ICD-11 and generic sources. Each module
is independently enabled, disabled, or removed. Enabled modules are searched
locally with exact-code boosting and SQLite FTS4; a bounded, attributed evidence
block is appended after the current user question so it remains in DMC's recent
context window. Classification data is not bundled in the APK. Use
`scripts/knowledge/build_knowledge_module.py` to package a lawful local ICD-11
or generic JSONL export, and read `docs/OFFLINE_WISSENSMODULE.md` before
redistributing any source data.

Retrieval is deliberately relevance-gated. Greetings and common conversational
words are removed, short terms require whole-word matches, and prefix search is
allowed only for terms of at least seven characters. A record is injected only
for an exact code, an exact title word, a sufficiently long medical prefix, or
multiple matching content terms. This prevents collisions such as `Hallo`
matching ICD-10's `Hallopeau` and avoids expensive irrelevant DMC prefills.

The package builder has automated tests for deterministic output and rejection
of unofficial WHO sources, invalid licences, missing official URIs, and missing
unchanged-content confirmation. Android instrumentation tests cover package
import, exact-code and free-text retrieval, enable/disable isolation,
transactional rollback, and deletion. Classification data, WHO credentials,
and redistribution rights are never included by the builder or APK.

The current signed InetMind/DMC release was built successfully at:

`android/llama.android/app/build/outputs/apk/release/app-release.apk`

Versioned release artifacts:

- `publish/com.inetconnector.dmc/1.0.1+2/com.inetconnector.dmc-1.0.1+2-release.apk`
- `publish/com.inetconnector.dmc/1.0.1+2/com.inetconnector.dmc-1.0.1+2-release.aab`

Release APK SHA-256:

`45882B5CD40792FDA3273C94B0DA6296BFD3C04C9AFBD8CF09CE0F921F6A1707`

Release AAB SHA-256:

`5D3B820572CB866C729D5CF362A23E34BAD6BDC94926075C3B33FF467B4E7905`

Published APK/AAB files are kept under the ignored `publish/` directory and
must not be committed. The current signed release and Play submission archive
are mirrored to the branch directory on the DiskStation after verification.
Google Play internal testing currently serves `1.0.1 (2)` with status
`completed`.

If you want to check the launch without starting the server, use:

```powershell
.\scripts\windows\start-llama-lan.ps1 -DryRun
```

## Why It Matters

- You can keep more project context without constantly repeating yourself.
- Long chats, coding sessions, and large documents become easier to handle.
- The local setup gives you control over the model, the context size, and the
  runtime.
- The project is organized so the public documentation matches the actual
  implementation.

## How It Works

1. The model runs locally in `llama.cpp`.
2. Android retains the canonical logical token history outside the physical KV
   window.
3. DMC deterministically selects global, local, and multiresolution replay
   spans when the physical prompt budget is exceeded.
4. llama.cpp rebuilds the KV state in causal order and applies exact attention
   over those selected tokens.
5. Short Android chats stay dense; long chats switch to DMC without recursive
   summarization.
6. The Windows LAN launcher currently remains the standard dense llama.cpp
   runtime; its DMC adapter is still a separate follow-up task.

## Start Here

1. Read `state.md` for the current status and next steps.
2. Read `docs/INSTANCE.md` for the active runtime setup.
3. Read `docs/LLAMA_CPP_LAN.md` if you want the browser UI on your laptop or
   phone.
4. Read `docs/CONTINUE_SETUP.md` if you want editor integration in VS Code or
   JetBrains.
5. Use `run.bat` for the fastest out-of-the-box start.
6. Use `run-phone.bat` if you want to reach the chat UI from a phone on the
   same network.
7. Use `scripts/windows/start-llama-lan.ps1` if you want to pass explicit
   runtime options.
8. Use `-Use64KContext`, `-Use128KContext`, or `-Use256KContext` only when you
   want to force a specific preset.
9. Read `docs/VALIDATION.md` if you want the test and benchmark order.
10. Read `docs/PUBLICATION_READINESS.md` for the release posture and checklist.
11. Use `dmc/` and `cpp/` if you want the reference implementations.
12. Use `scripts/windows/prepare-dev.ps1` to validate and run the local setup.
13. Read `docs/OFFLINE_WISSENSMODULE.md` to install or build offline ICD and
    other reference modules.

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.
- Keep the release narrative conservative and technically defensible.
