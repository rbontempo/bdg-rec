# BDG rec analytics — design spec

## Overview

Send usage analytics from the BDG rec desktop app (C++/JUCE) to the BDG rec site (SiteGround, PHP + MySQL). The admin dashboard (`admin.html`) already has the UI mockup — this spec covers the backend API, database, C++ reporter, and wiring the dashboard to real data.

## Architecture

```
┌─────────────┐    POST /api/events.php   ┌──────────────────────┐
│  BDG rec    │ ────────────────────────→  │  SiteGround (PHP)    │
│  C++ app    │    X-API-Key header        │                      │
└─────────────┘                            │  /api/events.php     │
                                           │  /api/stats.php      │
┌─────────────┐    GET /api/stats.php      │  /api/auth.php       │
│ admin.html  │ ←────────────────────────  │  /api/config.php     │
│ (browser)   │    (session cookie)        │                      │
└─────────────┘                            │  MySQL               │
                                           └──────────────────────┘
```

## Events tracked

| Event | Trigger | Extra data |
|-------|---------|------------|
| `app_open` | App launches | — |
| `recording_end` | Recording stops | `duration_seconds` |
| `dsp_applied` | DSP processing completes | `effects[]` (normalize, denoise, compress, deesser) |
| `export_complete` | Final WAV saved after DSP or raw | `file_size_mb` |
| `error` | Unhandled error caught | `error_code` (string: `dsp_crash`, `file_write_fail`, `device_lost`), `message` |

### Canonical effect names (C++ → PHP → dashboard)

| C++ settings key | Analytics value | Dashboard label |
|-----------------|----------------|-----------------|
| `normalize` | `normalize` | Normalizar |
| `noiseReduction` | `denoise` | Redução de ruído |
| `compressor` | `compress` | Compressor |
| `deEsser` | `deesser` | De-Esser |

### `export_complete` trigger points

- When DSP is used: fires in `dspFinished()` callback after processed WAV is written
- When no DSP: fires after raw WAV is finalized in `handleRecordButtonClicked()` else-branch
- Reports final file size in both cases

### Context sent with every event

- `machine_id` — funny pt-BR word + short UUID (e.g. `girafa-a3f8c21d`), generated once, saved in settings
- `os` — e.g. "macOS 15.3", "Windows 11"
- `app_version` — e.g. "1.0.0"
- `hardware` — active microphone name
- `locale` — "pt-BR" or "en"

## C++ changes (BDG rec)

### New class: `AnalyticsReporter`

Similar pattern to existing `UpdateChecker`:
- Background thread, fire-and-forget
- Uses `juce::URL` for HTTP POST
- Sends JSON payload with `X-API-Key` header
- Silent failure — analytics never blocks the app
- Queues events internally and sends in batch every 30 seconds (reduces HTTP overhead when multiple events fire in quick succession)
- Persists unsent events to `PropertiesFile` on app close; retries on next launch

### Machine ID generation

On first launch (no `machineId` in settings):
1. Pick random word from hardcoded list of ~50 funny pt-BR words
2. Generate UUID, take first 8 hex chars
3. Combine: `"girafa-a3f8c21d"`
4. Save to `PropertiesFile`

Dashboard displays the full funny-word format (not truncated).

### Integration points

- `MainComponent` constructor → `app_open`
- `MainComponent::stopRecording()` → `recording_end` with duration
- `MainComponent` DSP completion callback (`dspFinished`) → `dsp_applied` with effects list
- `MainComponent` after final WAV write (both DSP and non-DSP paths) → `export_complete` with file size
- Error handler → `error` with code and message

## PHP backend (SiteGround)

### Files

| File | Purpose |
|------|---------|
| `api/config.php` | DB credentials, API key, admin password hash (bcrypt) |
| `api/events.php` | POST: validate API key, insert event(s) into MySQL |
| `api/stats.php` | GET: aggregate queries for dashboard (requires admin session) |
| `api/auth.php` | POST: admin login → PHP session |

### `POST /api/events.php`

Accepts single event or batch array:

Single event:
```json
{
    "event": "recording_end",
    "machine_id": "girafa-a3f8c21d",
    "os": "macOS 15.3",
    "app_version": "1.0.0",
    "hardware": "Scarlett 2i2",
    "locale": "pt-BR",
    "extra": { "duration_seconds": 2520 }
}
```

Batch:
```json
{
    "batch": [
        { "event": "app_open", "machine_id": "girafa-a3f8c21d", ... },
        { "event": "recording_end", "machine_id": "girafa-a3f8c21d", ... }
    ]
}
```

Validation:
- Header `X-API-Key` must match configured key
- `event` must be one of the 5 known types
- `machine_id` required, max 60 chars
- All other fields optional, sanitized
- Plausibility checks on extra data: `duration_seconds` <= 86400, `file_size_mb` <= 10000, `effects[]` values must be in canonical list
- Rate limit: max 60 requests/min per IP (MySQL-based, see below)
- No `Access-Control-Allow-Origin` header — rejects browser-origin requests

Response: `201 {"ok": true}` or `4xx {"error": "message"}`

### Rate limiting (MySQL-based)

Shared hosting has no persistent memory. Rate limiting uses a MySQL table:

```sql
CREATE TABLE rate_limits (
    ip           VARCHAR(45) NOT NULL,
    minute_bucket INT UNSIGNED NOT NULL,
    hits         SMALLINT UNSIGNED DEFAULT 1,
    PRIMARY KEY (ip, minute_bucket)
);
```

On each request: `INSERT INTO rate_limits (ip, minute_bucket, hits) VALUES (?, ?, 1) ON DUPLICATE KEY UPDATE hits = hits + 1`. Reject if `hits >= 60`. Cleanup: delete rows older than 1 hour on every 100th request.

### `POST /api/auth.php`

Request: `{"email": "admin@bdg.fm", "password": "..."}`
Response: `200 {"ok": true}` + session cookie, or `401`

### `GET /api/stats.php?range=7d`

Requires valid admin session. Returns all data the dashboard needs:

```json
{
    "cards": {
        "active_installs": { "value": 247, "change_pct": 12 },
        "active_today": { "value": 38, "change_pct": 5 },
        "total_recordings": { "value": 1842, "change_pct": 23 },
        "avg_duration_min": { "value": 42, "change_pct": 0 }
    },
    "activity": {
        "labels": ["2026-03-09", "2026-03-10", "..."],
        "opens": [32, 28, "..."],
        "recordings": [18, 15, "..."]
    },
    "os_distribution": { "macOS": 68, "Windows": 32 },
    "dsp_usage": {
        "normalize": 78, "denoise": 64,
        "compress": 45, "deesser": 31
    },
    "versions": { "1.0.0": 189, "0.9.0": 42 },
    "recent_events": [
        {
            "event": "app_open",
            "machine_id": "girafa-a3f8c21d",
            "os": "macOS 15.3",
            "app_version": "1.0.0",
            "hardware": "Scarlett 2i2",
            "created_at": "2026-03-15T14:30:00Z"
        }
    ]
}
```

### Definitions

- **`active_installs`**: distinct `machine_id` values with at least one event in the selected range
- **`active_today`**: distinct `machine_id` values with at least one event today
- **`total_recordings`**: count of `recording_end` events in the selected range
- **`avg_duration_min`**: average `extra->duration_seconds` / 60 from `recording_end` events in range
- **`change_pct`**: percentage change vs the previous period of equal length (e.g. 7d vs prior 7d)
- **`dsp_usage`**: percentage of `dsp_applied` events containing each effect, within range

## MySQL schema

```sql
CREATE TABLE events (
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

CREATE TABLE rate_limits (
    ip           VARCHAR(45) NOT NULL,
    minute_bucket INT UNSIGNED NOT NULL,
    hits         SMALLINT UNSIGNED DEFAULT 1,
    PRIMARY KEY (ip, minute_bucket)
);
```

Single table for events. No users table — admin auth uses hardcoded bcrypt hash in config.

### Data retention

Events older than 180 days are deleted automatically. Cleanup runs on every stats.php request (at most once per day, tracked via a simple config flag). Expected growth: ~5 events/user/session × 50 active users = ~250 rows/day ≈ 45k rows in 180 days — trivial for MySQL.

## Security

- **API key**: `X-API-Key` header, validated server-side. Key format: `brec_` + 32 random chars. Note: key is embedded in the desktop binary and is therefore extractable — this is accepted as a trade-off for a free desktop app. Plausibility checks on data mitigate abuse.
- **Admin auth**: bcrypt-hashed password in `config.php`, PHP sessions
- **SQL injection**: PDO prepared statements everywhere
- **Rate limiting**: 60 req/min per IP via MySQL table
- **Input validation**: whitelist event types, max lengths on all string fields, plausibility checks on numeric values
- **CORS**: no `Access-Control-Allow-Origin` header on events.php — prevents browser-origin abuse. Same domain for admin.html.
- **HTTPS**: SiteGround provides SSL

## admin.html changes

- Add login screen (simple form, calls `/api/auth.php`)
- Replace all mock data with `fetch('/api/stats.php?range=7d')`
- Tab buttons (7d/30d/90d) trigger re-fetch with new range
- Recent events table populated from API response, showing full machine_id (funny-word format)
- Loading states while fetching
- Sidebar pages "Instalações" and "Licenças" are out of scope — hidden or stubbed for now

## Local development

- Docker Compose with MySQL 8 container for local testing
- PHP built-in server (`php -S localhost:8080 -t .`) for API development
- `api/config.php` reads from environment variables, with `.env` defaults for local dev (not committed)
- `init.sql` script to create tables automatically

## What is NOT in scope

- Real-time/WebSocket updates (polling on tab switch is sufficient)
- User accounts or multi-tenant (single admin)
- Data export/CSV
- Email alerts
- Sidebar pages: Instalações, Licenças (stubbed)
