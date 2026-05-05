-- Seed data for Greeny Hydroponic Management System

-- Admin user (password: admin123)
INSERT INTO users (username, password_hash, role, display_name) VALUES
    ('admin', '$2b$10$KPuBiIjIp.ALwltTIh1QN.qGvlZm0jCssjHuHDo7vBeua/JlXzzmy', 'admin', '系統管理員');

-- Devices (hydroponic sensor nodes)
INSERT OR IGNORE INTO devices (id, name, floor, location, status, last_seen) VALUES
    ('WSD-001', 'A區-主水箱', 1, '溫室A區', 'online', unixepoch()-60),
    ('WSD-002', 'A區-營養液槽', 1, '溫室A區', 'online', unixepoch()-30),
    ('WSD-003', 'B區-主水箱', 2, '溫室B區', 'online', unixepoch()-45),
    ('WSD-004', 'B區-營養液槽', 2, '溫室B區', 'alarm', unixepoch()-120),
    ('WSD-005', 'C區-主水箱', 3, '露天C區', 'online', unixepoch()-20),
    ('WSD-006', 'C區-營養液槽', 3, '露天C區', 'online', unixepoch()-50),
    ('WSD-007', 'D區-主水箱', 4, '溫室D區', 'maintenance', unixepoch()-7200),
    ('WSD-008', 'D區-營養液槽', 4, '溫室D區', 'online', unixepoch()-15),
    ('WSD-009', 'E區-育苗區', 5, '育苗室', 'online', unixepoch()-10),
    ('WSD-010', 'E區-營養液槽', 5, '育苗室', 'online', unixepoch()-25),
    ('WSD-011', 'F區-主水箱', 6, '屋頂F區', 'online', unixepoch()-35),
    ('WSD-012', 'F區-營養液槽', 6, '屋頂F區', 'online', unixepoch()-40),
    ('WSD-013', 'G區-實驗區', 7, '實驗室', 'offline', unixepoch()-86400),
    ('WSD-014', 'G區-營養液槽', 7, '實驗室', 'online', unixepoch()-55),
    ('WSD-015', 'H區-展示區', 8, '展示廳', 'online', unixepoch()-5);

-- Telemetry (generate 7 days of data for charts)
-- Recent readings
INSERT INTO telemetry (device_id, ph, ec, water_temp, do_value, water_level, ts_ms, created_at) VALUES
    ('WSD-001', 6.2, 1200, 24.5, 7.8, 85, unixepoch()*1000-60000, unixepoch()-60),
    ('WSD-001', 6.1, 1180, 24.3, 7.6, 84, unixepoch()*1000-3600000, unixepoch()-3600),
    ('WSD-001', 6.3, 1220, 24.8, 7.9, 86, unixepoch()*1000-7200000, unixepoch()-7200),
    ('WSD-001', 6.0, 1190, 24.1, 7.5, 83, unixepoch()*1000-10800000, unixepoch()-10800),
    ('WSD-001', 6.2, 1210, 24.6, 7.7, 85, unixepoch()*1000-14400000, unixepoch()-14400),
    ('WSD-002', 6.4, 1350, 23.8, 8.1, 90, unixepoch()*1000-30000, unixepoch()-30),
    ('WSD-002', 6.3, 1340, 23.6, 8.0, 89, unixepoch()*1000-3500000, unixepoch()-3500),
    ('WSD-003', 5.9, 1150, 25.1, 7.2, 78, unixepoch()*1000-45000, unixepoch()-45),
    ('WSD-003', 6.0, 1160, 25.3, 7.3, 79, unixepoch()*1000-3645000, unixepoch()-3645),
    ('WSD-004', 7.8, 2100, 28.5, 5.1, 45, unixepoch()*1000-120000, unixepoch()-120),
    ('WSD-004', 7.5, 2000, 28.0, 5.3, 48, unixepoch()*1000-3720000, unixepoch()-3720),
    ('WSD-005', 6.1, 1250, 24.0, 7.4, 82, unixepoch()*1000-20000, unixepoch()-20),
    ('WSD-006', 6.5, 1400, 23.5, 8.2, 92, unixepoch()*1000-50000, unixepoch()-50),
    ('WSD-008', 6.2, 1180, 24.2, 7.6, 80, unixepoch()*1000-15000, unixepoch()-15),
    ('WSD-009', 6.0, 1100, 25.0, 7.5, 88, unixepoch()*1000-10000, unixepoch()-10),
    ('WSD-010', 6.3, 1300, 24.8, 7.8, 85, unixepoch()*1000-25000, unixepoch()-25),
    ('WSD-011', 6.1, 1200, 24.5, 7.6, 81, unixepoch()*1000-35000, unixepoch()-35),
    ('WSD-012', 6.4, 1350, 23.9, 8.0, 89, unixepoch()*1000-40000, unixepoch()-40),
    ('WSD-014', 6.2, 1250, 24.3, 7.5, 83, unixepoch()*1000-55000, unixepoch()-55),
    ('WSD-015', 6.3, 1280, 24.7, 7.7, 86, unixepoch()*1000-5000, unixepoch()-5);

-- Historical 7-day trend data for dashboard chart (WSD-001)
-- Day 1-7 aggregated readings
INSERT INTO telemetry (device_id, ph, ec, water_temp, do_value, water_level, ts_ms, created_at) VALUES
    ('WSD-001', 6.1, 1180, 23.5, 7.5, 90, (unixepoch()-86400*6)*1000, unixepoch()-86400*6),
    ('WSD-001', 6.0, 1170, 23.8, 7.4, 88, (unixepoch()-86400*5)*1000, unixepoch()-86400*5),
    ('WSD-001', 6.2, 1200, 24.0, 7.6, 87, (unixepoch()-86400*4)*1000, unixepoch()-86400*4),
    ('WSD-001', 6.1, 1195, 24.2, 7.5, 86, (unixepoch()-86400*3)*1000, unixepoch()-86400*3),
    ('WSD-001', 6.3, 1210, 24.5, 7.8, 85, (unixepoch()-86400*2)*1000, unixepoch()-86400*2),
    ('WSD-001', 6.2, 1200, 24.3, 7.7, 86, (unixepoch()-86400*1)*1000, unixepoch()-86400*1);

-- Alerts
INSERT INTO alerts (device_id, type, message, severity, acknowledged, created_at) VALUES
    ('WSD-004', 'ec_high', 'EC值過高：2100 μS/cm，超過上限 2000', 'warning', 0, unixepoch()-120),
    ('WSD-004', 'ph_abnormal', 'pH值異常：7.8，超出正常範圍 5.5-7.0', 'warning', 0, unixepoch()-300),
    ('WSD-007', 'offline', '設備離線超過 2 小時', 'critical', 0, unixepoch()-7200),
    ('WSD-013', 'offline', '設備離線超過 24 小時', 'critical', 0, unixepoch()-86400),
    ('WSD-003', 'do_low', '溶氧量偏低：7.2 mg/L', 'info', 1, unixepoch()-3600);

-- Settings defaults
INSERT INTO settings (key, value) VALUES
    ('wifi_ssid', 'GreenyFarm_5G'),
    ('wifi_password', '********'),
    ('ai_model', 'gpt-4o'),
    ('ai_temperature', '0.7'),
    ('mqtt_host', 'broker.greeny.local'),
    ('mqtt_port', '1883'),
    ('mqtt_topic', 'greeny/sensors/#'),
    ('notification_email', 'admin@greeny.farm'),
    ('notification_push', 'true'),
    ('notification_alert', 'true'),
    ('ph_min', '5.5'),
    ('ph_max', '7.0'),
    ('ec_max', '2000'),
    ('temp_min', '18'),
    ('temp_max', '30'),
    ('do_min', '6.0'),
    ('water_level_min', '20'),
    ('system_name', 'Greeny 智慧水耕控制系統'),
    ('system_version', 'v2.0.1'),
    ('firmware_version', 'v1.3.7');
