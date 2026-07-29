# Google Play publishing for Local AI - DMC

This guide configures Google Play Developer API access for the Local AI - DMC Android
app. DMC remains the native long-context engine and the public Android package
remains `com.inetconnector.dmc`.

## Fixed DMC values

| Purpose | Value |
| --- | --- |
| Play app name | Local AI - DMC |
| Android package | `com.inetconnector.dmc` |
| Suggested Cloud project name | Local AI DMC Play Publishing |
| Suggested Cloud project ID | `inetconnector-dmc-play` (must be globally unique) |
| Service account name/ID | Existing shared publisher account or `inetmind-gplay` |
| Local gplay profile | `dmc` |
| Private JSON key | `C:\Users\frede\.gplay\keys\dmc-play.json` |
| Repository package pin | `.gplay/config.yaml` |

## Current publication state

- Play app `com.inetconnector.dmc` exists.
- Signed version `1.1.0 (3)` is processed with status `completed` in the
  internal test track and is stored as a validated `draft` in production.
  It is not public yet.
- Its `playRelease` flavor contains no
  medical knowledge modules, no medical import path, and no diagnosis or
  treatment functionality. The separate `fullRelease` flavor retains optional
  offline knowledge modules and is not the Play artifact.
- The Play app is free to install. It starts one server-backed three-day trial,
  then requires the permanent non-consumable product
  `inetmind_full_unlock`. The product is active in all nine listing languages;
  the confirmed German end-user price is `4.99 EUR`.
- Localized release notes for all nine locales are versioned for `1.1.0`.
- Validated store listings for the same nine locales, German and English phone
  screenshots, the icon, and the feature graphic are versioned under `play/`.
- The shared service account has effective publishing permissions. All nine
  localized listings, four German and two English screenshots, the 512x512
  icon, and the 1024x500 feature graphic were uploaded and verified remotely.
- The Play privacy policy is public with HTTP 200 at
  `https://inetconnector.github.io/DMC/privacy/play/`.
- Canonical release validation reports zero automated blockers. Data safety,
  AI-generated-content, target audience, ads, app access, content rating, and
  the final policy review remain Console-only confirmations. The production
  draft must not be changed to `completed` until those declarations have been
  manually reviewed.
- The approved target audience is **18 and over only**. **Restrict minor
  access** must be enabled in Play Console. Exact answers and reviewer text are maintained in
  `play/CONSOLE_DECLARATIONS.md`.

A dedicated service account is optional. Google Play allows the existing
publishing service account to receive access to several selected apps. This
machine currently uses the existing publishing key for both the original
`activitylauncher` profile and a separate local `dmc` profile; the profile names
keep commands clear even though both profiles reference the same credential.

The service-account JSON and the Android upload keystore are different secrets:

- The service-account JSON authorizes the Google Play Developer API.
- The Android keystore signs APK/AAB files. DMC release builds obtain it through
  the `DMC_RELEASE_*` Gradle properties or environment variables.

Never commit either secret, paste its contents into logs, or store it in the
project directory.

## 1. Create the public app once

The Android Publisher API cannot create a normal public Play app. Before API
access can be tested, create the app manually in Google Play Console:

1. Select **Create app**.
2. Use **Local AI - DMC** as the name in every supported locale.
3. Choose **App**, not Game, and select the default language.
4. Select **Free**. The three-day trial and permanent unlock are implemented
   inside the app with Google Play Billing; they are not a paid-app trial.
5. Complete the declarations shown by Play Console. The package name is bound
   when the first AAB for `com.inetconnector.dmc` is uploaded.

Do not upload the old V1.0 AAB when publishing the current offline-knowledge
branch. Build a fresh signed AAB from the intended release commit. If version
code `1` has already been uploaded for this package, increment `versionCode`
before building; Google Play never accepts the same version code twice.

## 2. Prepare the Google Cloud project

1. Open Google Cloud Console.
2. Create **DMC Play Publishing** or choose an existing publishing project.
   `inetconnector-dmc-play` is a suitable project ID if it is still available.
3. Open **APIs & Services > Library**.
4. Find and enable **Google Play Android Developer API**
   (`androidpublisher.googleapis.com`).

Google no longer requires linking the Cloud project to the Play developer
account. Access is granted by inviting the service-account email in Play
Console.

## 3. Create the service account and JSON key

Skip this section when reusing an existing publishing service account. In that
case, grant its existing service-account email app-specific DMC permissions in
section 4 and register the existing JSON path under profile `dmc` in section 5.

1. Open **IAM & Admin > Service Accounts** in the selected Cloud project.
2. Select **Create service account**.
3. Use name and ID `dmc-gplay`.
4. Skip Cloud project roles. Play permissions are granted separately.
5. Open the created account and choose **Keys > Add key > Create new key**.
6. Select **JSON** and download the file once.
7. Move it outside the repository to:

   `C:\Users\frede\.gplay\keys\dmc-play.json`

The JSON contains a private key. Google cannot show that private key again. If
it is lost or exposed, delete that key in Cloud Console and create a replacement.

## 4. Grant minimal Play Console access

Copy the service-account email ending in
`@<project-id>.iam.gserviceaccount.com`, then open **Users and permissions** in
Google Play Console and invite it.

Restrict app access to **Local AI - DMC** / `com.inetconnector.dmc` and
grant only what the publishing workflow needs:

- View app information.
- Manage releases to testing tracks.
- Manage production releases.
- Manage the store presence/listing, including localized text and images.

Do not grant Admin, financial-data, order, subscription, or user-management
permissions unless a later, documented workflow genuinely requires them.

## 5. Register the DMC gplay profile

`gplay` is already installed on this machine and `.gplay/config.yaml` pins this
repository to `com.inetconnector.dmc`. After the JSON file exists, run:

```powershell
New-Item -ItemType Directory -Force "$env:USERPROFILE\.gplay\keys"

gplay auth login `
  --service-account "$env:USERPROFILE\.gplay\keys\dmc-play.json" `
  --profile dmc

gplay auth status
gplay auth doctor
gplay apps list --output table
```

Or use the repository check, which validates the JSON shape without printing
secret fields and then runs the same authentication checks:

```powershell
.\scripts\windows\setup-gplay-dmc.ps1
```

To reuse an existing publishing credential, pass its path explicitly. The
script creates the `dmc` alias without changing the current default profile:

```powershell
.\scripts\windows\setup-gplay-dmc.ps1 `
  -ServiceAccountPath "<existing-service-account.json>"
```

Use `-SetDefault` only when `dmc` should replace the current global default
profile. Regardless of whether the credential is dedicated or shared, the
script fails unless the selected profile can actually see
`com.inetconnector.dmc`.

The setup is complete only when the doctor succeeds and
`com.inetconnector.dmc` appears in the accessible app list.

## 6. Prepare and validate a release

Build a fresh signed AAB with the existing Android upload key, not the service
account key:

```powershell
$env:ANDROID_DISTRIBUTION = "play"
$env:ANDROID_BUILD_VARIANT = "release"
.\build-android.bat
```

Then validate the exact current artifact before any upload:

```powershell
gplay validate `
  --package com.inetconnector.dmc `
  --bundle ".\publish\com.inetconnector.dmc\1.1.0+3\com.inetconnector.dmc-1.1.0+3-play-release.aab" `
  --track internal `
  --strict
```

Store metadata uses one directory per locale containing `title.txt`,
`short_description.txt`, and `full_description.txt`. Validate it locally with:

```powershell
gplay metadata validate --dir .\play\metadata
```

Play media must include a 512x512 PNG icon, a 1024x500 feature graphic, and at
least two phone screenshots. Validate the complete submission before upload.

Four reviewed German and two English phone screenshots are versioned under
`play/screenshots/`. They are direct 1080x1920, 24-bit RGB captures from
Samsung SM-S931B and can be checked locally with:

```powershell
gplay validate screenshots `
  --dir .\play\screenshots `
  --locale de-DE `
  --pretty

gplay validate screenshots `
  --dir .\play\screenshots `
  --locale en-US `
  --pretty
```

## 7. Upload safely to internal testing first

Use the high-level command only after the validation report is clean:

```powershell
gplay release `
  --package com.inetconnector.dmc `
  --track internal `
  --bundle ".\publish\com.inetconnector.dmc\1.1.0+3\com.inetconnector.dmc-1.1.0+3-play-release.aab" `
  --listings-dir .\play\metadata `
  --screenshots-dir .\play\screenshots `
  --release-notes '@.\play\release-notes\1.1.0.json' `
  --wait
```

Version codes `1` and `2` have already been uploaded and cannot be reused. The
current candidate uses version code `3`; every later bundle must increment it.

Push the local listings and media through a fresh edit. Verify the uploaded
hashes before committing. The installed CLI currently probes the retired
`promoGraphic` type and may report a Google API 400 for that unused type even
when all supported assets upload successfully; validate the edit and query the
required asset types directly rather than treating that unrelated probe as an
upload failure. The expected graphics layout is
`play/graphics/<locale>/images/{icon.png,featureGraphic.png}`.

The one-time product from
`play/monetization/inetmind_full_unlock.json` is active. Google regional price
conversion was configured so the German end-user price is exactly `4.99 EUR`.
Before public rollout, verify one real licensed test purchase plus restore,
complete the Play Console declarations, and review all generated regional
prices.

## 8. Complete policy declarations

These declarations are not inferred safely from an AAB and must match the
current app behavior:

- Use `play/CONSOLE_DECLARATIONS.md` as the complete Console checklist.
- Select only **18 and over** as the target age group and enable
  **Restrict minor access**. Do not select any younger age group.
- Enter `https://inetconnector.github.io/DMC/privacy/play/` as the
  privacy-policy URL, after verifying HTTP 200.
- Complete Data safety from `play/DATA_SAFETY.md`. Do not select "no data
  collected": bundled ML Kit components document diagnostics, usage analytics,
  and a per-installation identifier; the trial service and optional response
  reports are disclosed separately.
- Declare no ads and no account/login requirement.
- Declare that `playRelease` has no health or medical functionality. Never use
  the separate full flavor or its optional modules when answering Play Console
  questions.
- Complete the generative-AI declaration. Local AI - DMC provides a localized flag
  action below assistant responses and submits confirmed reports directly to
  the controlled HTTPS endpoint without leaving the app.
- Review target audience and content rating conservatively. The app is not
  designed for children. The IARC rating remains the result of the separate,
  truthful content questionnaire and must not be forced to 18 by inaccurate
  answers.
- Confirm that users obtain compatible models separately and that model
  licences remain the user's responsibility.

The Play policy text is versioned in `privacy/play/index.html`; the brand and
policy rationale is in `docs/BRAND_AND_PLAY_REVIEW.md`.

## 9. Tag the exact release source

Create the final annotated tag only after the signed `playRelease` artifact,
medical-free artifact gate, metadata validation, product, privacy URL, and
mandatory Console declarations have all been verified:

```powershell
git status --short
git tag -a android-play-v1.1.0 -m "Local AI - DMC Android Play 1.1.0 (3)"
git push origin android-play-v1.1.0
```

Never move or reuse the tag. If any source changes after tagging, increment the
version and create a new tag.

## Security checklist

- `.gplay/config.yaml` is safe to commit; it contains only the package pin.
- `C:\Users\frede\.gplay\keys\dmc-play.json` stays outside Git.
- A shared publishing service account is supported, but grant it only the
  app-specific permissions each app requires and keep a separate local `dmc`
  profile name for unambiguous commands.
- Keep service-account permissions app-scoped and least-privileged.
- Revoke old JSON keys instead of accumulating unused active keys.
- Never use the service-account JSON as an Android signing key.
- Run `gplay auth doctor` after changing Play permissions; propagation may take
  a short time.

## Authoritative references

- Google Play Developer API setup:
  <https://developers.google.com/android-publisher/getting_started>
- Play Console user permissions:
  <https://support.google.com/googleplay/android-developer/answer/9844686>
- Installed gplay authentication model:
  <https://gplay.sh/docs/concepts/authentication/>
