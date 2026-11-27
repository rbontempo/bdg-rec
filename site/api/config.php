<?php
// api/config.php — DB connection + app config

/**
 * Where the credentials file lives.
 *
 * It belongs one level ABOVE the webroot (www/rec.bdg.fm/.env), where no URL
 * can reach it. The old location, public_html/.env, is only read as a
 * fallback so the site keeps working until the file is moved on the server;
 * the .htaccess at the root denies dotfiles in the meantime.
 */
function envFilePath(): string {
    $outside = dirname(__DIR__, 2) . '/.env';
    if (file_exists($outside)) return $outside;
    return dirname(__DIR__) . '/.env';
}

function loadEnv(): void {
    static $loaded = false;
    if ($loaded) return;
    $loaded = true;
    $envFile = envFilePath();
    if (file_exists($envFile)) {
        foreach (file($envFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
            if (str_starts_with(trim($line), '#')) continue;
            if (!str_contains($line, '=')) continue;
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

/**
 * The visitor's IP. rec.bdg.fm sits behind Cloudflare, so REMOTE_ADDR is a
 * Cloudflare edge — every visitor would share one rate-limit bucket, and five
 * wrong passwords from anyone would lock the admin out. The real address is
 * in CF-Connecting-IP, which is trusted ONLY when the request really comes
 * from a Cloudflare range (otherwise anyone could forge the header).
 * Ranges: https://www.cloudflare.com/ips/
 */
function clientIp(): string {
    $remote = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    $forwarded = $_SERVER['HTTP_CF_CONNECTING_IP'] ?? '';
    if ($forwarded === '' || filter_var($forwarded, FILTER_VALIDATE_IP) === false) {
        return $remote;
    }

    $cloudflare = [
        '173.245.48.0/20', '103.21.244.0/22', '103.22.200.0/22', '103.31.4.0/22',
        '141.101.64.0/18', '108.162.192.0/18', '190.93.240.0/20', '188.114.96.0/20',
        '197.234.240.0/22', '198.41.128.0/17', '162.158.0.0/15', '104.16.0.0/13',
        '104.24.0.0/14', '172.64.0.0/13', '131.0.72.0/22',
        '2400:cb00::/32', '2606:4700::/32', '2803:f800::/32', '2405:b500::/32',
        '2405:8100::/32', '2a06:98c0::/29', '2c0f:f248::/32',
    ];
    foreach ($cloudflare as $cidr) {
        if (ipInCidr($remote, $cidr)) return $forwarded;
    }
    return $remote;
}

function ipInCidr(string $ip, string $cidr): bool {
    [$net, $bits] = explode('/', $cidr);
    $ipBin = @inet_pton($ip);
    $netBin = @inet_pton($net);
    if ($ipBin === false || $netBin === false || strlen($ipBin) !== strlen($netBin)) {
        return false;
    }
    $bits = (int) $bits;
    $bytes = intdiv($bits, 8);
    if (substr($ipBin, 0, $bytes) !== substr($netBin, 0, $bytes)) return false;
    $rest = $bits % 8;
    if ($rest === 0) return true;
    $mask = (0xff << (8 - $rest)) & 0xff;
    return (ord($ipBin[$bytes]) & $mask) === (ord($netBin[$bytes]) & $mask);
}

/**
 * Counts one hit for $ip in the current minute under $scope and reports
 * whether it is still within $limit. Shared by the telemetry endpoint and the
 * admin login, so both use the same table and the same cleanup.
 */
function rateLimitAllows(PDO $db, string $scope, string $ip, int $limit): bool {
    $minuteBucket = intdiv(time(), 60);
    $key = substr($scope . ':' . $ip, 0, 45);

    // Cleanup on ~5% of requests: at 1% a quiet site kept IPs for days.
    if (random_int(1, 20) === 1) {
        $db->prepare('DELETE FROM rate_limits WHERE minute_bucket < ?')
           ->execute([$minuteBucket - 60]);
    }

    $db->prepare(
        'INSERT INTO rate_limits (ip, minute_bucket, hits) VALUES (?, ?, 1)
         ON DUPLICATE KEY UPDATE hits = hits + 1'
    )->execute([$key, $minuteBucket]);

    $stmt = $db->prepare('SELECT hits FROM rate_limits WHERE ip = ? AND minute_bucket = ?');
    $stmt->execute([$key, $minuteBucket]);
    return (int) $stmt->fetchColumn() <= $limit;
}

function jsonResponse(int $code, array $data): never {
    http_response_code($code);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit;
}
