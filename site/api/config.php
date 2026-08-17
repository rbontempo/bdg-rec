<?php
// api/config.php — DB connection + app config

function loadEnv(): void {
    static $loaded = false;
    if ($loaded) return;
    $loaded = true;
    $envFile = __DIR__ . '/../.env';
    if (file_exists($envFile)) {
        foreach (file($envFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
            if (str_starts_with(trim($line), '#')) continue;
            [$key, $value] = explode('=', $line, 2);
            $_ENV[trim($key)] = trim($value);
        }
    }
}

function getDb(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        loadEnv();
        $dsn = sprintf('mysql:host=%s;port=%s;dbname=%s;charset=utf8mb4',
            $_ENV['DB_HOST'] ?? '127.0.0.1',
            $_ENV['DB_PORT'] ?? '3306',
            $_ENV['DB_NAME'] ?? 'bdg_analytics'
        );
        $pdo = new PDO($dsn, $_ENV['DB_USER'] ?? 'bdg', $_ENV['DB_PASS'] ?? 'bdgpass', [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        ]);
    }
    return $pdo;
}

function getApiKey(): string {
    loadEnv();
    return $_ENV['API_KEY'] ?? '';
}

function getAdminEmail(): string {
    loadEnv();
    return $_ENV['ADMIN_EMAIL'] ?? '';
}

function getAdminPasswordHash(): string {
    loadEnv();
    return $_ENV['ADMIN_PASSWORD_HASH'] ?? '';
}

/**
 * Reduce a microphone description to a fixed category.
 *
 * Device names routinely embed the owner's name ("AirPods do Renato",
 * "iPhone de Fulano — Microfone"), which is personal data we have no business
 * storing. The app stopped sending the raw name in v1.1.12, but every copy
 * still running an older build keeps sending it — so the normalisation has to
 * happen here, on the way in, or the database keeps collecting names for as
 * long as anyone is on an old version.
 *
 * Mirrors AudioEngine::getCurrentInputDeviceCategory() in the app.
 * Only ever returns one of: builtin, usb, bluetooth, other, none.
 */
function normalizeHardware(?string $raw): ?string {
    $raw = trim((string) $raw);
    if ($raw === '') return null;

    // Already a category (app v1.1.12+): pass through untouched.
    if (in_array($raw, ['builtin', 'usb', 'bluetooth', 'other', 'none'], true)) {
        return $raw;
    }

    $name = mb_strtolower($raw, 'UTF-8');

    foreach (['bluetooth', 'airpod', 'wireless'] as $needle) {
        if (str_contains($name, $needle)) return 'bluetooth';
    }
    if (str_contains($name, 'usb')) return 'usb';
    foreach (['built-in', 'builtin', 'internal', 'macbook', 'interno', 'integrado'] as $needle) {
        if (str_contains($name, $needle)) return 'builtin';
    }

    return 'other';
}

function jsonResponse(int $code, array $data): never {
    http_response_code($code);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit;
}
