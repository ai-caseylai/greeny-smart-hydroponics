-- Greeny Rack Management Migration

-- Offices (租戶)
CREATE TABLE IF NOT EXISTS offices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    contact_person TEXT NOT NULL DEFAULT '',
    contact_phone TEXT NOT NULL DEFAULT '',
    whatsapp_number TEXT NOT NULL DEFAULT '',
    notes TEXT NOT NULL DEFAULT '',
    active INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_offices_active ON offices(active);

-- Racks (水耕架)
CREATE TABLE IF NOT EXISTS racks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    office_id INTEGER NOT NULL REFERENCES offices(id),
    device_id TEXT REFERENCES devices(id),
    location TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active','inactive','maintenance')),
    layer_count INTEGER NOT NULL DEFAULT 3,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_racks_office ON racks(office_id);
CREATE INDEX IF NOT EXISTS idx_racks_device ON racks(device_id);

-- Rack Vegetables (蔬菜點算)
CREATE TABLE IF NOT EXISTS rack_vegetables (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rack_id INTEGER NOT NULL REFERENCES racks(id),
    layer_number INTEGER NOT NULL,
    variety TEXT NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 0,
    planted_at INTEGER NOT NULL DEFAULT (unixepoch()),
    notes TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_rack_veg_rack ON rack_vegetables(rack_id);
CREATE INDEX IF NOT EXISTS idx_rack_veg_layer ON rack_vegetables(rack_id, layer_number);

-- Rack Environment (環境記錄)
CREATE TABLE IF NOT EXISTS rack_environment (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rack_id INTEGER NOT NULL REFERENCES racks(id),
    temperature REAL,
    humidity REAL,
    light_level REAL,
    ph REAL,
    ec REAL,
    source TEXT NOT NULL DEFAULT 'manual' CHECK (source IN ('manual','telemetry')),
    recorded_at INTEGER NOT NULL DEFAULT (unixepoch()),
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_rack_env_rack ON rack_environment(rack_id);

-- Automations (WorkBuddy 自動化任務)
CREATE TABLE IF NOT EXISTS automations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK (type IN ('daily_report','env_check','nutrient_reminder','harvest_reminder','custom')),
    cron_expr TEXT NOT NULL DEFAULT '',
    config TEXT NOT NULL DEFAULT '{}',
    office_id INTEGER REFERENCES offices(id),
    enabled INTEGER NOT NULL DEFAULT 1,
    last_run_at INTEGER,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

-- Seed data
INSERT INTO offices (name, contact_person, contact_phone, whatsapp_number, notes) VALUES
    ('TechForLiving', 'Perry', '91234567', '85291234567', '主要合作夥伴'),
    ('GreenOffice Co.', 'Amy Wong', '98765432', '85298765432', '共享辦公室'),
    ('FarmLab Studio', 'David Lee', '92345678', '85292345678', '實驗性水耕');

INSERT INTO racks (name, office_id, device_id, location, status, layer_count) VALUES
    ('TFL-A1', 1, 'WSD-001', '會議室A', 'active', 4),
    ('TFL-A2', 1, 'WSD-002', '茶水間', 'active', 3),
    ('GO-B1', 2, 'WSD-003', '3F開放區', 'active', 4),
    ('GO-B2', 2, NULL, '5F休息室', 'active', 3),
    ('FL-C1', 3, 'WSD-005', '實驗室左側', 'active', 5),
    ('FL-C2', 3, NULL, '實驗室右側', 'maintenance', 4);

INSERT INTO rack_vegetables (rack_id, layer_number, variety, quantity, planted_at) VALUES
    (1, 1, '羅勒', 12, unixepoch()-86400*14),
    (1, 2, '生菜', 8, unixepoch()-86400*21),
    (1, 3, '薄荷', 10, unixepoch()-86400*7),
    (1, 4, '香菜', 6, unixepoch()-86400*10),
    (2, 1, '小白菜', 15, unixepoch()-86400*18),
    (2, 2, '青蔥', 20, unixepoch()-86400*30),
    (2, 3, '菠菜', 10, unixepoch()-86400*12),
    (3, 1, '蘿蔓生菜', 8, unixepoch()-86400*25),
    (3, 2, '芝麻菜', 12, unixepoch()-86400*15),
    (3, 3, '羽衣甘藍', 6, unixepoch()-86400*20),
    (3, 4, '芥菜', 10, unixepoch()-86400*8),
    (4, 1, '九層塔', 14, unixepoch()-86400*22),
    (4, 2, '芹菜', 8, unixepoch()-86400*16),
    (4, 3, '韭菜', 18, unixepoch()-86400*28),
    (5, 1, '番茄苗', 4, unixepoch()-86400*35),
    (5, 2, '草莓', 6, unixepoch()-86400*40),
    (5, 3, '辣椒', 8, unixepoch()-86400*20),
    (5, 4, '茄子', 4, unixepoch()-86400*45),
    (5, 5, '小黃瓜', 6, unixepoch()-86400*30);

INSERT INTO rack_environment (rack_id, temperature, humidity, light_level, ph, ec, source, recorded_at) VALUES
    (1, 24.5, 65, 850, 6.2, 1200, 'telemetry', unixepoch()-60),
    (1, 24.3, 64, 820, 6.1, 1180, 'telemetry', unixepoch()-3600),
    (2, 23.8, 68, 780, 6.4, 1350, 'telemetry', unixepoch()-30),
    (3, 25.1, 62, 900, 5.9, 1150, 'telemetry', unixepoch()-45),
    (5, 24.0, 70, 950, 6.1, 1250, 'telemetry', unixepoch()-20),
    (4, 22.5, 72, 600, 6.3, 1100, 'manual', unixepoch()-7200),
    (6, 23.0, 75, 500, 6.0, 1050, 'manual', unixepoch()-86400);

INSERT INTO automations (name, type, cron_expr, config, office_id, enabled) VALUES
    ('每日環境報告', 'daily_report', '0 9 * * *', '{"notify": "whatsapp"}', NULL, 1),
    ('環境異常檢查', 'env_check', '*/30 * * * *', '{"check_ph": true, "check_ec": true, "check_temp": true}', NULL, 1),
    ('營養補充提醒', 'nutrient_reminder', '0 8 * * 1', '{"message": "請檢查並補充營養液"}', 1, 1),
    ('採收提醒', 'harvest_reminder', '0 10 * * *', '{"min_days": 25}', NULL, 0);
