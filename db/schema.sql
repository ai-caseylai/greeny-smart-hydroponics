-- Greeny Hydroponic Management System Database Schema (D1/SQLite)

-- Users
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'staff' CHECK (role IN ('admin', 'manager', 'staff')),
    display_name TEXT NOT NULL DEFAULT '',
    active INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username ON users(username);

-- Devices (hydroponic sensors/controllers)
CREATE TABLE IF NOT EXISTS devices (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    floor INTEGER NOT NULL DEFAULT 1,
    location TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'offline' CHECK (status IN ('online', 'offline', 'warning', 'alarm', 'maintenance')),
    last_seen INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_devices_status ON devices(status);

-- Telemetry readings (ESP32 HTTP POST)
CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL REFERENCES devices(id),
    ph REAL NOT NULL DEFAULT 0.0,
    ec REAL NOT NULL DEFAULT 0.0,
    water_temp REAL NOT NULL DEFAULT 0.0,
    do_value REAL NOT NULL DEFAULT 0.0,
    water_level REAL NOT NULL DEFAULT 0.0,
    ts_ms INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_telemetry_device ON telemetry(device_id);
CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(ts_ms);
CREATE INDEX IF NOT EXISTS idx_telemetry_device_ts ON telemetry(device_id, ts_ms);

-- Alerts
CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL REFERENCES devices(id),
    type TEXT NOT NULL DEFAULT 'ph_abnormal' CHECK (type IN ('ph_abnormal', 'ec_high', 'temp_abnormal', 'do_low', 'offline', 'water_low', 'leak')),
    message TEXT NOT NULL DEFAULT '',
    severity TEXT NOT NULL DEFAULT 'warning' CHECK (severity IN ('info', 'warning', 'critical')),
    acknowledged INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_alerts_device ON alerts(device_id);
CREATE INDEX IF NOT EXISTS idx_alerts_created ON alerts(created_at);

-- Settings (key-value store for system configuration)
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL DEFAULT '',
    updated_at INTEGER NOT NULL DEFAULT (unixepoch())
);
