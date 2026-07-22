# Google Play publishing for DMC

This guide configures Google Play Developer API access specifically for the DMC
Android app. The public Android package is `com.inetconnector.dmc`.

## Fixed DMC values

| Purpose | Value |
| --- | --- |
| Play app name | DMC - Local AI Chat |
| Android package | `com.inetconnector.dmc` |
| Suggested Cloud project name | DMC Play Publishing |
| Suggested Cloud project ID | `inetconnector-dmc-play` (must be globally unique) |
| Service account name/ID | `dmc-gplay` |
| Local gplay profile | `dmc` |
| Private JSON key | `C:\Users\frede\.gplay\keys\dmc-play.json` |
| Repository package pin | `.gplay/config.yaml` |

## Current publication state

- Play app `com.inetconnector.dmc` exists.
- Signed version `1.0.0 (1)` is processed with status `completed` in the
  internal test track.
- Localized release notes for nine locales are attached to that release.
- Validated store listings for the same nine locales, German and English phone
  screenshots, the icon, and the feature graphic are versioned under `play/`.
- The shared service account can manage releases but currently lacks the
  DMC-specific **Manage store presence** permission. Until that permission is
  granted, Google rejects commits containing store text or media. The local
  files are complete; no secret or generated bundle is committed.
- Price `4.99 EUR`, Data safety, content declarations, target audience, ads,
  app access, and the final production review remain Console-only work.

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
2. Use **DMC - Local AI Chat** as the initial name.
3. Choose **App**, not Game, and select the default language.
4. Select **Paid** before the first publication if DMC is to cost `4.99 EUR`.
   A Play app that has once been offered for free cannot later become paid.
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

Restrict app access to **DMC - Local AI Chat** / `com.inetconnector.dmc` and
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
$env:ANDROID_BUILD_VARIANT = "release"
.\build-android.bat
```

Then validate the exact current artifact before any upload:

```powershell
gplay validate `
  --package com.inetconnector.dmc `
  --bundle ".\publish\com.inetconnector.dmc\1.0.0+1\com.inetconnector.dmc-1.0.0+1-release.aab" `
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

Five reviewed German and two English phone screenshots are versioned under
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
  --bundle ".\publish\com.inetconnector.dmc\1.0.0+1\com.inetconnector.dmc-1.0.0+1-release.aab" `
  --listings-dir .\play\metadata `
  --screenshots-dir .\play\screenshots `
  --release-notes '@.\play\release-notes\1.0.0.json' `
  --wait
```

Version code `1` has already been uploaded and cannot be reused. The next
bundle must increment `versionCode` before upload.

After granting **Manage store presence**, push the local listings and media.
Use a fresh edit for the graphics and commit only after `images sync` reports
no errors. The expected graphics layout is
`play/graphics/<locale>/images/{icon.png,featureGraphic.png}`.

After internal testing, complete the Play Console declarations, configure the
merchant/payments profile, set the app price to `4.99 EUR`, review generated
local prices, and only then prepare production. New personal developer accounts
may need a closed test with 12 continuously opted-in testers for 14 days before
production access is granted.

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
