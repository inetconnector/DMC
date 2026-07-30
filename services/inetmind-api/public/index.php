<?php
declare(strict_types=1);

const TRIAL_SECONDS = 3 * 24 * 60 * 60;
const REPORT_RETENTION_SECONDS = 90 * 24 * 60 * 60;
const TRIAL_RETENTION_SECONDS = 400 * 24 * 60 * 60;
const MAX_REQUEST_BYTES = 16_384;
const MAX_REPORT_CHARS = 4_000;

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header('Referrer-Policy: no-referrer');

try {
    $privateRoot = getenv('INETMIND_PRIVATE_ROOT');
    if (!is_string($privateRoot) || $privateRoot === '') {
        // Production layout: <subscription>/<vhost>/inetmind/v1/index.php.
        $privateRoot = dirname(__DIR__, 3) . '/private/inetmind-api';
    }
    $configPath = $privateRoot . '/config.php';
    if (!is_file($configPath)) {
        respond(503, ['error' => 'Service configuration is unavailable.']);
    }
    $config = require $configPath;
    $rateSecret = is_array($config) ? (string) ($config['rateSecret'] ?? '') : '';
    if (strlen($rateSecret) < 32) {
        respond(503, ['error' => 'Service configuration is invalid.']);
    }

    if (!is_dir($privateRoot) && !mkdir($privateRoot, 0750, true) && !is_dir($privateRoot)) {
        throw new RuntimeException('Unable to initialize private storage.');
    }
    $db = openDatabase($privateRoot . '/inetmind.sqlite3');
    removeExpiredData($db);

    $method = strtoupper((string) ($_SERVER['REQUEST_METHOD'] ?? 'GET'));
    $path = trim((string) parse_url((string) ($_SERVER['REQUEST_URI'] ?? '/'), PHP_URL_PATH), '/');
    $path = preg_replace('#^inetmind/v1/?#', '', $path) ?? '';

    if ($method === 'GET' && ($path === '' || $path === 'health')) {
        respond(200, [
            'service' => 'inetmind-api',
            'status' => 'ok',
            'trialDays' => 3,
            'serverTime' => milliseconds(time()),
        ]);
    }

    if ($method !== 'POST') {
        header('Allow: GET, POST');
        respond(405, ['error' => 'Method not allowed.']);
    }

    $body = readJsonBody();
    $rateKey = rateKey($rateSecret);

    if ($path === 'trial/status') {
        enforceRateLimit($db, 'trial:' . $rateKey, 60, 24 * 60 * 60);
        handleTrialStatus($db, $body);
    }

    if ($path === 'review/access') {
        handleReviewAccess($db, $body, $config, $rateKey);
    }

    if ($path === 'reports') {
        enforceRateLimit($db, 'report:' . $rateKey, 10, 24 * 60 * 60);
        handleReport($db, $body);
    }

    respond(404, ['error' => 'Endpoint not found.']);
} catch (Throwable $error) {
    error_log('InetMind API error: ' . $error->getMessage());
    respond(500, ['error' => 'Internal server error.']);
}

function openDatabase(string $path): PDO
{
    $db = new PDO('sqlite:' . $path, null, null, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);
    $db->exec('PRAGMA journal_mode=WAL');
    $db->exec('PRAGMA busy_timeout=5000');
    $db->exec(
        'CREATE TABLE IF NOT EXISTS trials (
            installation_hash TEXT PRIMARY KEY,
            started_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            last_seen_at INTEGER NOT NULL,
            app_version TEXT NOT NULL,
            locale TEXT NOT NULL
        )'
    );
    $db->exec(
        'CREATE TABLE IF NOT EXISTS reports (
            id TEXT PRIMARY KEY,
            created_at INTEGER NOT NULL,
            reason TEXT NOT NULL,
            excerpt TEXT NOT NULL,
            app_version TEXT NOT NULL,
            locale TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT "new"
        )'
    );
    $db->exec(
        'CREATE TABLE IF NOT EXISTS rate_limits (
            rate_key TEXT PRIMARY KEY,
            window_started_at INTEGER NOT NULL,
            request_count INTEGER NOT NULL
        )'
    );
    return $db;
}

function removeExpiredData(PDO $db): void
{
    if (random_int(1, 100) !== 1) {
        return;
    }
    $now = time();
    $statement = $db->prepare('DELETE FROM reports WHERE created_at < ?');
    $statement->execute([$now - REPORT_RETENTION_SECONDS]);
    $statement = $db->prepare('DELETE FROM trials WHERE last_seen_at < ?');
    $statement->execute([$now - TRIAL_RETENTION_SECONDS]);
    $statement = $db->prepare('DELETE FROM rate_limits WHERE window_started_at < ?');
    $statement->execute([$now - (2 * 24 * 60 * 60)]);
}

function handleTrialStatus(PDO $db, array $body): never
{
    $installationId = strtolower(requireString($body, 'installationId', 64));
    if (!preg_match('/^[a-f0-9]{64}$/', $installationId)) {
        respond(400, ['error' => 'Invalid installation identifier.']);
    }
    $appVersion = optionalString($body, 'appVersion', 64, 'unknown');
    $locale = optionalString($body, 'locale', 35, 'und');
    $now = time();

    $db->beginTransaction();
    try {
        $statement = $db->prepare(
            'INSERT OR IGNORE INTO trials
                (installation_hash, started_at, expires_at, last_seen_at, app_version, locale)
             VALUES (?, ?, ?, ?, ?, ?)'
        );
        $statement->execute([
            $installationId,
            $now,
            $now + TRIAL_SECONDS,
            $now,
            $appVersion,
            $locale,
        ]);
        $statement = $db->prepare(
            'UPDATE trials
             SET last_seen_at = ?, app_version = ?, locale = ?
             WHERE installation_hash = ?'
        );
        $statement->execute([$now, $appVersion, $locale, $installationId]);
        $statement = $db->prepare(
            'SELECT started_at, expires_at FROM trials WHERE installation_hash = ?'
        );
        $statement->execute([$installationId]);
        $trial = $statement->fetch();
        $db->commit();
    } catch (Throwable $error) {
        $db->rollBack();
        throw $error;
    }

    if (!is_array($trial)) {
        throw new RuntimeException('Unable to load trial.');
    }
    respond(200, [
        'active' => $now < (int) $trial['expires_at'],
        'startedAt' => milliseconds((int) $trial['started_at']),
        'expiresAt' => milliseconds((int) $trial['expires_at']),
        'serverTime' => milliseconds($now),
    ]);
}

function handleReviewAccess(PDO $db, array $body, array $config, string $rateKey): never
{
    $installationId = strtolower(requireString($body, 'installationId', 64));
    if (!preg_match('/^[a-f0-9]{64}$/', $installationId)) {
        respond(400, ['error' => 'Invalid installation identifier.']);
    }
    $appVersion = optionalString($body, 'appVersion', 64, 'unknown');
    $locale = optionalString($body, 'locale', 35, 'und');
    $codeHash = (string) ($config['reviewAccessCodeHash'] ?? '');
    $tokenSecret = (string) ($config['reviewTokenSecret'] ?? '');
    if ($codeHash === '' || strlen($tokenSecret) < 32) {
        respond(503, ['error' => 'Review access is unavailable.']);
    }

    if (isset($body['accessCode'])) {
        enforceRateLimit($db, 'review-code:' . $rateKey, 20, 24 * 60 * 60);
        $accessCode = requireString($body, 'accessCode', 128);
        if (!password_verify($accessCode, $codeHash)) {
            respond(403, ['error' => 'Invalid review access code.']);
        }
        $accessToken = issueReviewAccessToken($installationId, $tokenSecret);
        respond(200, [
            'active' => true,
            'accessToken' => $accessToken,
            'appVersion' => $appVersion,
            'locale' => $locale,
            'serverTime' => milliseconds(time()),
        ]);
    }

    if (isset($body['accessToken'])) {
        enforceRateLimit($db, 'review-token:' . $rateKey, 240, 24 * 60 * 60);
        $accessToken = requireString($body, 'accessToken', 1024);
        if (!verifyReviewAccessToken($accessToken, $installationId, $tokenSecret)) {
            respond(403, ['error' => 'Invalid review access token.']);
        }
        respond(200, [
            'active' => true,
            'appVersion' => $appVersion,
            'locale' => $locale,
            'serverTime' => milliseconds(time()),
        ]);
    }

    respond(400, ['error' => 'Review access code or token required.']);
}

function issueReviewAccessToken(string $installationId, string $secret): string
{
    $payload = json_encode([
        'version' => 1,
        'installationId' => $installationId,
        'issuedAt' => time(),
    ], JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR);
    $encodedPayload = base64UrlEncode($payload);
    $signature = hash_hmac('sha256', $encodedPayload, $secret);
    return $encodedPayload . '.' . $signature;
}

function verifyReviewAccessToken(
    string $token,
    string $installationId,
    string $secret
): bool {
    if (!preg_match('/^([A-Za-z0-9_-]{20,900})\.([a-f0-9]{64})$/', $token, $matches)) {
        return false;
    }
    $encodedPayload = $matches[1];
    $expectedSignature = hash_hmac('sha256', $encodedPayload, $secret);
    if (!hash_equals($expectedSignature, $matches[2])) {
        return false;
    }
    $decodedPayload = base64UrlDecode($encodedPayload);
    if ($decodedPayload === null) {
        return false;
    }
    try {
        $payload = json_decode($decodedPayload, true, 8, JSON_THROW_ON_ERROR);
    } catch (JsonException) {
        return false;
    }
    return is_array($payload)
        && ($payload['version'] ?? null) === 1
        && hash_equals((string) ($payload['installationId'] ?? ''), $installationId);
}

function base64UrlEncode(string $value): string
{
    return rtrim(strtr(base64_encode($value), '+/', '-_'), '=');
}

function base64UrlDecode(string $value): ?string
{
    $padding = (4 - strlen($value) % 4) % 4;
    $decoded = base64_decode(
        strtr($value . str_repeat('=', $padding), '-_', '+/'),
        true
    );
    return is_string($decoded) ? $decoded : null;
}

function handleReport(PDO $db, array $body): never
{
    $allowedReasons = ['harmful', 'hateful', 'sexual', 'illegal', 'inaccurate', 'other'];
    $reason = requireString($body, 'reason', 48);
    if (!in_array($reason, $allowedReasons, true)) {
        respond(400, ['error' => 'Invalid report reason.']);
    }
    $excerpt = trim(requireString($body, 'excerpt', MAX_REPORT_CHARS));
    if ($excerpt === '') {
        respond(400, ['error' => 'The report excerpt is empty.']);
    }
    $appVersion = optionalString($body, 'appVersion', 64, 'unknown');
    $locale = optionalString($body, 'locale', 35, 'und');
    $reportId = bin2hex(random_bytes(12));

    $statement = $db->prepare(
        'INSERT INTO reports (id, created_at, reason, excerpt, app_version, locale)
         VALUES (?, ?, ?, ?, ?, ?)'
    );
    $statement->execute([$reportId, time(), $reason, $excerpt, $appVersion, $locale]);

    respond(201, ['reportId' => $reportId]);
}

function enforceRateLimit(PDO $db, string $key, int $limit, int $windowSeconds): void
{
    $now = time();
    $db->beginTransaction();
    try {
        $statement = $db->prepare(
            'SELECT window_started_at, request_count FROM rate_limits WHERE rate_key = ?'
        );
        $statement->execute([$key]);
        $current = $statement->fetch();
        if (!is_array($current) || $now - (int) $current['window_started_at'] >= $windowSeconds) {
            $statement = $db->prepare(
                'INSERT INTO rate_limits (rate_key, window_started_at, request_count)
                 VALUES (?, ?, 1)
                 ON CONFLICT(rate_key) DO UPDATE SET window_started_at = excluded.window_started_at,
                     request_count = 1'
            );
            $statement->execute([$key, $now]);
        } else {
            if ((int) $current['request_count'] >= $limit) {
                $db->rollBack();
                header('Retry-After: ' . max(1, $windowSeconds - ($now - (int) $current['window_started_at'])));
                respond(429, ['error' => 'Too many requests. Try again later.']);
            }
            $statement = $db->prepare(
                'UPDATE rate_limits SET request_count = request_count + 1 WHERE rate_key = ?'
            );
            $statement->execute([$key]);
        }
        $db->commit();
    } catch (Throwable $error) {
        if ($db->inTransaction()) {
            $db->rollBack();
        }
        throw $error;
    }
}

function rateKey(string $secret): string
{
    $remoteAddress = (string) ($_SERVER['REMOTE_ADDR'] ?? 'unknown');
    return hash_hmac('sha256', gmdate('Y-m-d') . ':' . $remoteAddress, $secret);
}

function readJsonBody(): array
{
    $contentLength = (int) ($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($contentLength > MAX_REQUEST_BYTES) {
        respond(413, ['error' => 'Request is too large.']);
    }
    $raw = file_get_contents('php://input', false, null, 0, MAX_REQUEST_BYTES + 1);
    if (!is_string($raw) || strlen($raw) > MAX_REQUEST_BYTES) {
        respond(413, ['error' => 'Request is too large.']);
    }
    try {
        $decoded = json_decode($raw, true, 32, JSON_THROW_ON_ERROR);
    } catch (JsonException) {
        respond(400, ['error' => 'Invalid JSON.']);
    }
    if (!is_array($decoded)) {
        respond(400, ['error' => 'JSON object required.']);
    }
    return $decoded;
}

function requireString(array $body, string $key, int $maxLength): string
{
    $value = $body[$key] ?? null;
    if (!is_string($value) || $value === '') {
        respond(400, ['error' => "Missing field: $key."]);
    }
    if (textLength($value) > $maxLength) {
        respond(400, ['error' => "Field is too long: $key."]);
    }
    return $value;
}

function optionalString(array $body, string $key, int $maxLength, string $fallback): string
{
    if (!isset($body[$key])) {
        return $fallback;
    }
    return requireString($body, $key, $maxLength);
}

function textLength(string $value): int
{
    return function_exists('mb_strlen') ? mb_strlen($value, 'UTF-8') : strlen($value);
}

function milliseconds(int $seconds): int
{
    return $seconds * 1000;
}

function respond(int $status, array $body): never
{
    http_response_code($status);
    echo json_encode($body, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}
