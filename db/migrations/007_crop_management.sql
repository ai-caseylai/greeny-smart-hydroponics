-- Greeny Crop Batch Management (入苗/收成管理)

-- Crop Batches (入苗記錄)
CREATE TABLE IF NOT EXISTS crop_batches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    office_id INTEGER NOT NULL REFERENCES offices(id),
    rack_id INTEGER REFERENCES racks(id),
    layer_number INTEGER,
    variety TEXT NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 0,
    unit TEXT NOT NULL DEFAULT '株' CHECK (unit IN ('株','盆','公斤','克')),
    status TEXT NOT NULL DEFAULT 'growing' CHECK (status IN ('growing','ready','harvested','failed')),
    seeded_at INTEGER NOT NULL DEFAULT (unixepoch()),
    expected_harvest_days INTEGER NOT NULL DEFAULT 30,
    notes TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_crop_batches_office ON crop_batches(office_id);
CREATE INDEX IF NOT EXISTS idx_crop_batches_rack ON crop_batches(rack_id);
CREATE INDEX IF NOT EXISTS idx_crop_batches_status ON crop_batches(status);

-- Harvest Logs (收成記錄)
CREATE TABLE IF NOT EXISTS harvest_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_id INTEGER NOT NULL REFERENCES crop_batches(id),
    quantity INTEGER NOT NULL DEFAULT 0,
    unit TEXT NOT NULL DEFAULT '株',
    quality TEXT NOT NULL DEFAULT 'good' CHECK (quality IN ('excellent','good','fair','poor')),
    notes TEXT NOT NULL DEFAULT '',
    harvested_at INTEGER NOT NULL DEFAULT (unixepoch()),
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_harvest_logs_batch ON harvest_logs(batch_id);
