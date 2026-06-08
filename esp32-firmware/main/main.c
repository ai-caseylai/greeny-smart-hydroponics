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
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "cJSON.h"

#include "esp_http_client.h"

static const char *TAG = "GREENY";

// ===== I2C / OLED =====
#define OLED_I2C_PORT       I2C_NUM_0
#define OLED_SDA_GPIO       GPIO_NUM_21
#define OLED_SCL_GPIO       GPIO_NUM_22
#define OLED_I2C_ADDR       0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          (OLED_HEIGHT / 8)
#define OLED_COLS           (OLED_WIDTH / 6)   // 21 chars with 6x8 font
#define OLED_ROWS           8

// ===== 感測器 =====
#define PH_CHANNEL          ADC_CHANNEL_6   // GPIO34
#define EC_CHANNEL          ADC_CHANNEL_7   // GPIO35
#define TEMP_CHANNEL        ADC_CHANNEL_4   // GPIO32
#define RELAY_PINS          { GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_33 }
#define RELAY_COUNT         4
#define ONEWIRE_GPIO1       GPIO_NUM_13
#define ONEWIRE_GPIO2       GPIO_NUM_14

// ===== WiFi =====
#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_connected = false;
static char s_ip_str[16] = "---.---.---.---";

// ===== ADC =====
static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_cali_handle_t s_adc_cali = NULL;

// ===== 感測器讀數 =====
static float s_ph = 0, s_ec = 0, s_water_level = 0;
static float s_temp1 = 0, s_temp2 = 0;
static onewire_bus_handle_t s_ow_bus1 = NULL, s_ow_bus2 = NULL;
static ds18b20_device_handle_t s_ds1 = NULL, s_ds2 = NULL;
static int s_relay[RELAY_COUNT] = {0};
#ifndef CONFIG_USE_WEBSOCKET
static bool s_http_ok = false;
#endif

// ===== WebSocket =====
#ifdef CONFIG_USE_WEBSOCKET
static bool s_ws_connected = false;
#endif

#define SEND_INTERVAL_MS  (CONFIG_SEND_INTERVAL_SEC * 1000)

// ============================================================
// OLED (legacy I2C driver)
// ============================================================

static uint8_t s_fb[OLED_PAGES][OLED_WIDTH];
static uint8_t s_cursor_col = 0, s_cursor_row = 0;

// 6x8 ASCII font (0x20-0x7F)
static const uint8_t FONT6X8[96][6] = {
    [0]={0x00,0x00,0x00,0x00,0x00,0x00},[1]={0x00,0x00,0x5F,0x00,0x00,0x00},
    [2]={0x00,0x07,0x00,0x07,0x00,0x00},[3]={0x14,0x7F,0x14,0x7F,0x14,0x00},
    [4]={0x24,0x2A,0x7F,0x2A,0x12,0x00},[5]={0x23,0x13,0x08,0x64,0x62,0x00},
    [6]={0x36,0x49,0x55,0x22,0x50,0x00},[7]={0x00,0x05,0x03,0x00,0x00,0x00},
    [8]={0x00,0x1C,0x22,0x41,0x00,0x00},[9]={0x00,0x41,0x22,0x1C,0x00,0x00},
    [10]={0x08,0x2A,0x1C,0x2A,0x08,0x00},[11]={0x08,0x08,0x3E,0x08,0x08,0x00},
    [12]={0x00,0x50,0x30,0x00,0x00,0x00},[13]={0x08,0x08,0x08,0x08,0x08,0x00},
    [14]={0x00,0x60,0x60,0x00,0x00,0x00},[15]={0x20,0x10,0x08,0x04,0x02,0x00},
    [16]={0x3E,0x51,0x49,0x45,0x3E,0x00},[17]={0x00,0x42,0x7F,0x40,0x00,0x00},
    [18]={0x42,0x61,0x51,0x49,0x46,0x00},[19]={0x21,0x41,0x45,0x4B,0x31,0x00},
    [20]={0x18,0x14,0x12,0x7F,0x10,0x00},[21]={0x27,0x45,0x45,0x45,0x39,0x00},
    [22]={0x3C,0x4A,0x49,0x49,0x30,0x00},[23]={0x01,0x71,0x09,0x05,0x03,0x00},
    [24]={0x36,0x49,0x49,0x49,0x36,0x00},[25]={0x06,0x49,0x49,0x29,0x1E,0x00},
    [26]={0x00,0x36,0x36,0x00,0x00,0x00},[27]={0x00,0x56,0x36,0x00,0x00,0x00},
    [28]={0x00,0x08,0x14,0x22,0x41,0x00},[29]={0x14,0x14,0x14,0x14,0x14,0x00},
    [30]={0x41,0x22,0x14,0x08,0x00,0x00},[31]={0x02,0x01,0x51,0x09,0x06,0x00},
    [32]={0x32,0x49,0x79,0x41,0x3E,0x00},[33]={0x7E,0x11,0x11,0x11,0x7E,0x00},
    [34]={0x7F,0x49,0x49,0x49,0x36,0x00},[35]={0x3E,0x41,0x41,0x41,0x22,0x00},
    [36]={0x7F,0x41,0x41,0x22,0x1C,0x00},[37]={0x7F,0x49,0x49,0x49,0x41,0x00},
    [38]={0x7F,0x09,0x09,0x01,0x01,0x00},[39]={0x3E,0x41,0x41,0x51,0x32,0x00},
    [40]={0x7F,0x08,0x08,0x08,0x7F,0x00},[41]={0x00,0x41,0x7F,0x41,0x00,0x00},
    [42]={0x20,0x40,0x41,0x3F,0x01,0x00},[43]={0x7F,0x08,0x14,0x22,0x41,0x00},
    [44]={0x7F,0x40,0x40,0x40,0x40,0x00},[45]={0x7F,0x02,0x04,0x02,0x7F,0x00},
    [46]={0x7F,0x04,0x08,0x10,0x7F,0x00},[47]={0x3E,0x41,0x41,0x41,0x3E,0x00},
    [48]={0x7F,0x09,0x09,0x09,0x06,0x00},[49]={0x3E,0x41,0x51,0x21,0x5E,0x00},
    [50]={0x7F,0x09,0x19,0x29,0x46,0x00},[51]={0x46,0x49,0x49,0x49,0x31,0x00},
    [52]={0x01,0x01,0x7F,0x01,0x01,0x00},[53]={0x3F,0x40,0x40,0x40,0x3F,0x00},
    [54]={0x1F,0x20,0x40,0x20,0x1F,0x00},[55]={0x7F,0x20,0x18,0x20,0x7F,0x00},
    [56]={0x63,0x14,0x08,0x14,0x63,0x00},[57]={0x03,0x04,0x78,0x04,0x03,0x00},
    [58]={0x61,0x51,0x49,0x45,0x43,0x00},[59]={0x00,0x00,0x7F,0x41,0x41,0x00},
    [60]={0x02,0x04,0x08,0x10,0x20,0x00},[61]={0x41,0x41,0x7F,0x00,0x00,0x00},
    [62]={0x04,0x02,0x01,0x02,0x04,0x00},[63]={0x40,0x40,0x40,0x40,0x40,0x00},
    [64]={0x00,0x01,0x02,0x04,0x00,0x00},[65]={0x20,0x54,0x54,0x54,0x78,0x00},
    [66]={0x7F,0x48,0x44,0x44,0x38,0x00},[67]={0x38,0x44,0x44,0x44,0x20,0x00},
    [68]={0x38,0x44,0x44,0x48,0x7F,0x00},[69]={0x38,0x54,0x54,0x54,0x18,0x00},
    [70]={0x08,0x7E,0x09,0x01,0x02,0x00},[71]={0x08,0x14,0x54,0x54,0x3C,0x00},
    [72]={0x7F,0x08,0x04,0x04,0x78,0x00},[73]={0x00,0x44,0x7D,0x40,0x00,0x00},
    [74]={0x20,0x40,0x44,0x3D,0x00,0x00},[75]={0x00,0x7F,0x10,0x28,0x44,0x00},
    [76]={0x00,0x41,0x7F,0x40,0x00,0x00},[77]={0x7C,0x04,0x18,0x04,0x78,0x00},
    [78]={0x7C,0x08,0x04,0x04,0x78,0x00},[79]={0x38,0x44,0x44,0x44,0x38,0x00},
    [80]={0x7C,0x14,0x14,0x14,0x08,0x00},[81]={0x08,0x14,0x14,0x18,0x7C,0x00},
    [82]={0x7C,0x08,0x04,0x04,0x08,0x00},[83]={0x48,0x54,0x54,0x54,0x20,0x00},
    [84]={0x04,0x3F,0x44,0x40,0x20,0x00},[85]={0x3C,0x40,0x40,0x20,0x7C,0x00},
    [86]={0x1C,0x20,0x40,0x20,0x1C,0x00},[87]={0x3C,0x40,0x30,0x40,0x3C,0x00},
    [88]={0x44,0x28,0x10,0x28,0x44,0x00},[89]={0x0C,0x50,0x50,0x50,0x3C,0x00},
    [90]={0x44,0x64,0x54,0x4C,0x44,0x00},[91]={0x00,0x08,0x36,0x41,0x00,0x00},
    [92]={0x00,0x00,0x7F,0x00,0x00,0x00},[93]={0x00,0x41,0x36,0x08,0x00,0x00},
    [94]={0x08,0x04,0x08,0x10,0x08,0x00},[95]={0x00,0x00,0x00,0x00,0x00,0x00},
};

static void i2c_write(uint8_t addr, uint8_t ctrl, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, ctrl, true);
    if (len > 0) i2c_master_write(h, data, len, true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(OLED_I2C_PORT, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
}

static void oled_send_cmd(uint8_t cmd) { i2c_write(OLED_I2C_ADDR, 0x00, &cmd, 1); }

static void oled_init(void)
{
    oled_send_cmd(0xAE);
    oled_send_cmd(0xD5); oled_send_cmd(0x80);
    oled_send_cmd(0xA8); oled_send_cmd(0x3F);
    oled_send_cmd(0xD3); oled_send_cmd(0x00);
    oled_send_cmd(0x40);
    oled_send_cmd(0x8D); oled_send_cmd(0x14);
    oled_send_cmd(0x20); oled_send_cmd(0x00);
    oled_send_cmd(0xA1);
    oled_send_cmd(0xC8);
    oled_send_cmd(0xDA); oled_send_cmd(0x12);
    oled_send_cmd(0x81); oled_send_cmd(0xCF);
    oled_send_cmd(0xD9); oled_send_cmd(0xF1);
    oled_send_cmd(0xDB); oled_send_cmd(0x40);
    oled_send_cmd(0xA4);
    oled_send_cmd(0xA6);
    oled_send_cmd(0xAF);
}

static void oled_clear(void) { memset(s_fb, 0, sizeof(s_fb)); s_cursor_col = s_cursor_row = 0; }

static void oled_flush(void)
{
    for (int p = 0; p < OLED_PAGES; p++) {
        oled_send_cmd(0xB0 | p);
        oled_send_cmd(0x00);
        oled_send_cmd(0x10);
        i2c_write(OLED_I2C_ADDR, 0x40, s_fb[p], OLED_WIDTH);
    }
}

static void oled_set_cursor(uint8_t col, uint8_t row)
{
    s_cursor_col = col < OLED_COLS ? col : OLED_COLS - 1;
    s_cursor_row = row < OLED_ROWS ? row : OLED_ROWS - 1;
}

static void oled_write_char(char c)
{
    if (c < 0x20 || c > 0x7E) return;
    uint8_t idx = c - 0x20, px = s_cursor_col * 6, py = s_cursor_row;
    if (px + 6 > OLED_WIDTH) return;
    for (int i = 0; i < 6; i++) s_fb[py][px + i] = FONT6X8[idx][i];
    if (++s_cursor_col >= OLED_COLS) { s_cursor_col = 0; s_cursor_row = (s_cursor_row + 1) % OLED_ROWS; }
}

static void oled_write_str(const char *s) { while (*s) oled_write_char(*s++); }

static void oled_write_line(uint8_t row, const char *s)
{
    oled_set_cursor(0, row);
    for (int c = 0; c < OLED_COLS; c++) {
        uint8_t px = c * 6;
        for (int i = 0; i < 6 && (px + i) < OLED_WIDTH; i++) s_fb[row][px + i] = 0;
    }
    oled_write_str(s);
}

// ============================================================
// I2C + OLED setup
// ============================================================
static void oled_setup(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(OLED_I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(OLED_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
    oled_init();
    oled_write_line(0, "   GREENY SENSOR");
    oled_write_line(2, "  Booting...");
    oled_flush();
}

// ============================================================
// OLED 顯示更新
// ============================================================
static void oled_update_display(void)
{
    char line[22];
    snprintf(line, sizeof(line), "GREENY %s", CONFIG_DEVICE_ID);
    oled_write_line(0, line);
    snprintf(line, sizeof(line), "pH:%04.1f EC:%04.0f", s_ph, s_ec);
    oled_write_line(1, line);
    snprintf(line, sizeof(line), "T1:%05.1f T2:%05.1f C", s_temp1, s_temp2);
    oled_write_line(2, line);
    snprintf(line, sizeof(line), "R:%d%d%d%d",
             s_relay[0] ? 1 : 0, s_relay[1] ? 1 : 0,
             s_relay[2] ? 1 : 0, s_relay[3] ? 1 : 0);
    oled_write_line(3, line);
    snprintf(line, sizeof(line), "pHcal:%.2f", s_ph_cal);
    oled_write_line(4, line);
#ifdef CONFIG_USE_WEBSOCKET
    snprintf(line, sizeof(line), "WiFi:%s WSS:%s",
             s_wifi_connected ? "OK" : "--", s_ws_connected ? "OK" : "--");
#else
    snprintf(line, sizeof(line), "WiFi:%s HTTP:%s",
             s_wifi_connected ? "OK" : "--", s_http_ok ? "OK" : "--");
#endif
    oled_write_line(5, line);
    oled_write_line(6, s_ip_str);
    uint32_t u = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    snprintf(line, sizeof(line), "UP:%02lu:%02lu:%02lu", u / 3600, (u % 3600) / 60, u % 60);
    oled_write_line(7, line);
    oled_flush();
}

// ============================================================
// ADC
// ============================================================
static void adc_init_all(void)
{
    adc_oneshot_unit_init_cfg_t uc = { .unit_id = ADC_UNIT_1, .clk_src = ADC_DIGI_CLK_SRC_DEFAULT, .ulp_mode = ADC_ULP_MODE_DISABLE };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&uc, &s_adc1_handle));
    adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_11, .bitwidth = ADC_BITWIDTH_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, PH_CHANNEL, &cc));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, EC_CHANNEL, &cc));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, TEMP_CHANNEL, &cc));
    adc_cali_line_fitting_config_t lc = { .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_11, .bitwidth = ADC_BITWIDTH_12, .default_vref = 1100 };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&lc, &s_adc_cali));
}

static int adc_read_raw(adc_channel_t ch) { int v = 0; adc_oneshot_read(s_adc1_handle, ch, &v); return v; }

static uint32_t adc_read_mv(adc_channel_t ch)
{
    int raw = 0, mv = 0;
    adc_oneshot_read(s_adc1_handle, ch, &raw);
    if (s_adc_cali) adc_cali_raw_to_voltage(s_adc_cali, raw, &mv);
    return (uint32_t)mv;
}

static float s_ph_cal = 21.34f;  // 預設值，可透過 menuconfig 或 relay_cmd 調整

static float read_ph(void)
{
    int samples[6];
    for (int i = 0; i < 6; i++) samples[i] = adc_read_raw(PH_CHANNEL);
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 6; j++)
            if (samples[i] > samples[j]) { int t = samples[i]; samples[i] = samples[j]; samples[j] = t; }
    int sum = 0;
    for (int i = 1; i < 5; i++) sum += samples[i];
    float volt = (float)sum / 4.0f * 3.3f / 4095.0f;
    // Nernst 溫度修正：斜率 = -5.70 * (T°C + 273.15) / 298.15
    float T = (s_temp1 > 0) ? s_temp1 : 25.0f;
    float slope = -5.70f * (T + 273.15f) / 298.15f;
    return slope * volt + s_ph_cal;
}
static float read_ec(void) { return (float)adc_read_mv(EC_CHANNEL); }
static float read_water_level(void) { return 85.0f; }

static void ds18b20_init_one(onewire_bus_handle_t *bus, ds18b20_device_handle_t *ds, gpio_num_t gpio)
{
    onewire_bus_rmt_config_t rmt = { .max_rx_bytes = 10 };
    onewire_bus_config_t cfg = { .bus_gpio_num = gpio };
    if (onewire_new_bus_rmt(&cfg, &rmt, bus) != ESP_OK) {
        ESP_LOGW(TAG, "1-Wire GPIO%d init failed", gpio);
        return;
    }
    ds18b20_config_t ds_cfg = {};
    // Assume single device per bus
    if (ds18b20_new_device_from_bus(*bus, &ds_cfg, ds) == ESP_OK) {
        onewire_device_address_t addr;
        ds18b20_get_device_address(*ds, &addr);
        ESP_LOGI(TAG, "DS18B20 GPIO%d addr=%016llX", gpio, addr);
    } else {
        ESP_LOGW(TAG, "DS18B20 not found on GPIO%d", gpio);
    }
}

static void ds18b20_init(void)
{
    ds18b20_init_one(&s_ow_bus1, &s_ds1, ONEWIRE_GPIO1);
    ds18b20_init_one(&s_ow_bus2, &s_ds2, ONEWIRE_GPIO2);
}

static void read_temp_sensors(void)
{
    if (s_ds1) {
        ds18b20_trigger_temperature_conversion(s_ds1);
        ds18b20_get_temperature(s_ds1, &s_temp1);
    }
    if (s_ds2) {
        ds18b20_trigger_temperature_conversion(s_ds2);
        ds18b20_get_temperature(s_ds2, &s_temp2);
    }
}

static void read_all_sensors(void)
{
    s_ph = read_ph(); s_ec = read_ec();
    s_water_level = read_water_level();
    read_temp_sensors();
}

// ============================================================
// Relay
// ============================================================
static const int RELAY_GPIOS[RELAY_COUNT] = RELAY_PINS;

static void relay_init_all(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < RELAY_COUNT; i++) mask |= (1ULL << RELAY_GPIOS[i]);
    gpio_config_t c = { .pin_bit_mask = mask, .mode = GPIO_MODE_OUTPUT,
                        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
                        .intr_type = GPIO_INTR_DISABLE };
    gpio_config(&c);
    for (int i = 0; i < RELAY_COUNT; i++) { s_relay[i] = 0; gpio_set_level(RELAY_GPIOS[i], 0); }
}

static void set_relay(int idx, int val)
{
    if (idx >= 0 && idx < RELAY_COUNT && val >= 0) {
        s_relay[idx] = val;
        gpio_set_level(RELAY_GPIOS[idx], val);
        ESP_LOGI(TAG, "Relay%d (GPIO%d) = %d", idx + 1, RELAY_GPIOS[idx], val);
    }
}

// ============================================================
// JSON
// ============================================================
#ifdef CONFIG_USE_WEBSOCKET
static char *build_telemetry_ws_json(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "type", "telemetry");
    cJSON_AddStringToObject(r, "device_id", CONFIG_DEVICE_ID);
    cJSON_AddNumberToObject(r, "ph", s_ph); cJSON_AddNumberToObject(r, "ec", s_ec);
    cJSON_AddNumberToObject(r, "water_temp", s_temp1); cJSON_AddNumberToObject(r, "water_temp2", s_temp2); cJSON_AddNumberToObject(r, "water_level", s_water_level);
    for (int i = 0; i < RELAY_COUNT; i++) {
        char key[8]; snprintf(key, sizeof(key), "relay%d", i + 1);
        cJSON_AddNumberToObject(r, key, s_relay[i]);
    }
    cJSON_AddNumberToObject(r, "ts_ms", (double)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    char *j = cJSON_PrintUnformatted(r); cJSON_Delete(r); return j;
}
#else
static char *build_telemetry_http_json(void)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "device_id", CONFIG_DEVICE_ID);
    cJSON_AddNumberToObject(r, "ph", s_ph); cJSON_AddNumberToObject(r, "ec", s_ec);
    cJSON_AddNumberToObject(r, "water_temp", s_temp1); cJSON_AddNumberToObject(r, "water_temp2", s_temp2); cJSON_AddNumberToObject(r, "water_level", s_water_level);
    for (int i = 0; i < RELAY_COUNT; i++) {
        char key[8]; snprintf(key, sizeof(key), "relay%d", i + 1);
        cJSON_AddNumberToObject(r, key, s_relay[i]);
    }
    char *j = cJSON_PrintUnformatted(r); cJSON_Delete(r); return j;
}
#endif

// ============================================================
// 訊息處理 (WSS only)
// ============================================================
#ifdef CONFIG_USE_WEBSOCKET
static void ws_send_frame(const char *text);
static void handle_incoming_message(const char *data, int len)
{
    cJSON *r = cJSON_ParseWithLength(data, len);
    if (!r) return;
    cJSON *t = cJSON_GetObjectItem(r, "type");
    if (t && cJSON_IsString(t) && strcmp(t->valuestring, "relay_cmd") == 0) {
        cJSON *cal = cJSON_GetObjectItem(r, "ph_cal");
        if (cal && cJSON_IsNumber(cal)) {
            s_ph_cal = (float)cal->valuedouble;
            ESP_LOGI(TAG, "pH calibration set to %.2f", s_ph_cal);
        }
        for (int i = 0; i < RELAY_COUNT; i++) {
            char key[8]; snprintf(key, sizeof(key), "relay%d", i + 1);
            cJSON *v = cJSON_GetObjectItem(r, key);
            if (v && cJSON_IsNumber(v)) set_relay(i, v->valueint);
        }
    } else if (t && cJSON_IsString(t) && strcmp(t->valuestring, "ping") == 0) {
        if (s_ws_connected) ws_send_frame("{\"type\":\"pong\"}");
    }
    cJSON_Delete(r);
}
#endif

// ============================================================
// WiFi
// ============================================================
static void wifi_handler(void *a, esp_event_base_t b, int32_t id, void *d)
{
    if (b == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) esp_wifi_connect();
        else if (id == WIFI_EVENT_STA_DISCONNECTED) { s_wifi_connected = false; strcpy(s_ip_str, "---"); esp_wifi_connect(); }
    } else if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&((ip_event_got_ip_t *)d)->ip_info.ip));
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_all(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t c = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&c));
    esp_event_handler_instance_t i1, i2;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL, &i1));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL, &i2));
    wifi_config_t w = {0};
    strlcpy((char *)w.sta.ssid, CONFIG_WIFI_SSID, sizeof(w.sta.ssid));
    strlcpy((char *)w.sta.password, CONFIG_WIFI_PASSWORD, sizeof(w.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w));
    ESP_ERROR_CHECK(esp_wifi_start());
    // 設定 DNS fallback（Cloudflare 1.1.1.1）避免路由器 DNS 無法解析 workers.dev
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_dns_info_t dns;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_FALLBACK, &dns);
        if (dns.ip.u_addr.ip4.addr == 0) {
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_FALLBACK, &dns);
            ESP_LOGI(TAG, "DNS fallback set to 1.1.1.1");
        }
    }
    oled_write_line(7, "WiFi connecting..."); oled_flush();
    if (!(xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000)) & WIFI_CONNECTED_BIT)) {
        oled_write_line(7, "WiFi FAILED!"); oled_flush();
    }
}

// ============================================================
// HTTPS mode
// ============================================================
#ifndef CONFIG_USE_WEBSOCKET
static void send_telemetry_http(void)
{
    char *j = build_telemetry_http_json(); if (!j) return;
    esp_http_client_config_t c = { .url = "https://greenie.techforliving.net/api/telemetry", .method = HTTP_METHOD_POST, .timeout_ms = 5000, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t cl = esp_http_client_init(&c);
    esp_http_client_set_header(cl, "Content-Type", "application/json");
    esp_http_client_set_post_field(cl, j, strlen(j));
    esp_err_t err = esp_http_client_perform(cl);
    int status = esp_http_client_get_status_code(cl);
    s_http_ok = (err == ESP_OK && status == 200);
    ESP_LOGI(TAG, "HTTP POST %s (status=%d)", s_http_ok ? "OK" : "FAIL", status);
    esp_http_client_cleanup(cl); cJSON_free(j);
}
static void telemetry_task(void *pv) { while (1) { if (s_wifi_connected) { read_temp_sensors(); send_telemetry_http(); } vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS)); } }
#endif

// ============================================================
// WSS mode
// ============================================================
#ifdef CONFIG_USE_WEBSOCKET
#include "esp_tls.h"
#include "mbedtls/base64.h"
#include "esp_random.h"
#include <lwip/sockets.h>

static int s_ws_sock = -1;
static esp_tls_t *s_tls = NULL;

static int ws_read_frame(char *buf, int max_len)
{
    uint8_t hdr[2];
    int n = esp_tls_conn_read(s_tls, hdr, 2);
    if (n != 2) return -1;
    uint8_t opcode = hdr[0] & 0x0F, masked = hdr[1] & 0x80;
    int len = hdr[1] & 0x7F;
    if (len == 126) { uint8_t ext[2]; esp_tls_conn_read(s_tls, ext, 2); len = (ext[0]<<8)|ext[1]; }
    else if (len == 127) { uint8_t ext[8]; esp_tls_conn_read(s_tls, ext, 8); len = (ext[4]<<24)|(ext[5]<<16)|(ext[6]<<8)|ext[7]; }
    uint8_t mask[4] = {0};
    if (masked) esp_tls_conn_read(s_tls, mask, 4);
    if (len > max_len - 1) len = max_len - 1;
    esp_tls_conn_read(s_tls, (uint8_t *)buf, len);
    if (masked) for (int i = 0; i < len; i++) buf[i] ^= mask[i % 4];
    buf[len] = 0;
    return (opcode == 0x01 || opcode == 0x02) ? len : -1;
}

static void ws_send_frame(const char *text)
{
    int n = strlen(text);
    uint8_t frame[256], *p = frame;
    *p++ = 0x81;
    if (n < 126) *p++ = 0x80 | n;
    else { *p++ = 0x80 | 126; *p++ = (n >> 8) & 0xFF; *p++ = n & 0xFF; }
    uint8_t mask[4];
    for (int i = 0; i < 4; i++) mask[i] = esp_random() & 0xFF;
    memcpy(p, mask, 4); p += 4;
    for (int i = 0; i < n; i++) *p++ = text[i] ^ mask[i % 4];
    esp_tls_conn_write(s_tls, frame, p - frame);
}

static bool ws_connect(const char *host, int port, const char *path)
{
    esp_tls_cfg_t tls_cfg = { .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 10000 };
    s_tls = esp_tls_init();
    if (esp_tls_conn_new_sync(host, strlen(host), port, &tls_cfg, s_tls) != 1) {
        ESP_LOGE(TAG, "TLS connect failed"); esp_tls_conn_destroy(s_tls); s_tls = NULL; return false;
    }
    // WebSocket handshake
    uint8_t key_bytes[16];
    for (int i = 0; i < 16; i++) key_bytes[i] = esp_random() & 0xFF;
    char b64_key[32]; size_t olen;
    mbedtls_base64_encode((unsigned char *)b64_key, sizeof(b64_key), &olen, key_bytes, 16);
    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", path, host, b64_key);
    esp_tls_conn_write(s_tls, req, req_len);
    char resp[512]; int total = 0;
    int64_t deadline = esp_timer_get_time() + 5000000;
    while (total < sizeof(resp) - 1) {
        int r = esp_tls_conn_read(s_tls, (uint8_t *)(resp + total), 1);
        if (r < 0) { esp_tls_conn_destroy(s_tls); s_tls = NULL; return false; }
        total++; resp[total] = 0;
        if (strstr(resp, "\r\n\r\n")) break;
        if (esp_timer_get_time() > deadline) { esp_tls_conn_destroy(s_tls); s_tls = NULL; return false; }
    }
    if (!strstr(resp, "101")) { esp_tls_conn_destroy(s_tls); s_tls = NULL; return false; }
    // Set socket timeout for non-blocking reads
    int fd;
    if (esp_tls_get_conn_sockfd(s_tls, &fd) == ESP_OK) {
        struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 }; // 200ms
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    ESP_LOGI(TAG, "WSS handshake OK");
    return true;
}

static void websocket_init(void)
{
    char path[128];
    snprintf(path, sizeof(path), "/ws?device_id=%s&office_id=%s", CONFIG_DEVICE_ID, CONFIG_OFFICE_ID);
    if (ws_connect(CONFIG_WS_HOST, 443, path)) s_ws_connected = true;
}
static void telemetry_ws_task(void *pv)
{
    while (1) {
        if (!s_ws_connected) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            websocket_init();
            continue;
        }
        read_temp_sensors();
        char *j = build_telemetry_ws_json();
        if (j) { ws_send_frame(j); cJSON_free(j); }
        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}
static void ws_reader_task(void *pv)
{
    while (1) {
        if (!s_ws_connected || !s_tls) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        char buf[512];
        int r = ws_read_frame(buf, sizeof(buf));
        if (r > 0) handle_incoming_message(buf, r);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif // CONFIG_USE_WEBSOCKET

// ============================================================
// OLED refresh task
// ============================================================
static void oled_task(void *pv) { while (1) { oled_update_display(); vTaskDelay(pdMS_TO_TICKS(1000)); } }

// ============================================================
// Main
// ============================================================
void app_main(void)
{
    ESP_LOGI(TAG, "=== Greeny Smart Hydroponics - %s ===", CONFIG_DEVICE_ID);
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    adc_init_all(); relay_init_all(); ds18b20_init(); oled_setup();
    wifi_init_all();
    xTaskCreate(oled_task, "oled", 3072, NULL, 3, NULL);
#ifdef CONFIG_USE_WEBSOCKET
    xTaskCreate(telemetry_ws_task, "ws_tel", 8192, NULL, 5, NULL);
    xTaskCreate(ws_reader_task, "ws_rdr", 4096, NULL, 4, NULL);
#else
    xTaskCreate(telemetry_task, "http_tel", 8192, NULL, 5, NULL);
#endif
    ESP_LOGI(TAG, "System running. Interval=%d sec Mode=%s", CONFIG_SEND_INTERVAL_SEC,
#ifdef CONFIG_USE_WEBSOCKET
             "WSS"
#else
             "HTTPS"
#endif
    );
}
