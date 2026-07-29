# InetMind trial and content-report API

This small first-party HTTPS service supports the Google Play build without
handling chat inference:

- `POST /inetmind/v1/trial/status` creates or refreshes the three-day trial for
  a pseudonymous installation hash.
- `POST /inetmind/v1/reports` stores a user-confirmed AI response report.
- `GET /inetmind/v1/health` exposes no private data and is suitable for
  deployment monitoring.

The service never receives prompts, conversations, files or models during trial
activation. A content report contains only the selected reason, the explicitly
confirmed response excerpt, app version and locale.

## Private server configuration

Deploy `public/` below the HTTPS vhost at `inetmind/v1/`. Create the following
file outside the document root and never commit it:

`<subscription-root>/private/inetmind-api/config.php`

```php
<?php
return [
    'rateSecret' => '<at least 32 cryptographically random bytes>',
];
```

The PHP-FPM site user needs read access to `config.php` and write access to its
parent directory. SQLite creates `inetmind.sqlite3` there. Reports are removed
after 90 days and inactive trial records after 400 days. Rate-limit records use
a rotating HMAC of the remote address; plaintext IP addresses are not stored.

The production `.htaccess` explicitly sets `INETMIND_PRIVATE_ROOT` to the
Plesk subscription's private directory. Change that value when deploying to a
different server layout. Without it, the service derives the private directory
from the deployed script path and fails closed with HTTP 503 if no
configuration exists.
