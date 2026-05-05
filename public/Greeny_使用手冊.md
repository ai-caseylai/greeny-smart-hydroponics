# Greeny Smart Hydroponics Management System 使用手冊

**AquaGreen — From Seed To Table**

---

## 目錄

1. [系統簡介](#1-系統簡介)
2. [登入系統](#2-登入系統)
3. [角色與權限](#3-角色與權限)
4. [控制台 Dashboard](#4-控制台-dashboard)
5. [水質監測 Water Quality](#5-水質監測-water-quality)
6. [設備控制 Device Control](#6-設備控制-device-control)
7. [水耕架管理 Rack Management](#7-水耕架管理-rack-management)
8. [農作物管理 Crop Management](#8-農作物管理-crop-management)
9. [人員管理 Personnel Management](#9-人員管理-personnel-management)
10. [告警管理 Alerts](#10-告警管理-alerts)
11. [ESP32 設備連線](#11-esp32-設備連線)
12. [WorkBuddy Skill](#12-workbuddy-skill)
13. [常見問題 FAQ](#13-常見問題-faq)

---

## 1. 系統簡介

Greeny 是一套智慧水耕架管理系統，用於：
- 即時監控水質參數（pH、EC、水溫、NDVI、光譜）
- 遠端控制 ESP32 設備的 Relay 開關
- 管理多個辦公室/公司的水耕架
- 農作物入苗及收成記錄管理
- 人員管理（角色權限控制）
- 自動告警通知

**系統網址**: https://greeny.techforliving.net

### 系統架構

```
ESP32 設備 ←→ WebSocket/HTTPS ←→ Cloudflare (D1 + Worker) ←→ 網頁 Dashboard
```

### 硬體需求
- ESP32 微控制器（每個水耕架一台）
- 感測器：pH、EC、水溫、NDVI、多光波光譜
- 兩個 Relay（控制水泵、燈光等）

---

## 2. 登入系統

### 登入頁面

1. 開啟瀏覽器前往 https://greeny.techforliving.net
2. 輸入帳號和密碼
3. 點擊「登入」

### 示範帳號

| 帳號 | 密碼 | 角色 | 可見範圍 |
|------|------|------|----------|
| admin | admin123 | Super Admin | 所有辦公室 |
| office1 | admin123 | Office Admin | TechForLiving |
| staff1 | admin123 | Staff | TechForLiving（唯讀）|
| office2 | admin123 | Office Admin | GreenOffice Co. |

### 忘記密碼
請聯繫系統管理員 (Super Admin) 在人員管理頁面重設密碼。

---

## 3. 角色與權限

系統採用三級權限架構：

### Super Admin 系統管理員
- **可見範圍**：所有辦公室
- **功能權限**：
  - 查看所有辦公室的數據
  - 切換辦公室篩選器
  - 新增/編輯/刪除 辦公室
  - 新增/編輯/刪除 水耕架
  - 管理所有用戶（含建立 Super Admin）
  - 控制所有設備的 Relay
  - 發送 WhatsApp 訊息
  - 管理農作物入苗及收成

### Office Admin 辦公室管理員
- **可見範圍**：僅自己所屬辦公室
- **功能權限**：
  - 查看自己辦公室的數據
  - 新增/編輯/刪除 自己辦公室的水耕架
  - 管理自己辦公室的 Staff 用戶
  - 控制自己辦公室的設備 Relay
  - 發送 WhatsApp 訊息
  - 管理農作物入苗及收成

### Staff 一般員工
- **可見範圍**：僅自己所屬辦公室
- **功能權限**：
  - 查看自己辦公室的數據（唯讀）
  - 無法管理水耕架、人員、設備控制
  - 無法發送 WhatsApp

### 側邊欄選單（依角色顯示）

| 選單項目 | Super Admin | Office Admin | Staff |
|----------|:-----------:|:------------:|:-----:|
| 控制台 Dashboard | ✅ | ✅ | ✅ |
| 水質監測 Water Quality | ✅ | ✅ | ✅ |
| 設備控制 Device Control | ✅ | ✅ | ✅ |
| 水耕架管理 Racks | ✅ | ✅ | ✅ |
| 農作物管理 Crops | ✅ | ✅ | ✅ |
| 人員管理 Personnel | ✅ | ✅ | ❌ |

---

## 4. 控制台 Dashboard

### 功能概覽
Dashboard 提供系統整體狀態的即時總覽。

### KPI 卡片
頁面頂部顯示 5 個關鍵指標：

| 指標 | 說明 |
|------|------|
| 在線設備 | 目前在線設備數 / 總設備數 |
| 今日告警 | 今日產生的告警數量 |
| 平均 pH | 所有設備的平均 pH 值 |
| 平均水溫 | 所有設備的平均水溫 |
| 平均 NDVI | 所有設備的平均植被指數 |

### 辦公室篩選
- **Super Admin**：在側邊欄上方有辦公室下拉選單，可選擇「全部」或特定辦公室
- **其他角色**：自動鎖定到所屬辦公室，無需選擇

### 設備狀態表格

| 欄位 | 說明 |
|------|------|
| Device ID | 設備唯一識別碼（如 WSD-001）|
| Name | 設備名稱（中英雙語）|
| Company | 所屬公司/辦公室名稱 |
| Location | 設備所在位置 |
| pH / EC / °C / NDVI | 最新感測器讀數 |
| Status | 設備狀態 |
| Last Report | 最後回報時間 |

---

## 5. 水質監測 Water Quality

### 功能概覽
以**每個水耕架**為單位，獨立顯示該架的完整水質數據。

### 每個 Rack 卡片包含

#### 1. 感測器儀表板（4 個 Gauge）
| 參數 | 正常範圍 | 說明 |
|------|----------|------|
| pH | 5.5 – 7.0 | 酸鹼度 |
| EC | 800 – 1500 μS/cm | 電導率（營養液濃度）|
| Water Temp | 18 – 28 °C | 水溫 |
| NDVI | 0.3 – 0.8 | 植被指數 |

#### 2. 多光波圖（Multi-Spectrum Chart）
顯示 Blue、Green、Red、NIR 四個波段的強度。

#### 3. Relay 控制
- **Relay 1** 和 **Relay 2** 開關，透過 WebSocket 即時傳送指令

---

## 6. 設備控制 Device Control

### 功能概覽
以卡片形式顯示所有設備的狀態和最新讀數，支援遠端控制。

### 即時連線狀態
- **Live** = WebSocket 已連線（即時更新）
- **Polling** = 使用輪詢模式（非即時）

### 每個設備卡片顯示
- 設備名稱、位置、ID
- 狀態標籤
- 最新感測器讀數
- **Relay 1 / Relay 2** 開關
- 最後回報時間

---

## 7. 水耕架管理 Rack Management

### 7.1 辦公室列表頁（/racks）

#### 功能
- 顯示所有辦公室卡片
- 每張卡片顯示：辦公室名稱、聯絡人、電話、水耕架數量
- **WhatsApp 按鈕**：點擊可發送 WhatsApp 訊息

### 7.2 水耕架列表頁（/racks/office/:id）

#### 功能
- 顯示該辦公室下所有水耕架
- 每個水耕架可展開查看：
  - 環境數據（溫度、濕度、光照、pH、EC）
  - 蔬菜記錄（按層顯示）
  - WhatsApp 按鈕

---

## 8. 農作物管理 Crop Management

### 功能概覽
管理農作物的入苗及收成記錄，類似入貨/出貨管理。

### 頁面位置
側邊欄「農作物管理」

### 統計卡片
頁面頂部顯示三個統計：
- **生長中 / Growing**：目前狀態為「生長中」的數量
- **可收成 / Ready**：預計可收成的數量
- **已收成 / Harvested**：已記錄收成的總數量

### 入苗記錄 Tab

#### 新增入苗
1. 點擊「入苗 / New Seedling」按鈕
2. 填入：
   - **Office 辦公室**（Super Admin 需選擇）
   - **Rack 耕架**（選擇性）
   - **Variety 品種**（必填，例如：生菜）
   - **Quantity 數量**（必填）
   - **預計天數**（預設 10 天）
   - **Notes 備註**（選填）
3. 點擊「入苗 / Plant」
4. 成功後會顯示綠色提示「入苗成功！」

#### 入苗卡片顯示
每筆入苗記錄顯示：
- 品種名稱 + 狀態標籤（生長中/可收成/已收成/失敗）
- 數量、耕架、辦公室
- 入苗日期、已種植天數、預計天數
- 生長進度條

#### 操作按鈕
- **收成**：點擊後填入收成數量和品質，記錄收成
- **標記可收成**：當種植天數達到預計天數 80% 時出現
- **刪除**：刪除該筆入苗記錄

#### 狀態篩選
可依狀態篩選：全部 / 生長中 / 可收成 / 已收成 / 失敗

### 收成記錄 Tab

顯示所有收成記錄的表格，包含：
| 欄位 | 說明 |
|------|------|
| Date | 收成日期 |
| Variety | 品種 |
| Quantity | 收成數量 |
| Quality | 品質（優/良/可/差）|
| Rack | 所在耕架 |
| Office | 所屬辦公室 |
| Notes | 備註 |

### 收成操作
1. 在入苗記錄中，點擊該筆記錄的「收成」按鈕
2. 在彈出視窗中輸入：
   - **收成數量**（必填）
   - **品質**：優 Excellent / 良 Good / 可 Fair / 差 Poor
   - **備註**（選填）
3. 點擊「確認收成 / Confirm Harvest」
4. 成功後該筆記錄狀態改為「已收成」，收成記錄 Tab 會顯示新記錄

### 入苗收成日曆

頁面底部顯示月曆視圖：
- **綠色標記** = 入苗日期
- **橙色標記** = 預計收成日期
- **綠色外框** = 今天
- 可用左右箭頭切換月份，或點擊「今天」回到當月

---

## 9. 人員管理 Personnel Management

> 只有 Super Admin 和 Office Admin 可以看到此頁面。

### 角色階層

```
Super Admin 系統管理員 → 管理所有辦公室和用戶
  └─ Office Admin 辦公室管理員 → 管理所屬辦公室
       └─ Staff 一般員工 → 唯讀查看
```

### 新增用戶
1. 點擊「Add User / 新增用戶」
2. 填入：帳號、密碼、顯示名稱、角色、辦公室
3. 點擊「Create」

### 編輯用戶
- 點擊用戶右側的筆形圖標
- 可修改：顯示名稱、角色、辦公室（Super Admin 適用）

### 重設密碼
1. 點擊用戶的編輯圖標
2. 在「重設密碼」區塊輸入新密碼
3. 留空表示不更改
4. 點擊「儲存 / Save」

### 停用用戶
- 點擊用戶右側的紅色「停用」按鈕
- 用戶會被標記為 Inactive

### 重新啟用
- 已停用的用戶會顯示「Reactivate / 重新啟用」按鈕
- 點擊即可恢復

---

## 10. 告警管理 Alerts

### 告警類型

| 類型 | 觸發條件 | 嚴重程度 |
|------|----------|----------|
| pH 異常 | pH < 5.5 或 > 7.0 | Warning |
| EC 過高 | EC > 2000 μS/cm | Warning |
| 水溫異常 | < 18°C 或 > 30°C | Warning |
| 設備離線 | 超過設定時間未回報 | Critical |

---

## 11. ESP32 設備連線

### 支援框架
- **ESP-IDF**（推薦）：官方 Espressif IoT Development Framework
- **Arduino**：相容 Arduino IDE

### HTTPS 模式
ESP32 向 `/api/telemetry` POST 感測器數據。

### WebSocket 模式
即時雙向通訊，支援遠端 Relay 控制。

連線 URL：
```
wss://greeny-ws.ai-caseylai.workers.dev/ws?device_id=WSD-001&office_id=1
```

### 感測器欄位

| 欄位 | 類型 | 說明 |
|------|------|------|
| device_id | string | 設備 ID |
| ph | float | pH 值 |
| ec | float | 電導率 |
| water_temp | float | 水溫 |
| water_level | float | 水位 |
| ndvi | float | 植被指數 |
| spectral_red/green/blue/nir | float | 光譜數據 |
| relay1 / relay2 | int | Relay 狀態 |

詳細 ESP-IDF 範例代碼請參考 `ws-worker/ESP32_EXAMPLE.md`。

---

## 12. WorkBuddy Skill

### 安裝
將 `greeny-skill.zip` 解壓到 `~/.workbuddy/skills/greeny/`。

### 查詢指令

```bash
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py devices
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py telemetry --device_id WSD-001
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py alerts --status active
python3 ~/.workbuddy/skills/greeny/scripts/greeny/scripts/greeny_query.py racks --office_id 1
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py stats
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py offices
python3 ~/.workbuddy/skills/greeny/scripts/greeny_query.py users
```

### 控制指令

```bash
python3 ~/.workbuddy/skills/greeny/scripts/greeny_control.py acknowledge --alert_id 1
python3 ~/.workbuddy/skills/greeny/scripts/greeny_control.py add_vegetable --rack_id 1 --layer 2 --variety "生菜" --quantity 8
python3 ~/.workbuddy/skills/greeny/scripts/greeny_control.py notify --phone 85291234567 --message "Hello"
python3 ~/.workbuddy/skills/greeny/scripts/greeny_control.py create_user --username user1 --password pass123 --role staff --office_id 1
```

---

## 13. 常見問題 FAQ

### Q: 登入後看不到任何數據？
A: 請確認您的帳號已綁定正確的辦公室。聯繫 Super Admin 檢查。

### Q: 設備顯示 Offline？
A: 請檢查 ESP32 供電、WiFi 連線、以及是否已向系統註冊。

### Q: Relay 控制沒有反應？
A: 請確認設備已透過 WebSocket 連線（非 HTTPS 模式），Dashboard 顯示「Live」狀態。

### Q: 收成按鈕看不到？
A: 只有狀態為「生長中」或「可收成」的記錄才會顯示收成按鈕。已收成的記錄不會顯示。

### Q: 如何更改密碼？
A: 請聯繫 Super Admin 或 Office Admin，在人員管理頁面點擊您的編輯圖標進行重設。

### Q: 入苗後看不到記錄？
A: 請重新整理頁面。如果仍然看不到，請檢查瀏覽器 Console 是否有錯誤訊息。

---

## 聯絡支援

- **系統開發**：AquaGreen
- **系統網址**：https://greeny.techforliving.net
- **GitHub**：https://github.com/ai-caseylai/greeny-smart-hydroponics
