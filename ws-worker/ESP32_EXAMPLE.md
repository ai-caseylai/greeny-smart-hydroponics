# ESP32 Arduino 範例 — WebSocket 連線 Greeny

## 安裝庫
Arduino IDE → Library Manager → 搜尋並安裝 `WebSocketsClient` by Markus Sattler

## 程式碼

```cpp
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* ws_host = "greeny-ws.ai-caseylai.workers.dev";
const int ws_port = 443;
const char* ws_path = "/ws?device_id=WSD-001&office_id=1";
const char* fingerprint = ""; // 留空不安裝驗證

WebSocketsClient webSocket;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000; // 每 5 秒上傳一次

// ===== 感測器接腳（依實際硬體調整）=====
#define PH_PIN        34   // pH sensor analog
#define EC_PIN        35   // EC sensor analog
#define TEMP_PIN      32   // water temp (DS18B20 OneWire)
#define NDVI_PIN      33   // NDVI sensor
#define RELAY1_PIN    26   // Relay 1
#define RELAY2_PIN    27   // Relay 2

float readPH()     { return analogRead(PH_PIN) * 14.0 / 4095.0; }
float readEC()     { return analogRead(EC_PIN) * 3000.0 / 4095.0; }
float readTemp()   { return 25.0; /* 替換為 DS18B20 讀取 */ }
float readNDVI()   { return analogRead(NDVI_PIN) / 4095.0; }

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  // 連線 WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // 連線 WebSocket
  webSocket.beginSSL(ws_host, ws_port, ws_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("[WS] Connected");
      break;
    case WStype_TEXT: {
      Serial.printf("[WS] Received: %s\n", payload);
      // 處理來自雲端的指令
      StaticJsonDocument<256> doc;
      deserializeJson(doc, payload);
      String msgType = doc["type"];

      if (msgType == "relay_cmd") {
        int r1 = doc["relay1"];
        int r2 = doc["relay2"];
        digitalWrite(RELAY1_PIN, r1 ? HIGH : LOW);
        digitalWrite(RELAY2_PIN, r2 ? HIGH : LOW);
        Serial.printf("Relay1=%d, Relay2=%d\n", r1, r2);
      }
      break;
    }
  }
}

void loop() {
  webSocket.loop();

  // 定期上傳感測器數據
  if (millis() - lastSend > SEND_INTERVAL) {
    sendTelemetry();
    lastSend = millis();
  }
}

void sendTelemetry() {
  StaticJsonDocument<512> doc;
  doc["type"] = "telemetry";
  doc["device_id"] = "WSD-001";  // 替換為你的設備 ID
  doc["ph"] = readPH();
  doc["ec"] = readEC();
  doc["water_temp"] = readTemp();
  doc["water_level"] = 85;        // 替換為實際讀取
  doc["ndvi"] = readNDVI();
  doc["spectral_red"] = 0;        // 替換為光譜感測器讀取
  doc["spectral_green"] = 0;
  doc["spectral_blue"] = 0;
  doc["spectral_nir"] = 0;
  doc["ts_ms"] = millis();

  char buffer[512];
  serializeJson(doc, buffer);
  webSocket.sendTXT(buffer);
  Serial.printf("Sent: %s\n", buffer);
}
```

## 部署步驟

1. 修改 `ssid` / `password` 為你的 WiFi 資訊
2. 修改 `ws_path` 中的 `device_id` 和 `office_id`
3. 依實際硬體修改感測器接腳和讀取函數
4. 上傳到 ESP32
5. 打開 Serial Monitor (115200 baud) 查看連線狀態
6. 在 Dashboard 的設備控制頁面切換 Relay，ESP32 應會收到 `relay_cmd`

## 訊息格式

### ESP32 → 雲端（每 5 秒）
```json
{"type":"telemetry","device_id":"WSD-001","ph":6.5,"ec":1200,"water_temp":24.5,...}
```

### 雲端 → ESP32（當用戶切換 Relay）
```json
{"type":"relay_cmd","relay1":1,"relay2":0}
```

## 安全加固（生產環境）

使用 mTLS (API Shield) 保護 WebSocket 連線：
1. 在 Cloudflare Dashboard → SSL/TLS → Client Certificates 建立憑證
2. 將憑證和私鑰嵌入 ESP32 的 `WiFiClientSecure`
3. 設定 Cloudflare 規則：無有效憑證的請求返回 403
