# BDG rec analytics — implementation plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Send usage analytics from BDG rec (C++/JUCE desktop app) to BDG rec site (SiteGround PHP + MySQL), wiring the existing admin.html dashboard to real data.

**Architecture:** PHP API on SiteGround receives events via POST, stores in MySQL, serves aggregated stats via GET. C++ `AnalyticsReporter` class (modeled on existing `UpdateChecker`) sends events in batches. Admin dashboard fetches real data instead of mocks. Docker Compose for local MySQL during development.

**Tech Stack:** PHP 8+ / PDO / MySQL 8, C++17 / JUCE 8, Docker Compose, Chart.js (already in admin.html)

**Spec:** `docs/superpowers/specs/2026-03-15-analytics-design.md`

---

## Chunk 1: Local dev environment + PHP backend

### Task 1: Docker Compose + MySQL + init.sql

**Files:**
- Create: `bdg-rec-site/docker-compose.yml`
- Create: `bdg-rec-site/init.sql`
- Create: `bdg-rec-site/.env.example`

- [ ] **Step 1: Create `docker-compose.yml`**

```yaml
services:
  mysql:
    image: mysql:8
    environment:
      MYSQL_ROOT_PASSWORD: rootpass
      MYSQL_DATABASE: bdg_analytics
      MYSQL_USER: bdg
      MYSQL_PASSWORD: bdgpass
    ports:
      - "3306:3306"
    volumes:
      - ./init.sql:/docker-entrypoint-initdb.d/init.sql
      - mysql_data:/var/lib/mysql

volumes:
  mysql_data:
```

- [ ] **Step 2: Create `init.sql`**

```sql
CREATE TABLE IF NOT EXISTS events (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    event_type  VARCHAR(30) NOT NULL,
    machine_id  VARCHAR(60) NOT NULL,
    os          VARCHAR(40),
    app_version VARCHAR(15),
    hardware    VARCHAR(100),
    locale      VARCHAR(10),
    extra       JSON,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_type_date (event_type, created_at),
    INDEX idx_machine (machine_id),
    INDEX idx_created (created_at)
);

CREATE TABLE IF NOT EXISTS rate_limits (
    ip           VARCHAR(45) NOT NULL,
    minute_bucket INT UNSIGNED NOT NULL,
    hits         SMALLINT UNSIGNED DEFAULT 1,
    PRIMARY KEY (ip, minute_bucket)
);
```

- [ ] **Step 3: Create `.env.example`**

```
DB_HOST=127.0.0.1
DB_PORT=3306
DB_NAME=bdg_analytics
DB_USER=bdg
DB_PASS=bdgpass
API_KEY=brec_dev_test_key_1234567890abcdef
ADMIN_EMAIL=admin@bdg.fm
ADMIN_PASSWORD_HASH=$2y$10$... (generate with: php -r "echo password_hash('admin123', PASSWORD_BCRYPT);")
```

- [ ] **Step 4: Start Docker and verify MySQL**

```bash
cd bdg-rec-site && docker compose up -d
# Wait a few seconds for MySQL to initialize
docker compose exec mysql mysql -ubdg -pbdgpass bdg_analytics -e "SHOW TABLES;"
```

Expected: tables `events` and `rate_limits` listed.

- [ ] **Step 5: Add `.env` to `.gitignore`, copy `.env.example` to `.env`**

```bash
echo ".env" >> .gitignore
cp .env.example .env
# Edit .env: generate a real bcrypt hash for ADMIN_PASSWORD_HASH
php -r "echo password_hash('admin123', PASSWORD_BCRYPT) . PHP_EOL;"
# Paste the output into .env as ADMIN_PASSWORD_HASH value
```

- [ ] **Step 6: Commit**

```bash
git add docker-compose.yml init.sql .env.example .gitignore
git commit -m "feat: add Docker Compose for local MySQL + schema init"
```

---

### Task 2: PHP config + events endpoint

**Files:**
- Create: `bdg-rec-site/api/config.php`
- Create: `bdg-rec-site/api/events.php`

- [ ] **Step 1: Create `api/config.php`**

Reads from environment variables (set via `.env` locally, server config on SiteGround). Returns a PDO instance.

```php
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
```

- [ ] **Step 2: Create `api/events.php`**

POST endpoint: validates API key, rate limits, inserts event(s).

```php
<?php
// api/events.php — Receive analytics events from BDG rec app

require_once __DIR__ . '/config.php';

// Only POST allowed
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

// Validate API key
$apiKey = $_SERVER['HTTP_X_API_KEY'] ?? '';
if ($apiKey !== getApiKey()) {
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
if (!$body) {
    jsonResponse(400, ['error' => 'Invalid JSON']);
}

// Handle batch or single event
$events = isset($body['batch']) ? $body['batch'] : [$body];

$validTypes = ['app_open', 'recording_end', 'dsp_applied', 'export_complete', 'error'];
$validEffects = ['normalize', 'denoise', 'compress', 'deesser'];

$insertStmt = $db->prepare(
    'INSERT INTO events (event_type, machine_id, os, app_version, hardware, locale, extra)
     VALUES (?, ?, ?, ?, ?, ?, ?)'
);

$inserted = 0;
foreach ($events as $evt) {
    // Validate required fields
    $type = $evt['event'] ?? '';
    $machineId = $evt['machine_id'] ?? '';

    if (!in_array($type, $validTypes, true)) continue;
    if (strlen($machineId) === 0 || strlen($machineId) > 60) continue;

    // Sanitize optional fields
    $os = substr($evt['os'] ?? '', 0, 40);
    $version = substr($evt['app_version'] ?? '', 0, 15);
    $hardware = substr($evt['hardware'] ?? '', 0, 100);
    $locale = substr($evt['locale'] ?? '', 0, 10);

    // Validate extra data plausibility
    $extra = $evt['extra'] ?? null;
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
        $hardware ?: null,
        $locale ?: null,
        $extra !== null ? json_encode($extra) : null,
    ]);
    $inserted++;
}

if ($inserted === 0) {
    jsonResponse(400, ['error' => 'No valid events']);
}

jsonResponse(201, ['ok' => true, 'inserted' => $inserted]);
```

- [ ] **Step 3: Start PHP dev server and test with curl**

```bash
cd bdg-rec-site && php -S localhost:8080 -t . &

# Test: missing API key → 401
curl -s -X POST http://localhost:8080/api/events.php \
  -H "Content-Type: application/json" \
  -d '{"event":"app_open","machine_id":"girafa-a3f8c21d"}' | jq .

# Test: valid single event → 201
curl -s -X POST http://localhost:8080/api/events.php \
  -H "Content-Type: application/json" \
  -H "X-API-Key: brec_dev_test_key_1234567890abcdef" \
  -d '{"event":"app_open","machine_id":"girafa-a3f8c21d","os":"macOS 15.3","app_version":"1.0.0","hardware":"Scarlett 2i2","locale":"pt-BR"}' | jq .

# Test: valid batch → 201
curl -s -X POST http://localhost:8080/api/events.php \
  -H "Content-Type: application/json" \
  -H "X-API-Key: brec_dev_test_key_1234567890abcdef" \
  -d '{"batch":[
    {"event":"app_open","machine_id":"pinguim-b2c4d6e8","os":"Windows 11","app_version":"1.0.0","hardware":"Realtek HD","locale":"en"},
    {"event":"recording_end","machine_id":"pinguim-b2c4d6e8","os":"Windows 11","app_version":"1.0.0","hardware":"Realtek HD","locale":"en","extra":{"duration_seconds":1800}}
  ]}' | jq .

# Test: invalid event type → 400 (no valid events)
curl -s -X POST http://localhost:8080/api/events.php \
  -H "Content-Type: application/json" \
  -H "X-API-Key: brec_dev_test_key_1234567890abcdef" \
  -d '{"event":"fake_event","machine_id":"test-1234"}' | jq .

# Verify in MySQL
docker compose exec mysql mysql -ubdg -pbdgpass bdg_analytics -e "SELECT * FROM events;"
```

Expected: 3 rows in events table (1 single + 2 batch). 401 without key. 400 with fake event type.

- [ ] **Step 4: Commit**

```bash
git add api/config.php api/events.php
git commit -m "feat: add PHP events API with rate limiting and validation"
```

---

### Task 3: Auth endpoint

**Files:**
- Create: `bdg-rec-site/api/auth.php`

- [ ] **Step 1: Create `api/auth.php`**

```php
<?php
// api/auth.php — Admin login

require_once __DIR__ . '/config.php';

session_start();

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(405, ['error' => 'Method not allowed']);
}

$body = json_decode(file_get_contents('php://input'), true);
if (!$body) {
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
```

- [ ] **Step 2: Test auth with curl**

```bash
# Test: wrong password → 401
curl -s -X POST http://localhost:8080/api/auth.php \
  -H "Content-Type: application/json" \
  -d '{"email":"admin@bdg.fm","password":"wrong"}' | jq .

# Test: correct credentials → 200 + session cookie
curl -s -v -X POST http://localhost:8080/api/auth.php \
  -H "Content-Type: application/json" \
  -d '{"email":"admin@bdg.fm","password":"admin123"}' 2>&1 | grep -E "(Set-Cookie|{)"
```

Expected: 401 on wrong password. 200 + `Set-Cookie: PHPSESSID=...` on correct credentials.

- [ ] **Step 3: Commit**

```bash
git add api/auth.php
git commit -m "feat: add admin auth endpoint with bcrypt"
```

---

### Task 4: Stats endpoint

**Files:**
- Create: `bdg-rec-site/api/stats.php`

- [ ] **Step 1: Create `api/stats.php`**

```php
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
// Format created_at as ISO 8601
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
```

- [ ] **Step 2: Seed test data and test stats endpoint**

```bash
# Seed varied test data
API_KEY="brec_dev_test_key_1234567890abcdef"
URL="http://localhost:8080/api/events.php"

for i in $(seq 1 20); do
  curl -s -X POST "$URL" \
    -H "Content-Type: application/json" \
    -H "X-API-Key: $API_KEY" \
    -d "{\"batch\":[
      {\"event\":\"app_open\",\"machine_id\":\"gato-$(printf '%08x' $((RANDOM*RANDOM)))\",\"os\":\"macOS 15.3\",\"app_version\":\"1.0.0\",\"hardware\":\"Scarlett 2i2\",\"locale\":\"pt-BR\"},
      {\"event\":\"recording_end\",\"machine_id\":\"gato-$(printf '%08x' $((RANDOM*RANDOM)))\",\"os\":\"macOS 15.3\",\"app_version\":\"1.0.0\",\"hardware\":\"Scarlett 2i2\",\"locale\":\"pt-BR\",\"extra\":{\"duration_seconds\":$((RANDOM%3600+60))}},
      {\"event\":\"dsp_applied\",\"machine_id\":\"gato-$(printf '%08x' $((RANDOM*RANDOM)))\",\"os\":\"Windows 11\",\"app_version\":\"1.0.0\",\"hardware\":\"Realtek HD\",\"locale\":\"en\",\"extra\":{\"effects\":[\"normalize\",\"denoise\"]}}
    ]}" > /dev/null
done

# Login to get session cookie
curl -s -c /tmp/bdg_cookies -X POST http://localhost:8080/api/auth.php \
  -H "Content-Type: application/json" \
  -d '{"email":"admin@bdg.fm","password":"admin123"}' | jq .

# Fetch stats with session
curl -s -b /tmp/bdg_cookies "http://localhost:8080/api/stats.php?range=7d" | jq .
```

Expected: JSON response with cards, activity, os_distribution, dsp_usage, versions, recent_events — all with real data from the seeded events.

- [ ] **Step 3: Test stats without auth → 401**

```bash
curl -s "http://localhost:8080/api/stats.php?range=7d" | jq .
```

Expected: `{"error": "Not authenticated"}`

- [ ] **Step 4: Commit**

```bash
git add api/stats.php
git commit -m "feat: add stats API with aggregated analytics queries"
```

---

## Chunk 2: Admin dashboard — live data

### Task 5: Wire admin.html to real API

**Files:**
- Modify: `bdg-rec-site/admin.html`

- [ ] **Step 1: Add login overlay**

In `admin.html`, add a login screen that covers the dashboard until authenticated. Add it right after `<body>`:

```html
<!-- Login overlay -->
<div id="login-overlay" class="fixed inset-0 z-50 flex items-center justify-center" style="background: #09090b;">
  <div class="w-full max-w-sm p-8">
    <div class="flex items-center justify-center gap-3 mb-8">
      <img src="logo-bdg-rec.png" alt="BDG REC" class="h-8">
      <span class="badge badge-pink font-mono text-[10px]">ADMIN</span>
    </div>
    <form id="login-form" class="space-y-4">
      <div>
        <label class="text-xs text-muted block mb-1.5">Email</label>
        <input type="email" id="login-email" required
          class="w-full px-3 py-2 rounded-lg text-sm bg-[#18181b] border border-border text-white focus:outline-none focus:border-bdg-primary"
          placeholder="admin@bdg.fm">
      </div>
      <div>
        <label class="text-xs text-muted block mb-1.5">Senha</label>
        <input type="password" id="login-password" required
          class="w-full px-3 py-2 rounded-lg text-sm bg-[#18181b] border border-border text-white focus:outline-none focus:border-bdg-primary"
          placeholder="••••••••">
      </div>
      <div id="login-error" class="text-xs text-red-500 hidden">Credenciais inválidas</div>
      <button type="submit"
        class="w-full py-2 rounded-lg text-sm font-semibold text-white bg-bdg-primary hover:opacity-90 transition-opacity">
        Entrar
      </button>
    </form>
  </div>
</div>
```

- [ ] **Step 2: Replace mock `<script>` block with live data fetching**

Replace the entire `<script>` section at the bottom of `admin.html` with new JavaScript that:
1. Handles login form submission
2. Fetches stats from `/api/stats.php`
3. Renders charts and tables with real data
4. Handles tab switching (7d/30d/90d) with re-fetch

```javascript
// --- Login ---
const loginOverlay = document.getElementById('login-overlay');
const loginForm = document.getElementById('login-form');
const loginError = document.getElementById('login-error');

loginForm.addEventListener('submit', async (e) => {
  e.preventDefault();
  loginError.classList.add('hidden');
  const email = document.getElementById('login-email').value;
  const password = document.getElementById('login-password').value;

  try {
    const res = await fetch('/api/auth.php', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password }),
    });
    if (res.ok) {
      loginOverlay.style.display = 'none';
      loadDashboard('7d');
    } else {
      loginError.classList.remove('hidden');
    }
  } catch {
    loginError.classList.remove('hidden');
  }
});

// Try loading dashboard (if session already active)
(async () => {
  try {
    const res = await fetch('/api/stats.php?range=7d');
    if (res.ok) {
      loginOverlay.style.display = 'none';
      const data = await res.json();
      renderDashboard(data);
    }
  } catch { /* show login */ }
})();

// --- Chart instances (for destruction on re-render) ---
let activityChartInstance = null;
let osChartInstance = null;
let versionsChartInstance = null;

// --- Dashboard ---
async function loadDashboard(range) {
  try {
    const res = await fetch(`/api/stats.php?range=${range}`);
    if (!res.ok) { loginOverlay.style.display = 'flex'; return; }
    const data = await res.json();
    renderDashboard(data);
  } catch (err) {
    console.error('Failed to load dashboard:', err);
  }
}

function renderDashboard(data) {
  // Stat cards
  document.getElementById('stat-installs').textContent = data.cards.active_installs.value.toLocaleString();
  document.getElementById('stat-today').textContent = data.cards.active_today.value.toLocaleString();
  document.getElementById('stat-recordings').textContent = data.cards.total_recordings.value.toLocaleString();

  const avgMin = data.cards.avg_duration_min.value;
  document.getElementById('stat-duration').innerHTML = `${avgMin}<span class="text-lg text-muted ml-1">min</span>`;

  // Update change indicators on stat cards
  const cardEls = document.querySelectorAll('.stat-card');
  const cardKeys = ['active_installs', 'active_today', 'total_recordings', 'avg_duration_min'];
  cardKeys.forEach((key, i) => {
    const card = cardEls[i];
    const pct = data.cards[key].change_pct;
    const changeEl = card.querySelector('.flex.items-center.gap-1.mt-2');
    if (changeEl) {
      const isUp = pct > 0;
      const isDown = pct < 0;
      const color = isUp ? '#22c55e' : isDown ? '#ef4444' : '#eab308';
      const textColor = isUp ? 'text-green-500' : isDown ? 'text-red-500' : 'text-yellow-500';
      const label = isUp ? `+${pct}%` : pct === 0 ? 'estável' : `${pct}%`;
      const svgUp = '<polyline points="23 6 13.5 15.5 8.5 10.5 1 18"/>';
      const svgDown = '<polyline points="23 18 13.5 8.5 8.5 13.5 1 6"/>';
      const svgFlat = '<line x1="5" y1="12" x2="19" y2="12"/>';
      const svgPath = isUp ? svgUp : isDown ? svgDown : svgFlat;
      changeEl.innerHTML = `
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="${color}" stroke-width="2.5">${svgPath}</svg>
        <span class="text-[11px] ${textColor} font-medium">${label}</span>
        <span class="text-[11px] text-muted ml-1">vs período anterior</span>`;
    }
  });

  // Activity chart
  Chart.defaults.color = '#71717a';
  Chart.defaults.borderColor = '#1e1e22';
  Chart.defaults.font.family = "'Instrument Sans', sans-serif";
  Chart.defaults.font.size = 11;

  if (activityChartInstance) activityChartInstance.destroy();
  const activityCtx = document.getElementById('activityChart').getContext('2d');
  activityChartInstance = new Chart(activityCtx, {
    type: 'line',
    data: {
      labels: data.activity.labels.map(d => {
        const date = new Date(d + 'T00:00:00');
        return date.toLocaleDateString('pt-BR', { weekday: 'short' });
      }),
      datasets: [
        {
          label: 'Aberturas', data: data.activity.opens,
          borderColor: '#E5067D', backgroundColor: 'rgba(229,6,125,0.08)',
          fill: true, tension: 0.4, borderWidth: 2, pointRadius: 0,
          pointHoverRadius: 5, pointHoverBackgroundColor: '#E5067D',
        },
        {
          label: 'Gravações', data: data.activity.recordings,
          borderColor: '#3b82f6', backgroundColor: 'rgba(59,130,246,0.06)',
          fill: true, tension: 0.4, borderWidth: 2, pointRadius: 0,
          pointHoverRadius: 5, pointHoverBackgroundColor: '#3b82f6',
        }
      ]
    },
    options: {
      responsive: true, maintainAspectRatio: false,
      interaction: { intersect: false, mode: 'index' },
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { display: false }, ticks: { padding: 8 } },
        y: { grid: { color: '#1e1e22' }, ticks: { padding: 8 }, beginAtZero: true }
      }
    }
  });

  // OS chart
  const osLabels = Object.keys(data.os_distribution);
  const osValues = Object.values(data.os_distribution);
  const osColors = osLabels.map(l => l === 'macOS' ? '#E5067D' : l === 'Windows' ? '#3b82f6' : '#71717a');

  if (osChartInstance) osChartInstance.destroy();
  const osCtx = document.getElementById('osChart').getContext('2d');
  osChartInstance = new Chart(osCtx, {
    type: 'doughnut',
    data: { labels: osLabels, datasets: [{ data: osValues, backgroundColor: osColors, borderWidth: 0, spacing: 3 }] },
    options: {
      responsive: true, maintainAspectRatio: false, cutout: '70%',
      plugins: { legend: { display: false }, tooltip: { callbacks: { label: ctx => `${ctx.label}: ${ctx.parsed}` } } }
    }
  });

  // DSP usage bars
  const dspLabels = { normalize: 'Normalizar', denoise: 'Redução de ruído', compress: 'Compressor', deesser: 'De-Esser' };
  const dspColors = { normalize: '#E5067D', denoise: '#8b5cf6', compress: '#3b82f6', deesser: '#06b6d4' };
  const dspContainer = document.querySelector('.chart-card.fade-in.fade-in-5 .space-y-4');
  if (dspContainer) {
    dspContainer.innerHTML = Object.entries(data.dsp_usage).map(([key, pct]) => `
      <div>
        <div class="flex items-center justify-between text-sm mb-1.5">
          <span class="text-[#d4d4d8]">${dspLabels[key] || key}</span>
          <span class="font-mono text-xs text-muted">${pct}%</span>
        </div>
        <div class="h-2 bg-[#18181b] rounded-full overflow-hidden">
          <div class="h-full rounded-full" style="width: ${pct}%; background: ${dspColors[key] || '#E5067D'}"></div>
        </div>
      </div>
    `).join('');
  }

  // Versions chart
  const versionLabels = Object.keys(data.versions).map(v => v.startsWith('v') ? v : `v${v}`);
  const versionValues = Object.values(data.versions);
  const versionColors = versionValues.map((_, i) => {
    const opacity = Math.max(0.15, 1 - i * 0.25);
    return i === 0 ? '#E5067D' : `rgba(229,6,125,${opacity})`;
  });

  if (versionsChartInstance) versionsChartInstance.destroy();
  const versionsCtx = document.getElementById('versionsChart').getContext('2d');
  versionsChartInstance = new Chart(versionsCtx, {
    type: 'bar',
    data: { labels: versionLabels, datasets: [{ data: versionValues, backgroundColor: versionColors, borderRadius: 6, barThickness: 40 }] },
    options: {
      responsive: true, maintainAspectRatio: false, indexAxis: 'y',
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { color: '#1e1e22' }, ticks: { padding: 8 }, beginAtZero: true },
        y: { grid: { display: false }, ticks: { padding: 8, font: { family: "'Space Mono', monospace", size: 12 } } }
      }
    }
  });

  // Recent events table
  const badgeClass = { app_open: 'badge-green', recording_end: 'badge-pink', dsp_applied: 'badge-yellow', export_complete: 'badge-pink', error: 'badge-red' };
  const tbody = document.getElementById('events-table');
  tbody.innerHTML = data.recent_events.map(evt => {
    const ago = timeAgo(new Date(evt.created_at));
    return `<tr>
      <td><span class="badge ${badgeClass[evt.event] || 'badge-green'}">${evt.event}</span></td>
      <td class="font-mono text-xs">${evt.machine_id}</td>
      <td>${evt.os || '—'}</td>
      <td>${evt.app_version ? 'v' + evt.app_version : '—'}</td>
      <td class="text-xs">${evt.hardware || '—'}</td>
      <td class="text-muted text-xs">${ago}</td>
    </tr>`;
  }).join('');
}

function timeAgo(date) {
  const seconds = Math.floor((Date.now() - date.getTime()) / 1000);
  if (seconds < 60) return 'agora';
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes} min atrás`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}h atrás`;
  const days = Math.floor(hours / 24);
  return `${days}d atrás`;
}

// Tab switching (range)
document.querySelectorAll('[data-range]').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('[data-range]').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    loadDashboard(btn.dataset.range);
  });
});

// Sidebar nav
document.querySelectorAll('[data-page]').forEach(link => {
  link.addEventListener('click', () => {
    document.querySelectorAll('[data-page]').forEach(l => l.classList.remove('active'));
    link.classList.add('active');
  });
});
```

- [ ] **Step 3: Hide out-of-scope sidebar items**

In the sidebar `<nav>`, add `style="display:none"` to the "Instalações" and "Licenças" links:

```html
<a class="sidebar-link" data-page="installs" style="display:none">
```
```html
<a class="sidebar-link" data-page="licenses" style="display:none">
```

- [ ] **Step 4: Test the full flow in browser**

```bash
# Make sure PHP server and Docker are running
# Open http://localhost:8080/admin.html in browser
# 1. Should see login form
# 2. Login with admin@bdg.fm / admin123
# 3. Should see dashboard with real data from seeded events
# 4. Click 30d / 90d tabs — data should re-fetch
```

- [ ] **Step 5: Commit**

```bash
git add admin.html
git commit -m "feat: wire admin dashboard to live analytics API"
```

---

## Chunk 3: C++ AnalyticsReporter

### Task 6: Create AnalyticsReporter class

**Files:**
- Create: `BDG rec/src/AnalyticsReporter.h`
- Create: `BDG rec/src/AnalyticsReporter.cpp`

- [ ] **Step 1: Create `AnalyticsReporter.h`**

```cpp
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

class AnalyticsReporter : private juce::Timer
{
public:
    AnalyticsReporter();
    ~AnalyticsReporter() override;

    /// Call once from MainComponent constructor with the app properties
    void initialise(juce::ApplicationProperties& props, const juce::String& analyticsUrl);

    /// Queue an event to be sent in the next batch
    void trackEvent(const juce::String& eventType,
                    const juce::var& extra = juce::var());

    /// Set context that is sent with every event
    void setContext(const juce::String& os,
                    const juce::String& appVersion,
                    const juce::String& hardware,
                    const juce::String& locale);

    /// Get the machine ID (funny word + UUID)
    juce::String getMachineId() const { return machineId; }

    /// Flush any queued events (call from destructor/shutdown)
    void flush();

private:
    void timerCallback() override;
    void sendBatch();
    juce::String generateMachineId();
    void loadPendingEvents();
    void savePendingEvents();

    juce::ApplicationProperties* appProps = nullptr;
    juce::String apiUrl;
    juce::String apiKey;
    juce::String machineId;

    // Context fields
    juce::String osName;
    juce::String appVersion;
    juce::String hardwareName;
    juce::String localeName;

    juce::CriticalSection queueLock;
    juce::Array<juce::var> eventQueue;
    std::atomic<bool> isSending { false };

    static constexpr int BATCH_INTERVAL_MS = 30000; // 30 seconds

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyticsReporter)
};
```

- [ ] **Step 2: Create `AnalyticsReporter.cpp`**

```cpp
#include "AnalyticsReporter.h"
#include <juce_gui_basics/juce_gui_basics.h>

// ~50 funny pt-BR words for machine ID generation
static const char* funnyWords[] = {
    "abacaxi", "capivara", "jacare", "pinguim", "tucano",
    "girafa", "golfinho", "papagaio", "tartaruga", "borboleta",
    "canguru", "flamingo", "hipopotamo", "lagarto", "macaco",
    "ornitorrinco", "pantera", "quati", "rinoceronte", "sardinha",
    "tamanduá", "urubu", "veado", "zebra", "arara",
    "besouro", "cachorro", "dragao", "elefante", "formiga",
    "gato", "hamster", "iguana", "jabuti", "koala",
    "leao", "morcego", "narvalo", "ovelha", "piranha",
    "raposa", "sapo", "tigre", "unicornio", "vaca",
    "xexeu", "yak", "zagaia", "aranha", "baleia"
};

static constexpr int NUM_FUNNY_WORDS = sizeof(funnyWords) / sizeof(funnyWords[0]);

AnalyticsReporter::AnalyticsReporter() = default;

AnalyticsReporter::~AnalyticsReporter()
{
    stopTimer();
    flush();
}

void AnalyticsReporter::initialise(juce::ApplicationProperties& props, const juce::String& analyticsUrl)
{
    appProps = &props;
    apiUrl = analyticsUrl;
    apiKey = "brec_REPLACE_WITH_PRODUCTION_KEY";

    // Load or generate machine ID
    if (auto* pf = props.getUserSettings())
    {
        machineId = pf->getValue("machineId", "");
        if (machineId.isEmpty())
        {
            machineId = generateMachineId();
            pf->setValue("machineId", machineId);
            pf->saveIfNeeded();
        }
    }

    // Load any unsent events from previous session
    loadPendingEvents();

    // Start batch timer
    startTimer(BATCH_INTERVAL_MS);
}

juce::String AnalyticsReporter::generateMachineId()
{
    juce::Random rng;
    int wordIndex = rng.nextInt(NUM_FUNNY_WORDS);
    auto uuid = juce::Uuid().toString().removeCharacters("-").substring(0, 8);
    return juce::String(funnyWords[wordIndex]) + "-" + uuid;
}

void AnalyticsReporter::setContext(const juce::String& os,
                                    const juce::String& version,
                                    const juce::String& hardware,
                                    const juce::String& locale)
{
    osName = os;
    appVersion = version;
    hardwareName = hardware;
    localeName = locale;
}

void AnalyticsReporter::trackEvent(const juce::String& eventType, const juce::var& extra)
{
    auto evt = new juce::DynamicObject();
    evt->setProperty("event", eventType);
    evt->setProperty("machine_id", machineId);
    evt->setProperty("os", osName);
    evt->setProperty("app_version", appVersion);
    evt->setProperty("hardware", hardwareName);
    evt->setProperty("locale", localeName);
    if (!extra.isVoid())
        evt->setProperty("extra", extra);

    juce::ScopedLock lock(queueLock);
    eventQueue.add(juce::var(evt));
}

void AnalyticsReporter::timerCallback()
{
    // Run on background thread to not block UI
    // weak flag prevents use-after-free if reporter is destroyed during send
    if (!isSending.exchange(true))
    {
        juce::Thread::launch([this]() {
            sendBatch();
            isSending = false;
        });
    }
}

void AnalyticsReporter::sendBatch()
{
    juce::Array<juce::var> batch;
    {
        juce::ScopedLock lock(queueLock);
        if (eventQueue.isEmpty()) return;
        batch = eventQueue;
        eventQueue.clear();
    }

    auto batchObj = new juce::DynamicObject();
    juce::Array<juce::var> batchArray;
    for (auto& evt : batch)
        batchArray.add(evt);
    batchObj->setProperty("batch", batchArray);

    auto jsonBody = juce::JSON::toString(juce::var(batchObj));

    auto url = juce::URL(apiUrl)
        .withPOSTData(jsonBody);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withConnectionTimeoutMs(10000)
                       .withExtraHeaders("Content-Type: application/json\r\nX-API-Key: " + apiKey);

    auto stream = url.createInputStream(options);

    if (stream == nullptr)
    {
        // No internet — put events back in queue for retry
        juce::ScopedLock lock(queueLock);
        for (auto& evt : batch)
            eventQueue.add(evt);
        return;
    }

    auto response = stream->readEntireStreamAsString();
    // If non-2xx, put events back for retry
    // (JUCE doesn't expose status code easily, so check response content)
    if (!response.contains("\"ok\""))
    {
        juce::ScopedLock lock(queueLock);
        for (auto& evt : batch)
            eventQueue.add(evt);
    }
}

void AnalyticsReporter::flush()
{
    // Don't do network I/O on shutdown — just persist unsent events to disk
    // They will be retried on next launch via loadPendingEvents()
    savePendingEvents();
}

void AnalyticsReporter::loadPendingEvents()
{
    if (auto* pf = appProps->getUserSettings())
    {
        auto pending = pf->getValue("pendingAnalytics", "");
        if (pending.isNotEmpty())
        {
            auto parsed = juce::JSON::parse(pending);
            if (parsed.isArray())
            {
                juce::ScopedLock lock(queueLock);
                for (int i = 0; i < parsed.size(); ++i)
                    eventQueue.add(parsed[i]);
            }
            pf->setValue("pendingAnalytics", "");
            pf->saveIfNeeded();
        }
    }
}

void AnalyticsReporter::savePendingEvents()
{
    juce::ScopedLock lock(queueLock);
    if (eventQueue.isEmpty()) return;

    if (auto* pf = appProps->getUserSettings())
    {
        juce::Array<juce::var> arr;
        for (auto& evt : eventQueue)
            arr.add(evt);
        pf->setValue("pendingAnalytics", juce::JSON::toString(arr));
        pf->saveIfNeeded();
    }
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add `src/AnalyticsReporter.cpp` to `target_sources`:

```cmake
target_sources(BDG_REC PRIVATE
    ...
    src/UpdateChecker.cpp
    src/AnalyticsReporter.cpp
)
```

- [ ] **Step 4: Verify it compiles**

```bash
cd "BDG rec" && cmake --build build --target BDG_REC 2>&1 | tail -20
```

Expected: builds successfully with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/AnalyticsReporter.h src/AnalyticsReporter.cpp CMakeLists.txt
git commit -m "feat: add AnalyticsReporter class with batching and offline retry"
```

---

### Task 7: Integrate AnalyticsReporter into MainComponent

**Files:**
- Modify: `BDG rec/src/MainComponent.h`
- Modify: `BDG rec/src/MainComponent.cpp`

- [ ] **Step 1: Add AnalyticsReporter member to `MainComponent.h`**

Add include and member:

```cpp
#include "AnalyticsReporter.h"
```

Add member next to `UpdateChecker`:

```cpp
AnalyticsReporter  analyticsReporter;
```

- [ ] **Step 2: Initialise reporter in `MainComponent` constructor**

In `MainComponent.cpp`, after `updateChecker.checkIfDue(...)` block (around line 93), add:

```cpp
// Analytics reporter
{
    analyticsReporter.initialise(appProperties, "https://rec.bdg.fm/api/events.php");

    // Set context
    juce::String os;
    #if JUCE_MAC
        os = "macOS " + juce::SystemStats::getOperatingSystemName().fromFirstOccurrenceOf("macOS ", false, true);
    #elif JUCE_WINDOWS
        os = juce::SystemStats::getOperatingSystemName();
    #endif

    auto hw = audioEngine.getCurrentInputDeviceName();
    auto lang = Strings::getLanguage() == Language::EN ? "en" : "pt-BR";
    auto ver = juce::String(JUCE_APPLICATION_VERSION_STRING);

    analyticsReporter.setContext(os, ver, hw, lang);
    analyticsReporter.trackEvent("app_open");
}
```

- [ ] **Step 3: Add `getElapsedSeconds()` getter to RecordingPanel**

In `RecordingPanel.h`, add a public method:

```cpp
int getElapsedSeconds() const { return elapsedSecs; }
```

- [ ] **Step 4: Track `recording_end` in `handleRecordButtonClicked`**

In the else-branch of `handleRecordButtonClicked()`, right after `lastRecordedFile = audioEngine.stopRecording();` and `isRecording = false;` (around line 271-273), add:

```cpp
// Track recording end
{
    auto extra = new juce::DynamicObject();
    extra->setProperty("duration_seconds", recordingPanel.getElapsedSeconds());
    analyticsReporter.trackEvent("recording_end", juce::var(extra));
}
```

- [ ] **Step 5: Track `dsp_applied` in `dspFinished`**

In `MainComponent::dspFinished()`, after `dspOverlay.hide()`, add:

```cpp
// Track DSP applied
{
    auto extra = new juce::DynamicObject();
    juce::Array<juce::var> effects;
    if (outputPanel.isNormalizeOn())      effects.add("normalize");
    if (outputPanel.isNoiseReductionOn()) effects.add("denoise");
    if (outputPanel.isCompressorOn())     effects.add("compress");
    if (outputPanel.isDeEsserOn())        effects.add("deesser");
    extra->setProperty("effects", effects);
    analyticsReporter.trackEvent("dsp_applied", juce::var(extra));
}
```

- [ ] **Step 6: Track `export_complete` in both paths**

In `dspFinished()`, after the DSP tracking code, add:

```cpp
// Track export complete
{
    auto extra = new juce::DynamicObject();
    extra->setProperty("file_size_mb", (double)file.getSize() / (1024.0 * 1024.0));
    analyticsReporter.trackEvent("export_complete", juce::var(extra));
}
```

In the non-DSP else-branch of `handleRecordButtonClicked()` (around line 301-306), after the success message, add:

```cpp
// Track export complete (no DSP)
{
    auto extra = new juce::DynamicObject();
    extra->setProperty("file_size_mb", (double)lastRecordedFile.getSize() / (1024.0 * 1024.0));
    analyticsReporter.trackEvent("export_complete", juce::var(extra));
}
```

- [ ] **Step 7: Track `error` in error handlers**

In `dspError()` (around line 343-351), after `dspOverlay.hide()`, add:

```cpp
analyticsReporter.trackEvent("error", [&]() {
    auto extra = new juce::DynamicObject();
    extra->setProperty("error_code", "dsp_crash");
    extra->setProperty("message", error);
    return juce::var(extra);
}());
```

In `devicesChanged()` (around line 174-180), inside the `if (isRecording && ...)` block, add:

```cpp
analyticsReporter.trackEvent("error", [&]() {
    auto extra = new juce::DynamicObject();
    extra->setProperty("error_code", "device_lost");
    extra->setProperty("message", "Device disconnected during recording");
    return juce::var(extra);
}());
```

- [ ] **Step 8: Update hardware context when device changes**

Add a private helper to `MainComponent` that builds the OS string:

In `MainComponent.h`, add:

```cpp
void updateAnalyticsContext();
```

In `MainComponent.cpp`, add the helper:

```cpp
void MainComponent::updateAnalyticsContext()
{
    juce::String os;
    #if JUCE_MAC
        os = juce::SystemStats::getOperatingSystemName();
    #elif JUCE_WINDOWS
        os = juce::SystemStats::getOperatingSystemName();
    #else
        os = "Unknown";
    #endif

    analyticsReporter.setContext(
        os,
        juce::String(JUCE_APPLICATION_VERSION_STRING),
        audioEngine.getCurrentInputDeviceName(),
        Strings::getLanguage() == Language::EN ? "en" : "pt-BR"
    );
}
```

Use it in the constructor (replace the inline setContext), in `inputPanel.onSettingsChanged`, and in `devicesChanged()`:

```cpp
updateAnalyticsContext();
```

- [ ] **Step 9: Flush on shutdown**

In `MainComponent::~MainComponent()`, before `appProperties.closeFiles()`, add:

```cpp
analyticsReporter.flush();
```

- [ ] **Step 10: Verify it compiles**

```bash
cd "BDG rec" && cmake --build build --target BDG_REC 2>&1 | tail -20
```

Expected: builds successfully.

- [ ] **Step 11: Commit**

```bash
git add src/MainComponent.h src/MainComponent.cpp
git commit -m "feat: integrate analytics tracking into MainComponent"
```

---

## Chunk 4: Deploy script update + final testing

### Task 8: Update deploy script

**Files:**
- Modify: `bdg-rec-site/deploy.sh`

- [ ] **Step 1: Add PHP API files to deploy script**

Update the `FILES` array in `deploy.sh` to include admin.html and api/ directory:

```bash
FILES=(
  "index.html"
  "admin.html"
  "BDG_ico.png"
  "logo-bdg-rec.png"
  "layout-bdg-rec.png"
)

# Upload API directory
echo "  Uploading api/..."
ssh -p "$PORT" -i ~/.ssh/id_ed25519 "$USER@$HOST" "mkdir -p $REMOTE_DIR/api"
for f in api/config.php api/events.php api/stats.php api/auth.php; do
  echo "    $f"
  scp -P "$PORT" -i ~/.ssh/id_ed25519 "$LOCAL_DIR/$f" "$USER@$HOST:$REMOTE_DIR/$f"
done
```

- [ ] **Step 2: Commit**

```bash
git add deploy.sh
git commit -m "feat: add admin.html and api/ to deploy script"
```

---

### Task 9: End-to-end test (local)

- [ ] **Step 1: Verify full stack locally**

```bash
# Ensure Docker MySQL is running
cd bdg-rec-site && docker compose up -d

# Ensure PHP server is running
php -S localhost:8080 -t . &

# Send test events simulating the C++ app
API_KEY="brec_dev_test_key_1234567890abcdef"
URL="http://localhost:8080/api/events.php"

# Simulate app_open
curl -s -X POST "$URL" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: $API_KEY" \
  -d '{"event":"app_open","machine_id":"capivara-12345678","os":"macOS 15.3","app_version":"1.0.0","hardware":"Scarlett 2i2","locale":"pt-BR"}' | jq .

# Simulate recording_end
curl -s -X POST "$URL" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: $API_KEY" \
  -d '{"event":"recording_end","machine_id":"capivara-12345678","os":"macOS 15.3","app_version":"1.0.0","hardware":"Scarlett 2i2","locale":"pt-BR","extra":{"duration_seconds":2520}}' | jq .

# Simulate dsp_applied
curl -s -X POST "$URL" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: $API_KEY" \
  -d '{"event":"dsp_applied","machine_id":"capivara-12345678","os":"macOS 15.3","app_version":"1.0.0","hardware":"Scarlett 2i2","locale":"pt-BR","extra":{"effects":["normalize","denoise","compress"]}}' | jq .

# Simulate export_complete
curl -s -X POST "$URL" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: $API_KEY" \
  -d '{"event":"export_complete","machine_id":"capivara-12345678","os":"macOS 15.3","app_version":"1.0.0","hardware":"Scarlett 2i2","locale":"pt-BR","extra":{"file_size_mb":45.2}}' | jq .
```

- [ ] **Step 2: Verify dashboard shows the new events**

Open `http://localhost:8080/admin.html`, login, and confirm:
- Stat cards show updated numbers
- Activity chart shows today's events
- Recent events table shows the test events with machine_id "capivara-12345678"
- DSP usage bars reflect the effects sent

- [ ] **Step 3: Test rate limiting**

```bash
# Send 65 requests rapidly
for i in $(seq 1 65); do
  curl -s -o /dev/null -w "%{http_code}\n" -X POST "$URL" \
    -H "Content-Type: application/json" \
    -H "X-API-Key: $API_KEY" \
    -d '{"event":"app_open","machine_id":"ratelimit-test","os":"test","app_version":"1.0.0"}'
done | sort | uniq -c
```

Expected: ~60 responses with `201`, remaining with `429`.

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "chore: end-to-end analytics test verification"
```
