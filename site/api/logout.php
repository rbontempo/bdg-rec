<?php
// api/logout.php — End the admin session

require_once __DIR__ . '/config.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

session_start([
    'cookie_httponly' => true,
    'cookie_secure'   => true,
    'cookie_samesite' => 'Strict',
]);

$_SESSION = [];
$params = session_get_cookie_params();
setcookie(session_name(), '', [
    'expires'  => time() - 3600,
    'path'     => $params['path'],
    'domain'   => $params['domain'],
    'secure'   => true,
    'httponly' => true,
    'samesite' => 'Strict',
]);
session_destroy();

jsonResponse(200, ['ok' => true]);
