<?php
// api/events.php — Receive analytics events from BDG rec app

require_once __DIR__ . '/config.php';

// Only POST allowed
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

// Validate API key
$apiKey = $_SERVER['HTTP_X_API_KEY'] ?? '';
if (!hash_equals(getApiKey(), $apiKey)) {
    jsonResponse(401, ['error' => 'Invalid API key']);
}

// Rate limiting
$db = getDb();
$ip = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
$minuteBucket = intdiv(time(), 60);

// Cleanup old rate limit entries (~1% of requests)
if (random_int(1, 100) === 1) {
    $cutoff = $minuteBucket - 60; // 1 hour ago
    $db->prepare('DELETE FROM rate_limits WHERE minute_bucket < ?')->execute([$cutoff]);
}

$stmt = $db->prepare(
    'INSERT INTO rate_limits (ip, minute_bucket, hits) VALUES (?, ?, 1)
     ON DUPLICATE KEY UPDATE hits = hits + 1'
);
$stmt->execute([$ip, $minuteBucket]);

$hitsStmt = $db->prepare(
    'SELECT hits FROM rate_limits WHERE ip = ? AND minute_bucket = ?'
);
$hitsStmt->execute([$ip, $minuteBucket]);
$hits = (int) $hitsStmt->fetchColumn();

if ($hits > 60) {
    jsonResponse(429, ['error' => 'Rate limit exceeded']);
}

// Parse body
$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

// Handle batch or single event
$events = isset($body['batch']) ? $body['batch'] : [$body];
$events = array_slice($events, 0, 100); // cap batch size

$validTypes = ['app_open', 'recording_end', 'dsp_applied', 'export_complete', 'error'];
$validEffects = ['normalize', 'denoise', 'compress', 'deesser'];

$insertStmt = $db->prepare(
    'INSERT INTO events (event_type, machine_id, os, app_version, hardware, locale, extra)
     VALUES (?, ?, ?, ?, ?, ?, ?)'
);

$inserted = 0;
$db->beginTransaction();
try {
    foreach ($events as $evt) {
        $type = $evt['event'] ?? '';
        $machineId = $evt['machine_id'] ?? '';

        if (!in_array($type, $validTypes, true)) continue;
        if (strlen($machineId) === 0 || strlen($machineId) > 60) continue;

        $os = substr($evt['os'] ?? '', 0, 40);
        $version = substr($evt['app_version'] ?? '', 0, 15);
        // Never store the raw device name — see normalizeHardware().
        $hardware = normalizeHardware($evt['hardware'] ?? null);
        $locale = substr($evt['locale'] ?? '', 0, 10);

        $extra = $evt['extra'] ?? null;
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
