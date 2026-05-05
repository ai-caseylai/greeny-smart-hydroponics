# Greeny Smart Hydroponics Management System

**By AquaGreen** — From Seed To Table

A full-stack hydroponic management system for monitoring water quality, controlling devices, managing racks, and real-time IoT communication.

## Architecture

```
ESP32 Devices
  ├── HTTPS POST → Cloudflare Pages Functions → D1 (SQLite)
  └── WebSocket  → Cloudflare Worker + Durable Object (Hibernation API)

React Frontend (Vite + TypeScript + Tailwind CSS)
  ├── REST API polling → Pages Functions
  └── WebSocket        → greeny-ws Worker (real-time telemetry + relay control)

Cloudflare Infrastructure:
  ├── Pages (greenie.techforliving.net) — Frontend + REST API
  ├── Worker (greeny-ws) — WebSocket + Durable Objects
  └── D1 (greeny-db) — SQLite database
```

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Frontend | React 18, TypeScript, Vite, Tailwind CSS, Recharts |
| Backend API | Cloudflare Pages Functions (file-based routing) |
| Real-time | Cloudflare Worker + Durable Objects (WebSocket Hibernation API) |
| Database | Cloudflare D1 (SQLite) |
| Auth | JWT (PBKDF2 password hashing) |
| i18n | react-i18next (zh-TW, zh-CN, en) |

## Project Structure

```
├── src/                          # React frontend
│   ├── components/
│   │   ├── AquaGreenLogo.tsx     # SVG logo component
│   │   └── layout/
│   │       ├── DashboardLayout.tsx
│   │       ├── Sidebar.tsx       # Nav + Office selector
│   │       └── Header.tsx        # Role badge
│   ├── context/
│   │   └── OfficeContext.tsx      # Global office selection + role-based locking
│   ├── hooks/
│   │   ├── useAuth.ts            # Login/logout, JWT management
│   │   ├── useDevices.ts         # Device list (filtered by office)
│   │   ├── useSensorData.ts      # Telemetry + dashboard stats hooks
│   │   ├── useRacks.ts           # Racks, vegetables, environment, automations
│   │   ├── useOffices.ts         # Office CRUD
│   │   └── useWebSocket.ts       # Real-time WebSocket connection to DO
│   ├── pages/
│   │   ├── LoginPage.tsx         # AquaGreen branded login
│   │   ├── DashboardPage.tsx     # KPI cards, trends, device status table
│   │   ├── WaterQualityPage.tsx  # Per-rack gauges, spectrum chart, relay controls
│   │   ├── DeviceControlPage.tsx # Device cards with relay toggles, WS status
│   │   ├── AlertsPage.tsx        # Alert list + acknowledge
│   │   └── racks/
│   │       ├── RackManagementPage.tsx  # Office cards + WhatsApp button
│   │       └── RackDetailPage.tsx      # Rack CRUD + vegetables + env + WhatsApp
│   ├── lib/
│   │   ├── api.ts                # apiFetch helper with JWT
│   │   └── utils.ts
│   └── types/
│       └── index.ts              # TypeScript interfaces
├── functions/                    # Cloudflare Pages Functions (REST API)
│   ├── _lib/
│   │   ├── types.ts              # Env interface, JwtPayload
│   │   ├── jwt.ts                # JWT sign/verify
│   │   └── password.ts           # PBKDF2 hashing
│   ├── api/
│   │   ├── _middleware.ts        # Auth middleware (excludes login, telemetry POST)
│   │   ├── auth/                 # login, me
│   │   ├── dashboard/            # stats (with office_id filter)
│   │   ├── devices/              # CRUD + office_name join
│   │   ├── telemetry.ts          # POST (ESP32) + GET (with spectral fields)
│   │   ├── alerts/               # List + acknowledge
│   │   ├── offices/              # CRUD
│   │   ├── racks/                # CRUD
│   │   ├── rack-vegetables/      # CRUD
│   │   ├── rack-environment/     # GET + POST
│   │   ├── automations/          # CRUD + run
│   │   ├── users/                # CRUD (role-based access)
│   │   └── workbuddy/            # WhatsApp send + status
├── ws-worker/                    # Separate Cloudflare Worker for WebSocket + DO
│   ├── src/
│   │   ├── index.ts              # Worker entry point (/ws, /health)
│   │   └── DeviceHub.ts          # Durable Object (Hibernation API)
│   ├── wrangler.toml
│   └── ESP32_EXAMPLE.md          # Arduino example for ESP32
├── db/
│   ├── schema.sql                # Base schema
│   ├── seed.sql                  # Demo data
│   └── migrations/
│       ├── 002_rack_management.sql
│       ├── 003_users_roles.sql
│       ├── 004_telemetry_sensors.sql  # NDVI, spectral, relays
│       ├── 005_device_names_i18n.sql  # Bilingual names
│       └── 006_rack_office_links.sql  # Link all devices to offices
└── wrangler.toml                 # Pages project config
```

## Database Schema

### Core Tables

| Table | Purpose |
|-------|---------|
| `devices` | ESP32 device registry (id, name, status, last_seen) |
| `telemetry` | Sensor readings: ph, ec, water_temp, water_level, ndvi, spectral_*, relay1/2 |
| `alerts` | Auto-generated alerts (ph_abnormal, ec_high, temp_abnormal, offline) |
| `users` | Authentication with role-based access (superadmin, office_admin, staff) |
| `offices` | Tenant companies (name, contact, whatsapp_number) |
| `racks` | Hydroponic racks linked to offices and devices |
| `rack_vegetables` | Vegetable records per rack layer |
| `rack_environment` | Environment data (temp, humidity, light, ph, ec) |
| `automations` | Scheduled automation tasks |

### Roles & Permissions

| Role | Access |
|------|--------|
| superadmin | All offices, user management, office selector visible |
| office_admin | Own office only, manage staff in own office |
| staff | Read-only own office data |

## API Endpoints

### Authentication
- `POST /api/auth/login` — Login, returns JWT with role + office_id
- `GET /api/auth/me` — Current user info

### Dashboard
- `GET /api/dashboard/stats?office_id=X` — KPI stats, device distribution, recent alerts

### Devices & Telemetry
- `GET /api/devices?office_id=X` — Device list with office_name
- `GET /api/telemetry?device_id=X&office_id=X&limit=100` — Telemetry readings
- `POST /api/telemetry` — ESP32 uploads sensor data (no auth required)

### Racks & Vegetables
- `GET/POST /api/racks?office_id=X` — Rack CRUD
- `GET/POST /api/rack-vegetables?rack_id=X` — Vegetable CRUD
- `GET/POST /api/rack-environment?rack_id=X` — Environment records

### Management
- `GET/POST/PATCH/DELETE /api/offices` — Office management
- `GET/POST/PATCH/DELETE /api/users` — User management (role-filtered)
- `GET/POST /api/automations` — Automation CRUD
- `POST /api/workbuddy/send-whatsapp` — Send WhatsApp message

### WebSocket (separate Worker)
- `wss://greeny-ws.ai-caseylai.workers.dev/ws?device_id=X&office_id=Y` — ESP32 connection
- `wss://greeny-ws.ai-caseylai.workers.dev/ws?dashboard=1&office_id=Y&token=JWT` — Dashboard connection

#### WebSocket Message Types

| Direction | Type | Description |
|-----------|------|-------------|
| ESP32 → DO | `telemetry` | Sensor data upload |
| Dashboard → DO | `relay` | Relay control command |
| DO → ESP32 | `relay_cmd` | Relay toggle instruction |
| DO → Dashboard | `telemetry_update` | Real-time sensor data |
| DO → Dashboard | `device_status` | Online/offline status |
| Any → DO | `ping` | Keep-alive (auto pong) |

## ESP32 Integration

### HTTPS Mode (simple, unidirectional)
```cpp
HTTPClient http;
http.begin("https://greenie.techforliving.net/api/telemetry");
http.addHeader("Content-Type", "application/json");
String json = "{\"device_id\":\"WSD-001\",\"ph\":6.5,\"ec\":1200,...}";
http.POST(json);
```

### WebSocket Mode (real-time, bidirectional)
See `ws-worker/ESP32_EXAMPLE.md` for full Arduino example using WebSocketsClient library.

## Deployment

### Prerequisites
- Node.js 18+
- Wrangler CLI (`npm install -g wrangler`)
- Cloudflare account with Pages + D1 + Workers

### Cloudflare Resources
- **D1 Database**: `greeny-db` (ID: `7191a23e-c43b-4962-b3fb-96818ac2d07c`)
- **Pages Project**: `greeny` → `greenie.techforliving.net`
- **Worker**: `greeny-ws` → `greeny-ws.ai-caseylai.workers.dev`

### Deploy Steps

```bash
# 1. Install dependencies
npm install

# 2. Deploy database migrations (in order)
npx wrangler d1 execute greeny-db --remote --file db/schema.sql
npx wrangler d1 execute greeny-db --remote --file db/seed.sql
npx wrangler d1 execute greeny-db --remote --file db/migrations/002_rack_management.sql
npx wrangler d1 execute greeny-db --remote --file db/migrations/003_users_roles.sql
npx wrangler d1 execute greeny-db --remote --file db/migrations/004_telemetry_sensors.sql
npx wrangler d1 execute greeny-db --remote --file db/migrations/005_device_names_i18n.sql
npx wrangler d1 execute greeny-db --remote --file db/migrations/006_rack_office_links.sql

# 3. Build & deploy frontend
npx vite build
npx wrangler pages deploy dist --project-name greeny

# 4. Deploy WebSocket Worker
cd ws-worker && npx wrangler deploy
```

### Environment Variables

**wrangler.toml (Pages)**
- `JWT_SECRET` — JWT signing key
- `WB_API_KEY` — WorkBuddy API key
- `WORKBUDDY_API_URL` — WorkBuddy endpoint
- `WORKBUDDY_API_KEY` — WorkBuddy auth key

**Frontend (optional `.env.local`)**
- `VITE_API_BASE` — API base URL (default: `/api`)
- `VITE_WS_URL` — WebSocket URL (default: `wss://greeny-ws.ai-caseylai.workers.dev`)

## Demo Accounts

| Username | Password | Role | Office |
|----------|----------|------|--------|
| admin | admin123 | superadmin | All |
| office1 | admin123 | office_admin | TechForLiving (#1) |
| staff1 | admin123 | staff | TechForLiving (#1) |
| office2 | admin123 | office_admin | GreenOffice Co. (#2) |

## Key Features

1. **Per-Rack Water Quality Monitoring** — Each rack shows individual pH, EC, water temp, NDVI gauges with multi-spectrum chart
2. **Real-time WebSocket** — ESP32 ↔ Durable Object ↔ Dashboard, with Hibernation API for cost efficiency
3. **Relay Control** — Toggle R1/R2 from dashboard, sent via WebSocket to ESP32
4. **Multi-Tenant** — Office-based data isolation with role-based access control
5. **WhatsApp Integration** — Send messages to office contacts directly from rack management page
6. **Bilingual UI** — Chinese + English device names and locations
7. **AquaGreen Branding** — Custom SVG logo on sidebar and login page

## Durable Object Cost Estimation

Using WebSocket Hibernation API (`ctx.acceptWebSocket()`):
- DO hibernates when idle → no duration billing
- Only billed for actual message processing time
- ~100 DOs with 10,000 connections: **~$10/month**

## License

Proprietary — AquaGreen
