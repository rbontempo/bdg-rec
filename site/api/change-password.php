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
if (!is_string($currentPassword) || !is_string($newPassword)) {
    jsonResponse(400, ['error' => 'Invalid input']);
}

if (strlen($newPassword) < 8) {
    jsonResponse(400, ['error' => 'A nova senha deve ter pelo menos 8 caracteres']);
}

if (!password_verify($currentPassword, getAdminPasswordHash())) {
    jsonResponse(401, ['error' => 'Senha atual incorreta']);
}

$newHash = password_hash($newPassword, PASSWORD_BCRYPT);

// Update the same .env the config was loaded from.
$envFile = envFilePath();
if (!file_exists($envFile)) {
    jsonResponse(500, ['error' => 'Config file not found']);
}

$envContent = file_get_contents($envFile);
$replaced = 0;
$envContent = preg_replace_callback(
    '/^ADMIN_PASSWORD_HASH=.*$/m',
    fn() => 'ADMIN_PASSWORD_HASH=' . $newHash,
    $envContent,
    -1,
    $replaced
);
if ($replaced !== 1) {
    // Saying "ok" here used to leave the old password in force.
    jsonResponse(500, ['error' => 'ADMIN_PASSWORD_HASH not found in config']);
}

// Write a sibling and rename over the original: a half-written .env would
// take the database credentials down with it.
$tmp = $envFile . '.tmp';
if (file_put_contents($tmp, $envContent, LOCK_EX) === false
    || !chmod($tmp, fileperms($envFile) & 0777)   // keep it as private as the original
    || !rename($tmp, $envFile)) {
    @unlink($tmp);
    jsonResponse(500, ['error' => 'Could not save config']);
}

// New id for this session. Other open sessions are NOT invalidated (PHP's
// file sessions cannot be enumerated); they expire on their own.
session_regenerate_id(true);

jsonResponse(200, ['ok' => true]);
