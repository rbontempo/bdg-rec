<?php
// api/stats.php — Aggregated analytics for admin dashboard

require_once __DIR__ . '/config.php';

session_start();

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

// Require admin session
if (empty($_SESSION['admin'])) {
    jsonResponse(401, ['error' => 'Not authenticated']);
}

$db = getDb();

// Parse range
$range = $_GET['range'] ?? '7d';
$days = match($range) {
    '30d' => 30,
    '90d' => 90,
    default => 7,
};

$now = new DateTimeImmutable('now', new DateTimeZone('UTC'));
$rangeStart = $now->modify("-{$days} days")->format('Y-m-d H:i:s');
$prevRangeStart = $now->modify("-" . ($days * 2) . " days")->format('Y-m-d H:i:s');
$todayStart = $now->format('Y-m-d') . ' 00:00:00';

// --- Stat cards ---

// Active installs (distinct machine_id in range)
$stmt = $db->prepare('SELECT COUNT(DISTINCT machine_id) FROM events WHERE created_at >= ?');
$stmt->execute([$rangeStart]);
$activeInstalls = (int) $stmt->fetchColumn();

$stmt = $db->prepare('SELECT COUNT(DISTINCT machine_id) FROM events WHERE created_at >= ? AND created_at < ?');
$stmt->execute([$prevRangeStart, $rangeStart]);
$prevActiveInstalls = (int) $stmt->fetchColumn();

// Active today
$stmt = $db->prepare('SELECT COUNT(DISTINCT machine_id) FROM events WHERE created_at >= ?');
$stmt->execute([$todayStart]);
$activeToday = (int) $stmt->fetchColumn();

$yesterdayStart = $now->modify('-1 day')->format('Y-m-d') . ' 00:00:00';
$stmt = $db->prepare('SELECT COUNT(DISTINCT machine_id) FROM events WHERE created_at >= ? AND created_at < ?');
$stmt->execute([$yesterdayStart, $todayStart]);
$activeYesterday = (int) $stmt->fetchColumn();

// Total recordings in range
$stmt = $db->prepare("SELECT COUNT(*) FROM events WHERE event_type = 'recording_end' AND created_at >= ?");
$stmt->execute([$rangeStart]);
$totalRecordings = (int) $stmt->fetchColumn();

$stmt = $db->prepare("SELECT COUNT(*) FROM events WHERE event_type = 'recording_end' AND created_at >= ? AND created_at < ?");
$stmt->execute([$prevRangeStart, $rangeStart]);
$prevRecordings = (int) $stmt->fetchColumn();

// Avg duration
$stmt = $db->prepare("SELECT AVG(JSON_EXTRACT(extra, '$.duration_seconds')) FROM events WHERE event_type = 'recording_end' AND created_at >= ? AND extra IS NOT NULL");
$stmt->execute([$rangeStart]);
$avgDurationSec = (float) $stmt->fetchColumn();
$avgDurationMin = round($avgDurationSec / 60);

$stmt = $db->prepare("SELECT AVG(JSON_EXTRACT(extra, '$.duration_seconds')) FROM events WHERE event_type = 'recording_end' AND created_at >= ? AND created_at < ? AND extra IS NOT NULL");
$stmt->execute([$prevRangeStart, $rangeStart]);
$prevAvgSec = (float) $stmt->fetchColumn();
$prevAvgMin = $prevAvgSec > 0 ? round($prevAvgSec / 60) : 0;

// Helper: calculate percentage change
function changePct(int|float $current, int|float $previous): int {
    if ($previous == 0) return $current > 0 ? 100 : 0;
    return (int) round(($current - $previous) / $previous * 100);
}

$cards = [
    'active_installs' => ['value' => $activeInstalls, 'change_pct' => changePct($activeInstalls, $prevActiveInstalls)],
    'active_today' => ['value' => $activeToday, 'change_pct' => changePct($activeToday, $activeYesterday)],
    'total_recordings' => ['value' => $totalRecordings, 'change_pct' => changePct($totalRecordings, $prevRecordings)],
    'avg_duration_min' => ['value' => $avgDurationMin, 'change_pct' => changePct($avgDurationMin, $prevAvgMin)],
];

// --- Activity chart (daily opens + recordings) ---
$activity = ['labels' => [], 'opens' => [], 'recordings' => []];
for ($i = $days - 1; $i >= 0; $i--) {
    $date = $now->modify("-{$i} days")->format('Y-m-d');
    $activity['labels'][] = $date;

    $stmt = $db->prepare("SELECT COUNT(*) FROM events WHERE event_type = 'app_open' AND DATE(created_at) = ?");
    $stmt->execute([$date]);
    $activity['opens'][] = (int) $stmt->fetchColumn();

    $stmt = $db->prepare("SELECT COUNT(*) FROM events WHERE event_type = 'recording_end' AND DATE(created_at) = ?");
    $stmt->execute([$date]);
    $activity['recordings'][] = (int) $stmt->fetchColumn();
}

// --- OS distribution ---
$stmt = $db->prepare(
    "SELECT
        CASE
            WHEN os LIKE 'macOS%' OR os LIKE 'Mac%' THEN 'macOS'
            WHEN os LIKE 'Windows%' OR os LIKE 'Win%' THEN 'Windows'
            ELSE 'Other'
        END as os_group,
        COUNT(DISTINCT machine_id) as cnt
     FROM events WHERE created_at >= ?
     GROUP BY os_group"
);
$stmt->execute([$rangeStart]);
$osRaw = $stmt->fetchAll();
$osDistribution = [];
foreach ($osRaw as $row) {
    $osDistribution[$row['os_group']] = (int) $row['cnt'];
}

// --- DSP usage ---
$stmt = $db->prepare("SELECT COUNT(*) FROM events WHERE event_type = 'dsp_applied' AND created_at >= ?");
$stmt->execute([$rangeStart]);
$totalDsp = (int) $stmt->fetchColumn();

$dspUsage = ['normalize' => 0, 'denoise' => 0, 'compress' => 0, 'deesser' => 0];
if ($totalDsp > 0) {
    foreach (array_keys($dspUsage) as $effect) {
        $stmt = $db->prepare(
            "SELECT COUNT(*) FROM events
             WHERE event_type = 'dsp_applied' AND created_at >= ?
             AND JSON_CONTAINS(JSON_EXTRACT(extra, '$.effects'), ?)"
        );
        $stmt->execute([$rangeStart, json_encode($effect)]);
        $count = (int) $stmt->fetchColumn();
        $dspUsage[$effect] = (int) round($count / $totalDsp * 100);
    }
}

// --- Versions distribution ---
$stmt = $db->prepare(
    "SELECT app_version, COUNT(DISTINCT machine_id) as cnt
     FROM events WHERE created_at >= ? AND app_version IS NOT NULL
     GROUP BY app_version ORDER BY cnt DESC LIMIT 10"
);
$stmt->execute([$rangeStart]);
$versions = [];
foreach ($stmt->fetchAll() as $row) {
    $versions[$row['app_version']] = (int) $row['cnt'];
}

// --- Recent events ---
$stmt = $db->prepare(
    "SELECT event_type as event, machine_id, os, app_version, hardware, created_at
     FROM events ORDER BY created_at DESC LIMIT 50"
);
$stmt->execute();
$recentEvents = $stmt->fetchAll();
foreach ($recentEvents as &$evt) {
    $evt['created_at'] = (new DateTimeImmutable($evt['created_at']))->format('c');
}

// --- Data retention cleanup (once per day) ---
$retentionFile = sys_get_temp_dir() . '/bdg_analytics_retention';
$lastCleanup = file_exists($retentionFile) ? (int) file_get_contents($retentionFile) : 0;
if (time() - $lastCleanup > 86400) {
    $cutoff = $now->modify('-180 days')->format('Y-m-d H:i:s');
    $db->prepare('DELETE FROM events WHERE created_at < ?')->execute([$cutoff]);
    file_put_contents($retentionFile, time());
}

// --- Response ---
jsonResponse(200, [
    'cards' => $cards,
    'activity' => $activity,
    'os_distribution' => $osDistribution,
    'dsp_usage' => $dspUsage,
    'versions' => $versions,
    'recent_events' => $recentEvents,
]);
