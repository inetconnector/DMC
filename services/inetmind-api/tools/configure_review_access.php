<?php
declare(strict_types=1);

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This tool must be run from the command line.\n");
    exit(1);
}

$configPath = $argv[1] ?? '';
$rotate = in_array('--rotate', $argv, true);
if ($configPath === '' || !is_file($configPath)) {
    fwrite(STDERR, "Usage: php configure_review_access.php <config.php> [--rotate]\n");
    exit(2);
}

$config = require $configPath;
if (!is_array($config) || strlen((string) ($config['rateSecret'] ?? '')) < 32) {
    fwrite(STDERR, "The existing service configuration is invalid.\n");
    exit(3);
}
if (
    !$rotate
    && isset($config['reviewAccessCodeHash'], $config['reviewTokenSecret'])
) {
    fwrite(
        STDERR,
        "Review access is already configured. Use --rotate only for a deliberate rotation.\n"
    );
    exit(4);
}

$reviewCode = rtrim(strtr(base64_encode(random_bytes(18)), '+/', '-_'), '=');
$config['reviewAccessCodeHash'] = password_hash($reviewCode, PASSWORD_DEFAULT);
$config['reviewTokenSecret'] = bin2hex(random_bytes(32));

$directory = dirname($configPath);
$temporaryPath = tempnam($directory, '.config-review-');
if ($temporaryPath === false) {
    fwrite(STDERR, "Unable to create a temporary configuration file.\n");
    exit(5);
}

try {
    $content = "<?php\nreturn " . var_export($config, true) . ";\n";
    if (file_put_contents($temporaryPath, $content, LOCK_EX) === false) {
        throw new RuntimeException('Unable to write the temporary configuration.');
    }
    $mode = fileperms($configPath);
    if ($mode !== false) {
        chmod($temporaryPath, $mode & 0777);
    }
    $owner = fileowner($configPath);
    if ($owner !== false) {
        chown($temporaryPath, $owner);
    }
    $group = filegroup($configPath);
    if ($group !== false) {
        chgrp($temporaryPath, $group);
    }
    if (!rename($temporaryPath, $configPath)) {
        throw new RuntimeException('Unable to replace the service configuration.');
    }
} catch (Throwable $error) {
    if (is_file($temporaryPath)) {
        unlink($temporaryPath);
    }
    fwrite(STDERR, $error->getMessage() . "\n");
    exit(6);
}

fwrite(STDOUT, $reviewCode . "\n");
