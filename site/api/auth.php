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

// Brute-force guard: a handful of attempts per IP per minute is plenty for a
// human typing a password and useless for a script.
$ip = clientIp();
try {
    $allowed = rateLimitAllows(getDb(), 'l', $ip, 5);
} catch (\Throwable $e) {
    // Without the database there is no rate limit, so no login either.
    jsonResponse(503, ['error' => 'Service unavailable']);
}
if (!$allowed) {
    jsonResponse(429, ['error' => 'Too many attempts, try again in a minute']);
}

$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

$email = $body['email'] ?? '';
$password = $body['password'] ?? '';
if (!is_string($email) || !is_string($password)) {
    jsonResponse(400, ['error' => 'Invalid credentials']);
}

// Always run password_verify, even when the e-mail is wrong: returning early
// made a wrong e-mail answer measurably faster than a wrong password.
$hash = getAdminPasswordHash();
$passwordOk = $hash !== '' && password_verify($password, $hash);
$emailOk = getAdminEmail() !== '' && hash_equals(getAdminEmail(), $email);

if (!$emailOk || !$passwordOk) {
    jsonResponse(401, ['error' => 'Invalid credentials']);
}

// New session id on privilege change, so a pre-login id cannot be reused.
session_regenerate_id(true);
$_SESSION['admin'] = true;
$_SESSION['admin_email'] = $email;

jsonResponse(200, ['ok' => true]);
