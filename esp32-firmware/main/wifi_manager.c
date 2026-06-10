#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_http_server.h"



bool s_ap_mode = false;

// ---- WiFi Manager HTML pages ----
static const char WIFI_HTML[] =
"<!DOCTYPE html>"
"<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Greeny WiFi Setup</title>"
"<style>body{font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;display:flex;"
"justify-content:center;align-items:center;min-height:100vh;margin:0}"
"form{background:#1e293b;padding:30px;border-radius:12px;width:320px}"
"h2{color:#38bdf8;text-align:center}label{display:block;margin-top:12px;font-size:14px;color:#94a3b8}"
"input{width:100%;padding:10px;margin-top:4px;border:1px solid #334155;border-radius:6px;"
"background:#0f172a;color:#e2e8f0;font-size:14px;box-sizing:border-box}"
"button{width:100%;padding:12px;margin-top:20px;background:#00a65a;border:none;border-radius:6px;"
"color:#fff;font-size:16px;cursor:pointer}"
"button:hover{background:#008a4a}.msg{text-align:center;font-size:12px;margin-top:10px;color:#94a3b8}"
"</style></head><body><form method='POST' action='/save'>"
"<h2>Greeny WiFi Setup</h2>"
"<label>SSID</label><input name='ssid' placeholder='WiFi name' required>"
"<label>Password</label><input name='pass' type='password' placeholder='WiFi password'>"
"<button type='submit'>Save & Reboot</button>"
"<p class='msg'>ESP32 will reboot with new settings</p>"
"</form></body></html>";

static const char WIFI_OK_HTML[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Saved</title><style>body{font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;"
"display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}"
"div{text-align:center}h2{color:#00a65a}p{color:#94a3b8}</style></head>"
"<body><div><h2>WiFi Saved!</h2><p>ESP32 is rebooting...</p></div></body></html>";

static esp_err_t wifi_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WIFI_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void url_decode(char *s) {
    char *o = s;
    while (*s) {
        if (*s == '%' && s[1] && s[2]) {
            int hi = s[1] >= 'A' ? (s[1] & 0xDF) - 'A' + 10 : s[1] - '0';
            int lo = s[2] >= 'A' ? (s[2] & 0xDF) - 'A' + 10 : s[2] - '0';
            *o++ = (hi << 4) | lo;
            s += 3;
        } else if (*s == '+') {
            *o++ = ' ';
            s++;
        } else {
            *o++ = *s++;
        }
    }
    *o = 0;
}

static esp_err_t wifi_save_handler(httpd_req_t *req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    buf[len] = 0;

    char ssid[33] = {0}, pass[65] = {0};
    char *p = strstr(buf, "ssid=");
    if (p) { p += 5; int i = 0; while (*p && *p != '&' && i < 32) ssid[i++] = *p++; }
    p = strstr(buf, "pass=");
    if (p) { p += 5; int i = 0; while (*p && *p != '&' && i < 64) pass[i++] = *p++; }
    url_decode(ssid); url_decode(pass);

    ESP_LOGI("GREENY", "WiFi Manager: saving SSID=%s", ssid);

    if (ssid[0]) {
        nvs_handle_t nvs;
        nvs_open("greeny", NVS_READWRITE, &nvs);
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, WIFI_OK_HTML, HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

void wifi_manager_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    httpd_start(&server, &cfg);

    httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = wifi_get_handler };
    httpd_uri_t uri_save = { .uri = "/save", .method = HTTP_POST, .handler = wifi_save_handler };
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_save);

    ESP_LOGI("GREENY", "WiFi Manager: http://192.168.4.1");
}
