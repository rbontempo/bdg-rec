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

function jsonResponse(int $code, array $data): never {
    http_response_code($code);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit;
}
