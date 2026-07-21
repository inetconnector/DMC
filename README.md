# DMC

Deterministic Multiresolution Context (DMC) is the active clean-room target for
this repository.

In plain terms: DMC is a design for a local AI system that can keep much more
conversation history in working memory, stay deterministic, and remain easier
to validate than a more ad hoc retrieval system.

## Current State

- `README.md` and `state.md` are the living project documents.
- [`docs/LARGE_CONTEXT.md`](docs/LARGE_CONTEXT.md) and
  [`docs/GROSSER_KONTEXT.md`](docs/GROSSER_KONTEXT.md) explain the Android
  long-context architecture in accessible English and German, including its
  algorithm, runtime limits, and trade-offs.
- `docs/TARGET_STATE.md` is the concise capability target.
- The code in `dmc/` and `cpp/` is the reference implementation.
- The Android native runtime now compiles that same C++ DMC selector into
  `libai-chat.so`; it is no longer only an app name or a separate demo.
- The local runtime in `scripts/windows/` runs `llama.cpp` on your own machine.
- The Android app lives in `android/llama.android/` and is built from the root
  helper scripts.
- DMC Android V1.0 is available as a signed APK from GitHub Releases.
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

**Download:** [DMC Android V1.0 release](https://github.com/inetconnector/DMC/releases/tag/android-v1.0.0)
or [download the signed APK directly](https://github.com/inetconnector/DMC/releases/download/android-v1.0.0/com.inetconnector.dmc-1.0.0%2B1-release.apk).

The APK requires Android 13 or newer. It contains no AI model; install the APK,
allow installation from the selected browser or file manager when Android asks,
then import or download a compatible local GGUF model inside the app. Large
models require several gigabytes of free storage.

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

The current DMC-enabled debug APK was built successfully at:

`android/llama.android/app/build/outputs/apk/debug/app-debug.apk`

The current signed release was built successfully at:

`android/llama.android/app/build/outputs/apk/release/app-release.apk`

Release APK SHA-256:

`E32DB4D728D53F0631428D4D2A7D801A1415F80DCC97525EE30B0EEFA9F732C7`

Release AAB SHA-256:

`3AD275C6DDD6D86C4D674A938812337F6FB4C3F03CDB3CE184DC51B336D78F6E`

Debug APK SHA-256:

`D663D4B60CAAA1C719ABDC9F5055AE36CA3AB309D84B22030F16D6AF9B4129C3`

Published APK/AAB files are kept under the ignored `publish/` directory. The
current debug APK, signed release APK, and signed release AAB were also copied
to `\\diskstation.fritz.box\Dani` using versioned
`com.inetconnector.dmc-1.0.0+1` names. Their hashes were verified again after
copying.

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

## Goals

- Preserve dense-model quality after training.
- Reduce long-context inference cost.
- Keep the dense execution path unchanged unless explicitly enabled.
- Produce reproducible benchmarks and open-source implementation.
- Keep the release narrative conservative and technically defensible.
