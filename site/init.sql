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
