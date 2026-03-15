<?php
// api/auth.php — Admin login

require_once __DIR__ . '/config.php';

session_start([
    'cookie_httponly' => true,
    'cookie_secure'   => true,
    'cookie_samesite' => 'Strict',
]);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

$email = $body['email'] ?? '';
$password = $body['password'] ?? '';

if ($email !== getAdminEmail()) {
    jsonResponse(401, ['error' => 'Invalid credentials']);
}

if (!password_verify($password, getAdminPasswordHash())) {
    jsonResponse(401, ['error' => 'Invalid credentials']);
}

$_SESSION['admin'] = true;
$_SESSION['admin_email'] = $email;

jsonResponse(200, ['ok' => true]);
