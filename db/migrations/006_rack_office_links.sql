-- Create missing racks to link all devices to offices
-- Distribution: TechForLiving(1)=A,D,E,H  GreenOffice(2)=B,F  FarmLab(3)=C,G

-- Zone D → TechForLiving (office 1)
INSERT INTO racks (name, office_id, device_id, location, status, layer_count, created_at) VALUES
('D區主水箱 / Zone D Main Tank', 1, 'WSD-007', '溫室D區 / Greenhouse D', 'maintenance', 4, unixepoch()),
('D區營養液槽 / Zone D Nutrient Tank', 1, 'WSD-008', '溫室D區 / Greenhouse D', 'active', 4, unixepoch());

-- Zone E → TechForLiving (office 1)
INSERT INTO racks (name, office_id, device_id, location, status, layer_count, created_at) VALUES
('E區育苗區 / Zone E Seedling', 1, 'WSD-009', '育苗室 / Seedling Room', 'active', 6, unixepoch()),
('E區營養液槽 / Zone E Nutrient Tank', 1, 'WSD-010', '育苗室 / Seedling Room', 'active', 4, unixepoch());

-- Zone F → GreenOffice Co. (office 2)
INSERT INTO racks (name, office_id, device_id, location, status, layer_count, created_at) VALUES
('F區主水箱 / Zone F Main Tank', 2, 'WSD-011', '屋頂F區 / Rooftop F', 'active', 4, unixepoch()),
('F區營養液槽 / Zone F Nutrient Tank', 2, 'WSD-012', '屋頂F區 / Rooftop F', 'active', 4, unixepoch());

-- Zone G → FarmLab Studio (office 3)
INSERT INTO racks (name, office_id, device_id, location, status, layer_count, created_at) VALUES
('G區實驗區 / Zone G Lab', 3, 'WSD-013', '實驗室 / Laboratory', 'inactive', 4, unixepoch()),
('G區營養液槽 / Zone G Nutrient Tank', 3, 'WSD-014', '實驗室 / Laboratory', 'active', 4, unixepoch());

-- Zone H → TechForLiving (office 1)
INSERT INTO racks (name, office_id, device_id, location, status, layer_count, created_at) VALUES
('H區展示區 / Zone H Display', 1, 'WSD-015', '展示廳 / Showroom', 'active', 5, unixepoch());

-- Also link the missing nutrient tanks in zones B and C
UPDATE racks SET device_id = 'WSD-004' WHERE id = 4;
UPDATE racks SET device_id = 'WSD-006' WHERE id = 6;
