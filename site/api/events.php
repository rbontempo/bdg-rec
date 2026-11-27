<?php
// api/events.php — Receive analytics events from BDG rec app

require_once __DIR__ . '/config.php';

// Only POST allowed
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

// Validate API key
$apiKey = $_SERVER['HTTP_X_API_KEY'] ?? '';
$expectedKey = getApiKey();
// An empty key on the server must not mean "accept anything".
if ($expectedKey === '' || !is_string($apiKey) || !hash_equals($expectedKey, $apiKey)) {
    jsonResponse(401, ['error' => 'Invalid API key']);
}

// Rate limiting
$db = getDb();
$ip = clientIp();
if (!rateLimitAllows($db, 'e', $ip, 60)) {
    jsonResponse(429, ['error' => 'Rate limit exceeded']);
}

// Parse body
$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

// Handle batch or single event
$events = isset($body['batch']) ? $body['batch'] : [$body];
if (!is_array($events)) {
    jsonResponse(400, ['error' => 'Invalid batch']);
}

// Current apps send at most 100 per request, but older ones send their whole
// queue (up to 500, persisted across runs) and re-send it unchanged on any
// error. So accept everything up to 500: refusing would wedge those clients
// forever, and cutting at 100 while answering "ok" (the old behaviour) threw
// the rest away. Only a batch no real client can produce is refused.
const MAX_BATCH = 500;
if (count($events) > MAX_BATCH) {
    jsonResponse(413, ['error' => 'Batch too large', 'max' => MAX_BATCH]);
}

$validTypes = ['app_open', 'recording_end', 'dsp_applied', 'export_complete', 'error'];
$validEffects = ['normalize', 'denoise', 'compress', 'deesser'];

$insertStmt = $db->prepare(
    'INSERT INTO events (event_type, machine_id, os, app_version, hardware, locale, extra)
     VALUES (?, ?, ?, ?, ?, ?, ?)'
);

$inserted = 0;
$db->beginTransaction();
try {
    // Non-string fields used to reach substr()/strlen() and throw a TypeError
    // (HTTP 500). Anything that is not a plain string becomes empty.
    $str = fn($v) => is_string($v) ? $v : '';

    foreach ($events as $evt) {
        if (!is_array($evt)) continue;

        $type = $str($evt['event'] ?? '');
        $machineId = $str($evt['machine_id'] ?? '');

        if (!in_array($type, $validTypes, true)) continue;
        if (!preg_match('/^[A-Za-z0-9_-]{1,60}$/', $machineId)) continue;

        $os = substr($str($evt['os'] ?? ''), 0, 40);
        $version = substr($str($evt['app_version'] ?? ''), 0, 15);
        // Never store the raw device name — see normalizeHardware().
        $hardware = normalizeHardware($str($evt['hardware'] ?? ''));
        $locale = substr($str($evt['locale'] ?? ''), 0, 10);

        $extra = $evt['extra'] ?? null;
        if ($extra !== null && !is_array($extra)) {
            $extra = null;
        }
        if ($extra !== null && strlen(json_encode($extra)) > 2048) {
            $extra = null;
        }
        if ($extra !== null) {
            if (isset($extra['duration_seconds']) && ($extra['duration_seconds'] < 0 || $extra['duration_seconds'] > 86400)) {
                $extra['duration_seconds'] = min(max((int)$extra['duration_seconds'], 0), 86400);
            }
            if (isset($extra['file_size_mb']) && ($extra['file_size_mb'] < 0 || $extra['file_size_mb'] > 10000)) {
                $extra['file_size_mb'] = min(max((float)$extra['file_size_mb'], 0), 10000);
            }
            // Free text from the app can carry file paths (and with them the
            // user's account name). Keep it short and strip anything path-like.
            if (isset($extra['message'])) {
                $msg = $str($extra['message']);
                // Paths can contain spaces ("Podcast do Fulano"), so cut
                // everything from the first path-looking character on.
                $msg = preg_replace('#(?:[A-Za-z]:)?[\\\\/].*$#us', '<path>', $msg);
                $extra['message'] = mb_substr($msg, 0, 120);
            }
            if (isset($extra['effects']) && is_array($extra['effects'])) {
                $extra['effects'] = array_values(array_filter($extra['effects'], fn($e) => in_array($e, $validEffects, true)));
            }
        }

        $insertStmt->execute([
            $type,
            $machineId,
            $os ?: null,
            $version ?: null,
            $hardware,
            $locale ?: null,
            $extra !== null ? json_encode($extra) : null,
        ]);
        $inserted++;
    }
    $db->commit();
} catch (\Throwable $e) {
    $db->rollBack();
    jsonResponse(500, ['error' => 'Insert failed']);
}

if ($inserted === 0) {
    jsonResponse(400, ['error' => 'No valid events']);
}

jsonResponse(201, ['ok' => true, 'inserted' => $inserted]);
