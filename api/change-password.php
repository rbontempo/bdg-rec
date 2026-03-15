<?php
// api/change-password.php — Change admin password

require_once __DIR__ . '/config.php';

session_start([
    'cookie_httponly' => true,
    'cookie_secure'   => true,
    'cookie_samesite' => 'Strict',
]);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

if (empty($_SESSION['admin'])) {
    jsonResponse(401, ['error' => 'Not authenticated']);
}

$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

$currentPassword = $body['current_password'] ?? '';
$newPassword = $body['new_password'] ?? '';

if (strlen($newPassword) < 8) {
    jsonResponse(400, ['error' => 'A nova senha deve ter pelo menos 8 caracteres']);
}

if (!password_verify($currentPassword, getAdminPasswordHash())) {
    jsonResponse(401, ['error' => 'Senha atual incorreta']);
}

$newHash = password_hash($newPassword, PASSWORD_BCRYPT);

// Update .env file with new hash
$envFile = __DIR__ . '/../.env';
if (!file_exists($envFile)) {
    jsonResponse(500, ['error' => 'Config file not found']);
}

$envContent = file_get_contents($envFile);
$envContent = preg_replace_callback(
    '/^ADMIN_PASSWORD_HASH=.*$/m',
    fn() => 'ADMIN_PASSWORD_HASH=' . $newHash,
    $envContent
);
file_put_contents($envFile, $envContent);

jsonResponse(200, ['ok' => true]);
