# Greeny 智慧水耕系統 — 使用手冊

## 架構

```
ESP32 (WSD-001) ──WSS──→ Cloudflare Worker (DO) ──→ D1 (SQLite)
                          │
                  Pages Functions (REST API)
                          │
                  React Dashboard (greeny.techforliving.net)
```

## Dashboard 頁面

### 總覽 `/`
- KPI 卡片：在線設備、今日報警、pH、水溫、TDS
- 7 日水質趨勢圖（需多天數據）
- 設備狀態列表

### 設備控制 `/device-control`
- **R1/R2**：點擊切換開關，顯示執行狀態（等待命令 → 執行中 → 已執行）
- **pH Cal**：顯示當前校正值，輸入新值後點 Set 即時生效
- 延遲約 1-3 秒（D1 隊列 + 1 秒心跳）

### 水質監測 `/water-quality`
- 24 小時趨勢圖（pH、TDS、水溫）
- 儀表板卡片

## Relays 控制 API

```bash
# 開啟 R1+R2
curl -X POST "https://greeny-ws.techforliving.net/relay" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"WSD-001","relay1":1,"relay2":1}'

# 關閉 R1+R2  
curl -X POST "https://greeny-ws.techforliving.net/relay" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"WSD-001","relay1":0,"relay2":0}'

# 修改 pH 校正值
curl -X POST "https://greeny-ws.techforliving.net/relay" \
  -H "Content-Type: application/json" \
  -d '{"device_id":"WSD-001","ph_cal":16.5}'
```

## pH 校正

1. 探頭放入 pH 7.0 標準液
2. 看 OLED 或 Dashboard 顯示的 pH
3. 差值 = 7.0 - 顯示值
4. 新校正值 = 當前校正值 + 差值
5. 透過 Dashboard pH Cal 輸入或 curl 設定

## ESP32 WiFi Manager

ESP32 開機後會同時：
- 連接已儲存的 WiFi（STA）
- 廣播 `Greeny-Setup` AP（無密碼）

連到 `Greeny-Setup` → 瀏覽器開 `http://192.168.4.1` → 選擇 WiFi 網路 → 輸入密碼 → Save & Reboot

## GPIO 接線

| GPIO | 功能 |
|------|------|
| 21 | OLED SDA |
| 22 | OLED SCL |
| 34 | pH 計 (ADC) |
| 35 | TDS 計 (ADC) |
| 13 | DS18B20 溫度 (1-Wire) |
| 26 | Relay 1 |
| 27 | Relay 2 |

## 技術架構

```
ESP32 韌體: ESP-IDF v5.5
後端: Cloudflare Pages Functions + Worker + Durable Objects + D1
前端: Vite + React 18 + TypeScript + Tailwind CSS + Recharts
即時通訊: WSS (telemetry 上行) + D1 隊列 (relay 下行)
```

## 常用指令

```bash
# 燒錄韌體
cd esp32-firmware && idf.py -p /dev/cu.usbserial-110 flash

# 部署 Worker
cd ws-worker && npx wrangler deploy

# 部署前端
npx vite build && npx wrangler pages deploy dist --project-name greeny

# 查看 D1 資料
npx wrangler d1 execute greeny-db --remote --command "SELECT * FROM devices"
```
