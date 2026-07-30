# Google Play Data Safety - Local AI - DMC

Last audited: 2026-07-30

Package: `com.inetconnector.dmc`
Play flavor: `playRelease`
Privacy policy: <https://inetconnector.github.io/DMC/privacy/play/>

This document is the source of truth for the Google Play Data safety form.
Recheck the final dependency graph, Play SDK Index and app behavior before every
release. The answers below apply only to the medical-free Google Play flavor,
not to the separately distributed `full` flavor.

## Required Answers

- Does the app collect required user data types? **Yes**
- Is all collected user data encrypted in transit? **Yes**, using HTTPS
- Can users request deletion? **Yes**, through
  `apps@inetconnector.com`; local app data is controlled directly by the user
- Is developer-collected data shared with third parties? **No**
- Does the app contain ads? **No**
- Is an account required? **No**
- Does the Play flavor provide health or medical functionality? **No**

## Developer-Collected Data

### Device or other IDs

For the user-confirmed three-day trial, the app sends:

- a pseudonymous SHA-256 identifier derived from the package name and Android
  installation identifier;
- app version;
- device language.

The identifier is collected, not shared, and is required for app functionality
and fraud prevention. Chats, files, models and prompts are not included. Trial
records are deleted no later than 400 days after their last access.

Authorized Google Play reviewers can send the same identifier, app version and
language together with the Console-only review code. The server verifies the
code and returns an installation-bound signed entitlement. The cleartext code
and entitlement are not stored server-side; the entitlement remains locally in
the app. This does not add a new user-data category or change the purposes
above.

### Optional user-generated content

The in-app report flow sends data only after explicit confirmation:

- selected report reason;
- up to 4,000 characters of the reported AI response;
- app version;
- device language.

This data is collected, not shared, optional, and used for app functionality,
safety, security and compliance. Reports are retained for no longer than 90
days. The app displays a report ID for support and deletion requests.

### App info and performance

Google ML Kit may process technical diagnostics and usage information according
to Google's SDK disclosure:

- diagnostics and performance data;
- app/device configuration;
- a per-installation identifier that Google states is not intended to identify
  a user or physical device.

Image, text and OCR content is processed on-device by ML Kit. Recheck the Play
SDK Index and Google's current disclosure before submitting each release.

## Google Play Billing

The permanent unlock is a non-consumable one-time product handled by Google
Play. Google processes checkout, payment information and receipts under its own
terms. The app queries purchase entitlement through Play Billing. InetConnector
does not receive payment-card or bank-account data through this implementation.

## Not Collected By The Developer

- full chat history;
- prompts and AI responses, except an excerpt voluntarily submitted in a report;
- raw images, camera photos or OCR results;
- audio recordings;
- imported files;
- downloaded GGUF models;
- contacts, precise or approximate location;
- financial information or purchase history;
- health information;
- browsing history.

## Important Disclosures

- Android cloud backup is disabled.
- Voice input uses the Android speech recognizer selected on the device. Its
  provider may process speech locally or online under its own terms.
- User-configured MCP servers, model download hosts and external links are
  destinations selected by the user.
- The Play flavor contains no offline medical knowledge modules, ICD import,
  medical diagnosis or treatment feature.
- The optional `full` flavor is not the Play artifact and must never be uploaded
  to the Play Console under this declaration.

## Console Declarations

- Complete the **AI-generated content** declaration and reference the in-app
  report mechanism.
- Answer **No health functionality** for the Play flavor. If the Console asks a
  differently worded question, verify the exact wording rather than inferring.
- Select an age group suitable for general-purpose local generative AI; do not
  target children. The approved target selection is **18 and over only**;
  **Restrict minor access** must be enabled in Play Console.
- Set ads to **No**.
- Set the public support email to `apps@inetconnector.com`.
- Use the Play privacy URL above, not the privacy policy for the full flavor.
- Verify that the app is listed as free to install. Access after the three-day
  trial is sold only through the configured one-time product.

## Evidence

- Build variants: `android/llama.android/app/build.gradle.kts`
- Trial client:
  `android/llama.android/app/src/main/java/com/inetconnector/dmc/billing/TrialAccessService.kt`
- Billing client:
  `android/llama.android/app/src/main/java/com/inetconnector/dmc/billing/PlayBillingManager.kt`
- API: `services/inetmind-api/public/index.php`
- Privacy source: `privacy/play/index.html`
- Google ML Kit disclosure:
  <https://developers.google.com/ml-kit/android-data-disclosure>
