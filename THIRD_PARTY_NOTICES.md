# Third-Party Notices

DMC's original source code is licensed under the repository's MIT license.
Third-party components remain under their own licenses and terms. The MIT
license does not replace or narrow those third-party terms.

## llama.cpp and ggml

- Component: `ggml-org/llama.cpp`, including `ggml`
- DMC fork: <https://github.com/inetconnector/llama.cpp>
- Upstream: <https://github.com/ggml-org/llama.cpp>
- License: MIT
- Copyright: 2023-2026 The ggml authors

The complete license is included as `licenses/LLAMA-CPP-LICENSE.txt` in the
Android application and remains present in the fork as `LICENSE`.

## AndroidX, Material Components, Kotlin and related Android libraries

The Android application uses AndroidX, Material Components for Android, the
Kotlin runtime, and transitive support libraries. These are predominantly
licensed under the Apache License 2.0. Their embedded archive notices are
preserved by the Android build where supplied. A complete copy of Apache 2.0
is additionally included as `licenses/Apache-2.0.txt` in the application.

## NanoHTTPD

- Component: `org.nanohttpd:nanohttpd:2.3.1`
- License: BSD 3-Clause
- Copyright: 2012-2016 nanohttpd

The complete license is included as
`licenses/NanoHTTPD-BSD-3-Clause.txt` in the Android application.

## Google ML Kit

The Android image-analysis feature uses these bundled ML Kit artifacts:

- `com.google.mlkit:text-recognition:16.0.1`
- `com.google.mlkit:image-labeling:17.0.9`

ML Kit is not relicensed under MIT. Its use is subject to the ML Kit Terms of
Service and the incorporated Google APIs Terms of Service:

- <https://developers.google.com/ml-kit/terms>
- <https://developers.google.com/terms>

ML Kit's terms state that image/text processing occurs on-device, while its
APIs can contact Google for updates, accelerator compatibility information,
and performance/utilization metrics. Product privacy disclosures must account
for those terms and behavior.

## Web UI packages

The Svelte Web UI includes third-party JavaScript and CSS packages resolved by
`upstream/llama.cpp/tools/ui/package-lock.json`. Their complete installed
license texts and exact versions are generated into
`third_party_licenses/WEB_UI_NOTICES.txt` by `node
scripts/generate-webui-license-notices.mjs`. The Android build packages that
generated file as `licenses/WEB_UI_NOTICES.txt`.

## Models and user content

No GGUF model is distributed in this repository or APK. Models imported or
downloaded by a user are governed by the model publisher's separate license
and acceptable-use terms. The repository's MIT license does not apply to such
models or user content.
