-- Migration 003: Multi-user tiered roles + office_id assignment

-- Add office_id to users (nullable for superadmin who sees all offices)
ALTER TABLE users ADD COLUMN office_id INTEGER REFERENCES offices(id);

-- Update role constraint: drop and recreate users table with new roles
-- SQLite doesn't support ALTER COLUMN, so we recreate

CREATE TABLE users_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'staff' CHECK (role IN ('superadmin', 'office_admin', 'staff')),
    display_name TEXT NOT NULL DEFAULT '',
    office_id INTEGER REFERENCES offices(id),
    active INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

-- Copy existing users, mapping old roles: admin -> superadmin, manager -> office_admin, staff -> staff
INSERT INTO users_new (id, username, password_hash, role, display_name, office_id, active, created_at)
SELECT id, username, password_hash,
    CASE role
        WHEN 'admin' THEN 'superadmin'
        WHEN 'manager' THEN 'office_admin'
        ELSE 'staff'
    END,
    display_name, NULL, active, created_at
FROM users;

DROP TABLE users;
ALTER TABLE users_new RENAME TO users;
CREATE UNIQUE INDEX idx_users_username ON users(username);

-- Seed demo accounts (passwords use same PBKDF2 hash as existing admin)
-- admin123 -> same hash as existing admin user
-- Update existing admin to superadmin
UPDATE users SET role = 'superadmin', display_name = 'Super Admin' WHERE username = 'admin';

-- Add office_admin for TechForLiving (office_id=1)
-- Using the same password hash as admin (admin123)
INSERT OR IGNORE INTO users (username, password_hash, role, display_name, office_id)
SELECT 'office1', password_hash, 'office_admin', 'Office Manager', 1
FROM users WHERE username = 'admin';

-- Add staff for TechForLiving (office_id=1)
INSERT OR IGNORE INTO users (username, password_hash, role, display_name, office_id)
SELECT 'staff1', password_hash, 'staff', 'Staff User', 1
FROM users WHERE username = 'admin';

-- Add office_admin for GreenLife (office_id=2)
INSERT OR IGNORE INTO users (username, password_hash, role, display_name, office_id)
SELECT 'office2', password_hash, 'office_admin', 'GreenLife Manager', 2
FROM users WHERE username = 'admin';
