# Google Play Data Safety - InetMind

Last audited: 2026-07-23

Package: `com.inetconnector.dmc`

This file is the source of truth for the Google Play Data safety form. Recheck
the final dependency graph and app behavior before every release. The form
itself must be completed in Play Console because the available `gplay` CLI
expects Google's exported protobuf payload rather than a human-readable form.

## Required Answers

- Does the app collect or share any required user data types? **Yes**
- Is all collected user data encrypted in transit? **Yes**
- Do users have a way to request deletion? **Yes, where the developer receives
  an email report; local app data is controlled directly by the user**
- Is data shared with third parties? **No**, based on Google's current ML Kit
  disclosure. Recheck this if dependencies change.

## Data Types To Declare

### App info and performance

- **Diagnostics**
- Collected, not shared
- Purpose: app functionality, analytics
- Processing: Google ML Kit SDK technical diagnostics and usage analytics
- Examples documented by Google: device/app information, performance metrics,
  API configuration, input/output sizes, feature versions, event types and
  error codes
- Raw chat, image and OCR text content is not included in this declaration;
  Google states that ML Kit processes those inputs and outputs on-device

### Device or other IDs

- **Device or other IDs**
- Collected, not shared
- Purpose: app functionality, analytics
- Processing: Google ML Kit per-installation identifier that Google says is not
  intended to identify a user or physical device

### Optional user-initiated support and content reports

The app prepares an email draft only after explicit confirmation. The user can
review or discard it before sending. If the user sends the email, the developer
receives:

- **User IDs / email address** when supplied by the user's email account
- **Other user-generated content**: selected report reason and up to 4,000
  characters of the reported AI response
- Collected, not shared
- Optional
- Purpose: app functionality, developer communications, fraud prevention,
  security and compliance
- Deletion request: `apps@inetconnector.com`

If Play treats ordinary support email outside the app as outside the app's data
collection, keep the conservative declaration unless Play support confirms that
it may be omitted.

## Not Collected By The Developer

- Full chat history
- Prompts and AI responses, except an excerpt voluntarily sent in a report
- Raw images, camera photos or OCR results
- Audio recordings
- Imported files or offline knowledge modules
- Downloaded GGUF models
- Contacts, location, financial information, health information or browsing
  history

## Important Product Disclosures

- Local Android backup is disabled.
- Voice input is provided by the Android speech recognizer selected on the
  device. Its provider may process speech under its own privacy terms.
- User-configured MCP servers, download hosts and external links are separate
  third-party destinations chosen by the user.
- The privacy policy URL is
  `https://inetconnector.github.io/DMC/privacy/`.

## Console-Only Declarations

- Complete the **AI-generated content** declaration.
- Complete the **Health apps** declaration because optional ICD/reference
  modules can surface health information.
- Declare that the app is not a medical device and include the existing
  medical disclaimer.
- Use an age group consistent with general-purpose local generative AI and
  do not target children.
- Set the public support email to `apps@inetconnector.com`.

## Evidence

- Android manifest:
  `android/llama.android/app/src/main/AndroidManifest.xml`
- Dependencies:
  `android/llama.android/app/build.gradle.kts`
- Google ML Kit disclosure:
  <https://developers.google.com/ml-kit/android-data-disclosure>
- Privacy policy source: `privacy/index.html`
