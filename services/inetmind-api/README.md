# InetMind access and content-report API

This small first-party HTTPS service supports the Google Play build without
handling chat inference:

- `POST /inetmind/v1/trial/status` creates or refreshes the three-day trial for
  a pseudonymous installation hash.
- `POST /inetmind/v1/review/access` exchanges the reusable Google Play review
  code for an installation-bound, HMAC-signed entitlement and validates that
  entitlement on later starts.
- `POST /inetmind/v1/reports` stores a user-confirmed AI response report.
- `GET /inetmind/v1/health` exposes no private data and is suitable for
  deployment monitoring.

The service never receives prompts, conversations, files or models during
trial or review-access activation. Review entitlements are signed and returned
to the app without a server-side entitlement record. A content report contains
only the selected reason, the explicitly confirmed response excerpt, app
version and locale.

## Private server configuration

Deploy `public/` below the HTTPS vhost at `inetmind/v1/`. Create the following
file outside the document root and never commit it:

`<subscription-root>/private/inetmind-api/config.php`

```php
<?php
return [
    'rateSecret' => '<at least 32 cryptographically random bytes>',
    'reviewAccessCodeHash' => '<password_hash output for the review code>',
    'reviewTokenSecret' => '<at least 32 cryptographically random bytes>',
];
```

Generate a long random review code in a password manager and keep its cleartext
only there and in the Play Console app-access instructions. Generate
`reviewAccessCodeHash` with PHP's `password_hash()` and use a separate random
secret for `reviewTokenSecret`. Never commit or embed any of these values in
the Android app. The same review code must remain reusable, location-independent
and available for later update reviews.

The PHP-FPM site user needs read access to `config.php` and write access to its
parent directory. SQLite creates `inetmind.sqlite3` there. Reports are removed
after 90 days and inactive trial records after 400 days. Rate-limit records use
a rotating HMAC of the remote address; plaintext IP addresses are not stored.

The repository includes a CLI helper that updates the existing private
configuration atomically, preserves its permissions and prints the cleartext
code exactly once:

```sh
php services/inetmind-api/tools/configure_review_access.php \
  /absolute/private/inetmind-api/config.php
```

Capture the printed code directly in the Play Console and a password manager.
The helper refuses to replace an existing review configuration unless
`--rotate` is supplied deliberately. Rotating invalidates locally stored review
tokens and requires updating the code in Play Console.

The production `.htaccess` explicitly sets `INETMIND_PRIVATE_ROOT` to the
Plesk subscription's private directory. Change that value when deploying to a
different server layout. Without it, the service derives the private directory
from the deployed script path and fails closed with HTTP 503 if no
configuration exists.
