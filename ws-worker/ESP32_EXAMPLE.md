# ESP32 ESP-IDF 範例 — Greeny 智慧水耕系統

## 環境建置

```bash
# 安裝 ESP-IDF v5.4+
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32
. ./export.sh

# 建立專案
idf.py create-project greeny-sensor
cd greeny-sensor
```

## sdkconfig 預設設定

在 `sdkconfig.defaults` 中加入：

```ini
CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS=y
CONFIG_LWIP_PTP_FINE_TEMPORAL_CORRECTION=y
```

## main/component.mk（如用 CMake 可跳過）

不需要，ESP-IDF v5 使用 CMake。

## CMakeLists.txt

```cmake
# 頂層 CMakeLists.txt（idf.py create-project 自動生成）
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(greeny-sensor)
```

## Kconfig.projbuild

```kconfig
menu "Greeny Sensor Configuration"

    config WIFI_SSID
        string "WiFi SSID"
        default "YOUR_WIFI_SSID"

    config WIFI_PASSWORD
        string "WiFi Password"
        default "YOUR_WIFI_PASSWORD"

    config DEVICE_ID
        string "Device ID"
        default "WSD-001"

    config OFFICE_ID
        string "Office ID"
        default "1"

    config WS_HOST
        string "WebSocket Host"
        default "greeny-ws.ai-caseylai.workers.dev"

    config SEND_INTERVAL_SEC
        int "Telemetry send interval (seconds)"
        default 5

    config USE_WEBSOCKET
        bool "Use WebSocket mode (uncheck for HTTPS POST only)"
        default y

endmenu
```

## main/main.c — 主程式

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "cJSON.h"

// WebSocket / HTTP（條件編譯）
#ifdef CONFIG_USE_WEBSOCKET
#include "esp_websocket_client.h"
#include "esp_tls.h"
#endif

#ifndef CONFIG_USE_WEBSOCKET
#include "esp_http_client.h"
#include "esp_tls.h"
#endif

static const char *TAG = "GREENY";

// ===== 接腳定義（依實際硬體調整）=====
#define PH_PIN          ADC1_CHANNEL_6   // GPIO34
#define EC_PIN          ADC1_CHANNEL_7   // GPIO35
#define TEMP_PIN        ADC1_CHANNEL_4   // GPIO32
#define NDVI_PIN        ADC1_CHANNEL_5   // GPIO33
#define RELAY1_PIN      GPIO_NUM_26
#define RELAY2_PIN      GPIO_NUM_27

// ===== WiFi =====
#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_connected = false;

// ===== ADC 校正 =====
static esp_adc_cal_characteristics_t adc_chars;

// ===== WebSocket（若啟用）=====
#ifdef CONFIG_USE_WEBSOCKET
static esp_websocket_client_handle_t s_ws_client = NULL;
static bool s_ws_connected = false;
#endif

// ===== 全域設定 =====
#define SEND_INTERVAL_MS  (CONFIG_SEND_INTERVAL_SEC * 1000)

// ============================================================
// ADC 讀取
// ============================================================
static void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PH_PIN,   ADC_ATTEN_DB_11);
    adc1_config_channel_atten(EC_PIN,   ADC_ATTEN_DB_11);
    adc1_config_channel_atten(TEMP_PIN, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(NDVI_PIN, ADC_ATTEN_DB_11);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
}

static float read_ph(void)
{
    int raw = adc1_get_raw(PH_PIN);
    if (raw < 0) return 0;
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    return (float)mv / 330.0f * 14.0f;  // 依感測器校正曲線調整
}

static float read_ec(void)
{
    int raw = adc1_get_raw(EC_PIN);
    if (raw < 0) return 0;
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    return (float)mv;  // 依感測器校正曲線調整
}

static float read_water_temp(void)
{
    // DS18B20 需要額外 OneWire 驅動，此處佔位
    // 可使用 onewire 或 ds18b20 ESP-IDF component
    return 25.0f;
}

static float read_ndvi(void)
{
    int raw = adc1_get_raw(NDVI_PIN);
    if (raw < 0) return 0;
    return (float)raw / 4095.0f;
}

static void read_spectral(float *red, float *green, float *blue, float *nir)
{
    // 多光譜感測器（如 AS7341）需 I2C 驅動
    // 此處佔位，依實際硬體回填
    *red   = 0;
    *green = 0;
    *blue  = 0;
    *nir   = 0;
}

static float read_water_level(void)
{
    return 85.0f;  // 依實際感測器調整
}

// ============================================================
// Relay 控制
// ============================================================
static void relay_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY1_PIN) | (1ULL << RELAY2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(RELAY1_PIN, 0);
    gpio_set_level(RELAY2_PIN, 0);
}

static void set_relay(int r1, int r2)
{
    gpio_set_level(RELAY1_PIN, r1);
    gpio_set_level(RELAY2_PIN, r2);
    ESP_LOGI(TAG, "Relay1=%d, Relay2=%d", r1, r2);
}

// ============================================================
// JSON 建構
// ============================================================
static char *build_telemetry_json(void)
{
    float ph = read_ph();
    float ec = read_ec();
    float temp = read_water_temp();
    float ndvi = read_ndvi();
    float s_red, s_green, s_blue, s_nir;
    read_spectral(&s_red, &s_green, &s_blue, &s_nir);
    float wl = read_water_level();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "telemetry");
    cJSON_AddStringToObject(root, "device_id", CONFIG_DEVICE_ID);
    cJSON_AddNumberToObject(root, "ph", ph);
    cJSON_AddNumberToObject(root, "ec", ec);
    cJSON_AddNumberToObject(root, "water_temp", temp);
    cJSON_AddNumberToObject(root, "water_level", wl);
    cJSON_AddNumberToObject(root, "ndvi", ndvi);
    cJSON_AddNumberToObject(root, "spectral_red", s_red);
    cJSON_AddNumberToObject(root, "spectral_green", s_green);
    cJSON_AddNumberToObject(root, "spectral_blue", s_blue);
    cJSON_AddNumberToObject(root, "spectral_nir", s_nir);
    cJSON_AddNumberToObject(root, "ts_ms", (double)(xTaskGetTickCount() * portTICK_PERIOD_MS));

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *build_telemetry_http_json(void)
{
    float ph = read_ph();
    float ec = read_ec();
    float temp = read_water_temp();
    float ndvi = read_ndvi();
    float s_red, s_green, s_blue, s_nir;
    read_spectral(&s_red, &s_green, &s_blue, &s_nir);
    float wl = read_water_level();

    cJSON *root = cJSON_CreateObject();
    // HTTP POST 不需要 type 欄位
    cJSON_AddStringToObject(root, "device_id", CONFIG_DEVICE_ID);
    cJSON_AddNumberToObject(root, "ph", ph);
    cJSON_AddNumberToObject(root, "ec", ec);
    cJSON_AddNumberToObject(root, "water_temp", temp);
    cJSON_AddNumberToObject(root, "water_level", wl);
    cJSON_AddNumberToObject(root, "ndvi", ndvi);
    cJSON_AddNumberToObject(root, "spectral_red", s_red);
    cJSON_AddNumberToObject(root, "spectral_green", s_green);
    cJSON_AddNumberToObject(root, "spectral_blue", s_blue);
    cJSON_AddNumberToObject(root, "spectral_nir", s_nir);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

// ============================================================
// 解析收到的指令（relay_cmd）
// ============================================================
static void handle_incoming_message(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "relay_cmd") == 0) {
        cJSON *r1 = cJSON_GetObjectItem(root, "relay1");
        cJSON *r2 = cJSON_GetObjectItem(root, "relay2");
        int v1 = (r1 && cJSON_IsNumber(r1)) ? r1->valueint : -1;
        int v2 = (r2 && cJSON_IsNumber(r2)) ? r2->valueint : -1;
        if (v1 >= 0 || v2 >= 0) {
            // 只更新有指定的 relay
            if (v1 >= 0) gpio_set_level(RELAY1_PIN, v1);
            if (v2 >= 0) gpio_set_level(RELAY2_PIN, v2);
            ESP_LOGI(TAG, "Relay cmd: relay1=%d relay2=%d",
                     gpio_get_level(RELAY1_PIN), gpio_get_level(RELAY2_PIN));
        }
    } else if (type && cJSON_IsString(type) && strcmp(type->valuestring, "ping") == 0) {
        ESP_LOGI(TAG, "Ping received");
    }

    cJSON_Delete(root);
}

// ============================================================
// WiFi 初始化
// ============================================================
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
            s_wifi_connected = false;
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler, NULL, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                         &wifi_event_handler, NULL, &inst_got_ip));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, waiting for connection...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                            pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "WiFi connection timeout");
    }
}

// ============================================================
// HTTPS POST 模式（不使用 WebSocket）
// ============================================================
#ifndef CONFIG_USE_WEBSOCKET

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    return ESP_OK;
}

static void send_telemetry_http(void)
{
    char *json = build_telemetry_http_json();
    if (!json) return;

    esp_http_client_config_t config = {
        .url = "https://greeny.techforliving.net/api/telemetry",
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach,  // 使用 ESP-IDF 內建 CA 憑證
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST OK, status=%d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_free(json);
}

static void telemetry_task(void *pv)
{
    while (1) {
        if (s_wifi_connected) {
            send_telemetry_http();
        }
        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

#endif

// ============================================================
// WebSocket 模式
// ============================================================
#ifdef CONFIG_USE_WEBSOCKET

static void ws_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        s_ws_connected = true;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        s_ws_connected = false;
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x08) {
            ESP_LOGW(TAG, "WS close frame received");
        } else if (data->data_len > 0) {
            ESP_LOGI(TAG, "WS recv: %.*s", data->data_len, (char *)data->data_ptr);
            handle_incoming_message((char *)data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        break;
    }
}

static void websocket_init(void)
{
    // 建 URL
    char ws_url[256];
    snprintf(ws_url, sizeof(ws_url),
             "wss://%s/ws?device_id=%s&office_id=%s",
             CONFIG_WS_HOST, CONFIG_DEVICE_ID, CONFIG_OFFICE_ID);

    esp_websocket_client_config_t ws_cfg = {
        .uri = ws_url,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
        .buffer_size = 1024,
    };

    s_ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    esp_websocket_client_start(s_ws_client);
    ESP_LOGI(TAG, "WebSocket client started, URL: %s", ws_url);
}

static void telemetry_ws_task(void *pv)
{
    // 等待 WS 連線
    while (!s_ws_connected) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    while (1) {
        if (s_ws_connected) {
            char *json = build_telemetry_json();
            if (json) {
                esp_websocket_client_send_text(s_ws_client, json, strlen(json), portMAX_DELAY);
                ESP_LOGI(TAG, "Sent: %s", json);
                cJSON_free(json);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

#endif

// ============================================================
// Main
// ============================================================
void app_main(void)
{
    ESP_LOGI(TAG, "Greeny Smart Hydroponics Sensor - " CONFIG_DEVICE_ID);

    // NVS 初始化（WiFi 需要）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化硬體
    adc_init();
    relay_init();

    // 連線 WiFi
    wifi_init();

#ifdef CONFIG_USE_WEBSOCKET
    // WebSocket 模式
    websocket_init();
    xTaskCreate(telemetry_ws_task, "ws_telemetry", 4096, NULL, 5, NULL);
#else
    // HTTPS POST 模式
    xTaskCreate(telemetry_task, "http_telemetry", 8192, NULL, 5, NULL);
#endif

    ESP_LOGI(TAG, "System running. Send interval: %d sec", CONFIG_SEND_INTERVAL_SEC);
}
```

## idf_component.yml（元件依賴）

在 `main/idf_component.yml` 中加入：

```yaml
dependencies:
  # WebSocket 用戶端（ESP-IDF v5 內建，若使用 v4 需手動加入）
  espressif/esp_websocket_client:
    version: ">0.1.0"
```

## 編譯與燒錄

```bash
# 設定配置（修改 WiFi、Device ID 等）
idf.py menuconfig
# → Greeny Sensor Configuration

# 編譯
idf.py build

# 燒錄並監看輸出
idf.py -p /dev/ttyUSB0 flash monitor
# macOS 通常為 /dev/cu.usbserial-xxxx

# 若要切換 HTTPS / WebSocket 模式
idf.py menuconfig
# → Greeny Sensor Configuration → Use WebSocket mode
```

## 訊息格式

### WebSocket 模式

**ESP32 → 雲端（每 N 秒）**
```json
{"type":"telemetry","device_id":"WSD-001","ph":6.5,"ec":1200,"water_temp":24.5,"ndvi":0.65,"spectral_red":450,"spectral_green":520,"spectral_blue":480,"spectral_nir":750,"water_level":85,"ts_ms":15000}
```

**雲端 → ESP32（用戶切換 Relay）**
```json
{"type":"relay_cmd","relay1":1,"relay2":0}
```

**雲端 → ESP32（Ping 保活）**
```json
{"type":"ping"}
```

### HTTPS POST 模式

**ESP32 → `/api/telemetry`（每 N 秒）**
```json
{"device_id":"WSD-001","ph":6.5,"ec":1200,"water_temp":24.5,"ndvi":0.65,"spectral_red":450,"spectral_green":520,"spectral_blue":480,"spectral_nir":750,"water_level":85}
```

## 目錄結構

```
greeny-sensor/
├── CMakeLists.txt
├── sdkconfig.defaults
├── Kconfig.projbuild
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.c
```

## 安全加固（生產環境）

### 1. mTLS（API Shield）
```c
// 在 esp_http_client_config_t 或 esp_websocket_client_config_t 中加入：
.tls_cfg = {
    .clientcert_pem = (const char *)client_cert_pem_start,
    .clientkey_pem  = (const char *)client_key_pem_start,
    .cacert_pem     = (const char *)server_cert_pem_start,
},
```

### 2. Cloudflare 設定
1. Dashboard → SSL/TLS → Client Certificates → 建立憑證
2. 下載 client certificate + private key
3. 用 `openssl` 轉 PEM 格式嵌入 ESP32
4. 設定 Cloudflare Rule：無有效憑證 → 403

### 3. 韌體 OTA 更新
```c
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
// 可從 Cloudflare R2 或其他 HTTPS 來源拉取新韌體
```

## 常見問題

**Q: 編譯失敗找不到 `esp_websocket_client.h`**
A: 執行 `idf.py add-dependency "espressif/esp_websocket_client>0.1.0"` 或在 `main/idf_component.yml` 加入依賴。

**Q: WiFi 連不上**
A: 檢查 SSID/密碼，確認 2.4GHz 頻段（ESP32 不支援 5GHz）。

**Q: WebSocket 斷線**
A: ESP-IDF WebSocket client 內建自動重連（`reconnect_timeout_ms`），預設 5 秒。

**Q: ADC 讀數不準**
A: ESP32 ADC 非線性，建議使用外部 ADC（如 ADS1115 via I2C）或查表校正。
```
