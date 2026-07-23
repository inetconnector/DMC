# InetMind brand and Google Play review

Last reviewed: 2026-07-23

## Decision

The Android product is presented as:

- English: **InetMind - Local AI**
- German: **InetMind - Lokale KI**
- Technical context engine: **Deterministic Multiresolution Context (DMC)**
- Immutable Android/Google Play package: `com.inetconnector.dmc`

The repository can continue to use DMC as its technical project and algorithm
name. The consumer-facing app name is InetMind.

## Why the old name was changed

The Play draft was initially named `Offline KI - OLLAMA UI -DMC`. This was both
technically misleading and unnecessarily risky:

- The Android runtime uses the pinned `llama.cpp` fork, not the Ollama service.
- Ollama's current terms state that no right to use its branding is granted.
- DMC is an accurate acronym for this project's context algorithm, but a
  three-letter product mark is weak, crowded and difficult to search for.

All visible Play titles therefore remove `OLLAMA`. Technical documentation may
still mention an existing Ollama model source where that is factually relevant.

## Name screening

An exact-name web search and searches limited to publicly indexed EUIPO, DPMA
and WIPO results did not identify an obvious software or AI product named
`InetMind` on 2026-07-23. By contrast, alternatives such as ContextVault,
ContextForge, ContextWeave and MindSpan already had clear software or AI uses.

This is a preliminary collision screen, not a formal trademark clearance.
Search-engine coverage of trademark registers is incomplete. Before a large
paid marketing launch, order a professional similarity search covering:

- exact and phonetic variants;
- EU and German word/device marks;
- Nice classes relevant to downloadable software and AI services;
- company names, app stores, domains and unregistered market use.

## Google Play policy implications

### AI-generated content

Google Play requires generative-AI apps to provide an in-app way to report or
flag offensive generated content. InetMind now shows a flag action below every
nonempty assistant response. The Android dialog collects a reason in-app and,
after confirmation, prepares an email draft containing at most 4,000
characters. The user can review or discard the draft before anything is sent.

### Data safety

Core inference, chat storage, image content and imported knowledge stay local,
but the bundled Google ML Kit SDKs collect technical diagnostics and usage
analytics. The Play Data safety form must therefore not claim that no data is
collected. The maintained answer matrix is in `play/DATA_SAFETY.md`.

### Comparison with other Play listings

The public Activity Launcher Pro listing and several competing offline-AI
listings currently declare that no data is collected or shared. Those
declarations cannot simply be copied to InetMind:

- Activity Launcher Pro has a different dependency and data-flow profile.
- Some offline-AI listings make broad "100% offline" claims while also offering
  model downloads, custom endpoints, web search, ads, analytics, or separate
  speech/vision services.
- InetMind bundles Google ML Kit for local image analysis. Google's own SDK
  disclosure lists technical diagnostics, usage analytics, and a
  per-installation identifier even though image/text inputs and outputs remain
  on-device.

InetMind therefore uses the narrower, verifiable claim that core prompts,
chats, models, attachments, and knowledge modules are processed locally and are
not automatically sent to an InetConnector server. User-triggered downloads,
external endpoints, speech providers, ML Kit diagnostics, support, and content
reports are disclosed separately.

### Health content

Optional ICD and generic offline reference modules can expose health
information. Complete the Play Health apps declaration and retain the
store/in-app disclaimer that reference data does not provide diagnosis,
treatment or professional medical advice.

### Privacy policy

The version-controlled bilingual policy is `privacy/index.html` and is
published at:

<https://inetconnector.github.io/DMC/privacy/>

It is linked from Android settings and should be entered into the Play Console
privacy-policy field.

## Authoritative sources

- Ollama terms: <https://ollama.com/terms>
- Google Play AI-generated content policy:
  <https://support.google.com/googleplay/android-developer/answer/13985936>
- Google ML Kit Android data disclosure:
  <https://developers.google.com/ml-kit/android-data-disclosure>
- Google Play health apps policy:
  <https://support.google.com/googleplay/android-developer/answer/16679511>
- Activity Launcher Pro public Play listing:
  <https://play.google.com/store/apps/details?id=com.inetconnector.activitylauncher>
- Example offline-AI listing with a no-collection declaration:
  <https://play.google.com/store/apps/details?id=com.hiro.localllm>
