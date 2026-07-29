# Google Play Console declarations

Last audited: 2026-07-29

Package: `com.inetconnector.dmc`

Store name: **Local AI - DMC**

Artifact scope: medical-free `playRelease` version `1.1.0 (3)`

This file is the human-readable source of truth for Console-only declarations.
It does not replace the answers stored in Google Play Console. Recheck every
answer against the current artifact and the exact wording shown by Google
before submitting an update.

## Target audience

- Target age group: **18 and over only**
- Include any age group below 18: **No**
- Designed for children: **No**
- Families programme: **No**
- Restrict minor access: **Must be enabled in Play Console**

The target-audience choice is not an IARC rating. Google and the regional IARC
authorities calculate the displayed content ratings from the separate content
rating questionnaire. Do not manipulate questionnaire answers merely to force
an 18 rating.

## Ads and monetization

- Contains ads: **No**
- Free to install: **Yes**
- Subscription: **No**
- Three-day trial: **Yes, server-backed and started once after explicit user
  confirmation**
- Permanent unlock: **One non-consumable Google Play purchase**
- Product ID: `inetmind_full_unlock`
- German consumer price: `4.99 EUR`

## App access

- Account required: **No**
- Login required: **No**
- Membership required: **No**
- External hardware required: **No**
- Core review access restricted: **No**

Reviewer note:

> Local AI - DMC runs compatible GGUF language models locally on the Android
> device. No account or login is required. The app does not bundle a language
> model; reviewers may import or download a compatible GGUF model from the
> model manager. The app offers one three-day trial and a permanent one-time
> Google Play unlock. The Google Play artifact contains no medical knowledge,
> diagnosis, treatment, or medical import functionality.

## Privacy policy

- Privacy-policy URL:
  `https://inetconnector.github.io/DMC/privacy/play/`
- Public HTTP status verified: **200**
- Contact for privacy and deletion requests: `apps@inetconnector.com`
- Local Android cloud backup: **Disabled**

## Data safety

Use [`DATA_SAFETY.md`](DATA_SAFETY.md) as the detailed answer matrix.

Top-level answers:

- Does the app collect required user data types? **Yes**
- Is collected data encrypted in transit? **Yes**
- Can users request deletion? **Yes**
- Is developer-collected data shared with third parties? **No**
- Does the app contain ads? **No**

Declared collection:

- Pseudonymous installation identifier, app version, and language for trial
  activation and fraud prevention.
- Optional user-submitted report reason, up to 4,000 characters of the selected
  AI response, app version, and language after explicit confirmation.
- Google ML Kit technical diagnostics, usage information, app/device
  configuration, and its documented per-installation identifier.

Not collected by InetConnector:

- Full chats or prompts.
- Models or imported files.
- Raw camera images, attachments, OCR output, or audio recordings.
- Contacts or location.
- Payment-card or bank details.
- Health information.

Google Play processes purchases and receipts under its own terms. The Android
speech recognizer selected by the user may be local or network-backed and is
governed by its provider.

## AI-generated content

- Does the app generate content using AI? **Yes**
- Primary AI mode: **Text-to-text conversational chatbot**
- Inputs may include text, voice, images, and documents: **Yes**
- In-app reporting of offensive generated content: **Yes**
- Reporting requires leaving the app: **No**
- Report destination: InetConnector-controlled HTTPS endpoint
- Whole chat automatically transmitted with a report: **No**

The report action appears below non-empty assistant responses. It requires
explicit confirmation and sends only the bounded report fields disclosed in
the privacy policy.

## Health and medical functionality

- Health app: **No**
- Medical knowledge feature in the Play artifact: **No**
- Diagnosis or treatment feature: **No**
- ICD or medical-reference import in the Play artifact: **No**

The separately distributed `fullRelease` is outside this declaration and must
never be uploaded under this Play application.

## Other app-content declarations

- News or magazine app: **No**
- Government app: **No**
- Financial-services app: **No**
- Gambling or real-money gaming: **No**
- Dating app: **No**
- Social networking app: **No**
- User-to-user content sharing: **No**
- User account deletion declaration required: **No account exists**
- COVID-19 contact tracing or status functionality: **No**

## Content rating questionnaire

Select the category that most accurately describes a general-purpose local AI
productivity/tool application. Answer every content question according to the
exact current wording and the functionality shipped in `playRelease`.

The app itself does not bundle sexual, violent, gambling, drug, or other mature
editorial content. However, it is a general-purpose local generative-AI client
whose output depends on a user-selected model and prompt. Do not claim that
generated output is categorically impossible when the questionnaire expressly
asks about variable or user-generated AI content.

Submit the questionnaire and retain the generated IARC certificate. The
regional authorities, not the developer, determine the displayed rating.

## Final Console sequence

1. Open **Policy and programmes > App content**.
2. Save the privacy-policy URL.
3. Set **Ads** to No.
4. Set **App access** to unrestricted/no login and add the reviewer note above.
5. Set **Target audience** to **18 and over only**.
6. Enable **Restrict minor access**.
7. Complete and submit the IARC content rating questionnaire truthfully.
8. Complete **Data safety** from `DATA_SAFETY.md`.
9. Declare the generative-AI chatbot and its in-app reporting mechanism.
10. Declare no health or medical functionality for `playRelease`.
11. Resolve every remaining App-content task and policy message.
12. Review all generated regional prices.
13. Change production `1.1.0 (3)` from `draft` to `completed` and send it for
    review.

The Android Publisher API rejects a completed release while the application
itself remains a draft. On 2026-07-29 Google returned:

`Only releases with status draft may be created on draft app.`

Therefore steps 1-11 must be completed in Play Console before the production
release can be submitted through the API.
