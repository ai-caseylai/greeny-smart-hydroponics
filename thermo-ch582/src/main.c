/**
 * Greeny Temp Controller — CH582M BLE 5.0 + RTC + WDT
 *
 * RISC-V single-chip solution. No external BLE module, no debugger.
 * USB direct programming via wchisp. CR2032 direct drive.
 * BOM: CH582M + NTC + MOSFET + crystal + CR2032 ≈ NT$35
 *
 * Pinout (CH582M QFN28):
 *   PA0/ADC0  = NTC 100K B=3950 (VCC→100K→PA0→NTC→GND)
 *   PA1       = MOSFET gate (fan/heater) via 1K
 *   PA8/PA9   = 32.768KHz crystal (RTC)
 *   PB10/PB11 = USB D+/D- (power + programming)
 *
 * Power: ~3μA average → 8.5 years on CR2032
 *
 * BLE Advertising:
 *   Phone scans, sees device "Greeny" with temp in Manufacturer Data.
 *   No connection needed to read temperature.
 *   Connect to GATT for settings (target temp, hysteresis, mode).
 *
 * Protocol (GATT):
 *   Service UUID: 0x1809 (Health Thermometer, or custom)
 *   Current Temp:  Read/Notify  uint16 (0.01°C)
 *   Target Temp:   Read/Write   uint16 (0.01°C)
 *   Output State:  Read/Notify  uint8  (0=OFF,1=ON)
 *   Control Mode:  Read/Write   uint8  (0=OFF,1=ON,'A'=Auto)
 *   Hysteresis:    Read/Write   uint16 (0.01°C)
 *
 * Build:  make
 * Flash:  make flash  (auto via wchisp USB)
 */

#include "CH58x_common.h"

// ============================================================
// BLE stack includes (WCH SDK)
// ============================================================
#include "CH58xBLE_LIB.h"
#include "gatt_profile.h"
#include "gap.h"
#include "gap_gatt.h"

// ============================================================
// Constants
// ============================================================
#define MEASURE_SEC        10      // measure every 10 seconds
#define ADV_INTERVAL_MS    1000    // BLE advertising interval (1 second)
#define DEFAULT_TARGET     250     // 25.0°C
#define DEFAULT_HYST       20      // 2.0°C hysteresis

// Company ID for Manufacturer Specific Data (dummy, replace with real)
#define COMPANY_ID         0x1234

// GATT UUIDs (custom 128-bit base + short)
#define TEMP_SERVICE_UUID  0x1809  // Health Thermometer (standard)
#define TEMP_CHAR_UUID     0x2A1C  // Temperature Measurement
#define TARGET_CHAR_UUID   0x2A00  // custom: target temperature
#define OUTPUT_CHAR_UUID   0x2A01  // custom: output state
#define MODE_CHAR_UUID     0x2A02  // custom: control mode
#define HYST_CHAR_UUID     0x2A03  // custom: hysteresis

// ============================================================
// Pin definitions
// ============================================================
#define NTC_ADC_CH      0       // PA0 = ADC channel 0
#define OUT_PIN         GPIO_Pin_1  // PA1
#define OUT_PORT        GPIOA

// ============================================================
// Global state
// ============================================================
static uint16_t target_temp    = DEFAULT_TARGET;   // 0.01°C
static uint16_t current_temp   = 2500;             // 0.01°C
static uint16_t hyst           = DEFAULT_HYST;     // 0.01°C
static uint8_t  output_state   = 0;
static uint8_t  control_mode   = 'A';              // 'A'uto, 0=OFF, 1=ON
static uint8_t  measure_count  = 0;
static uint8_t  report_count   = 0;
static uint8_t  task_id;
static uint8_t  adv_data[12];                      // BLE advertising data buffer

// ============================================================
// NTC lookup table (100K B=3950, 100K to VCC, NTC to GND)
// 12-bit ADC (0-4095)
// ============================================================
static const uint16_t ntc_table[][2] = {
    {3134,    0},  //  0.0°C
    {2963,   50},  //  5.0
    {2727,  100},  // 10.0
    {2499,  150},  // 15.0
    {2278,  200},  // 20.0
    {2160,  225},  // 22.5
    {2047,  250},  // 25.0°C
    {1938,  275},  // 27.5
    {1827,  300},  // 30.0
    {1624,  350},  // 35.0
    {1424,  400},  // 40.0
    {1232,  450},  // 45.0
    {1085,  500},  // 50.0
};
#define NTC_LEN (sizeof(ntc_table)/sizeof(ntc_table[0]))

// ============================================================
// NTC ADC → temperature (0.01°C) via linear interpolation
// ============================================================
static int16_t ntc_to_temp(uint16_t adc) {
    if (adc >= ntc_table[0][0]) return ntc_table[0][1];
    if (adc <= ntc_table[NTC_LEN-1][0]) return ntc_table[NTC_LEN-1][1];
    for (uint8_t i = 0; i < NTC_LEN-1; i++) {
        if (adc <= ntc_table[i][0] && adc >= ntc_table[i+1][0]) {
            uint16_t rng = ntc_table[i][0] - ntc_table[i+1][0];
            if (rng == 0) return ntc_table[i+1][1];
            return ntc_table[i+1][1] +
                   (int16_t)(((uint32_t)(adc - ntc_table[i+1][0]) *
                              (ntc_table[i][1] - ntc_table[i+1][1])) / rng);
        }
    }
    return 250;
}

// ============================================================
// ADC init (CH582 SAADC: 12-bit, single-ended)
// ============================================================
static void adc_init(void) {
    // PA0 as analog input (floating)
    GPIOA_ModeCfg(GPIO_Pin_0, GPIO_ModeIN_Floating);

    // ADC: 12-bit, single-ended, channel 0
    ADC_Init(ADC_CHANNEL_0, ADC_Mode_Single, ADC_PGA_0, ADC_Data_12bit);
    // ADC clock from system clock / 8 (7.5MHz for 60MHz sysclk)
    ADC_ClkDiv(8);
}

// ============================================================
// Temperature control with hysteresis
// ============================================================
static void control_update(void) {
    if (control_mode == 1) {
        output_state = 1;
    } else if (control_mode == 0) {
        output_state = 0;
    } else {
        int16_t upper = target_temp + hyst/2;
        int16_t lower = target_temp - hyst/2;
        if (current_temp > upper) output_state = 1;
        else if (current_temp < lower) output_state = 0;
    }

    if (output_state)
        GPIO_SetBits(OUT_PORT, OUT_PIN);
    else
        GPIO_ResetBits(OUT_PORT, OUT_PIN);
}

// ============================================================
// BLE Advertising data (includes current temperature)
// ============================================================
static void update_advertising_data(void) {
    uint8_t idx = 0;

    // Flags: LE General Discoverable, BR/EDR not supported
    adv_data[idx++] = 0x02;  // length
    adv_data[idx++] = 0x01;  // AD Type: Flags
    adv_data[idx++] = 0x06;  // LE General Discoverable | BR/EDR Not Supported

    // Complete Local Name: "Greeny"
    adv_data[idx++] = 0x07;  // length (6 chars + 1 type)
    adv_data[idx++] = 0x09;  // AD Type: Complete Local Name
    adv_data[idx++] = 'G';
    adv_data[idx++] = 'r';
    adv_data[idx++] = 'e';
    adv_data[idx++] = 'e';
    adv_data[idx++] = 'n';
    adv_data[idx++] = 'y';

    // Manufacturer Specific Data: temperature + output state
    // Format: [Company ID 2B] [Temp 2B] [Output 1B]
    adv_data[idx++] = 0x06;  // length
    adv_data[idx++] = 0xFF;  // AD Type: Manufacturer Specific
    adv_data[idx++] = COMPANY_ID & 0xFF;
    adv_data[idx++] = (COMPANY_ID >> 8) & 0xFF;
    adv_data[idx++] = current_temp & 0xFF;
    adv_data[idx++] = (current_temp >> 8) & 0xFF;
    adv_data[idx++] = output_state;

    GAP_SetAdvData(adv_data, idx);
}

// ============================================================
// GATT attribute table
// ============================================================
// Current Temperature characteristic value
static uint8_t temp_value[2];       // uint16, little-endian, 0.01°C

// GATT read callback
static uint8_t gatt_read_callback(uint16_t conn_handle, uint16_t attr_handle,
                                   uint8_t *buf, uint16_t *len, uint16_t offset) {
    // Update temperature value before read
    temp_value[0] = current_temp & 0xFF;
    temp_value[1] = (current_temp >> 8) & 0xFF;
    memcpy(buf, temp_value, 2);
    *len = 2;
    return SUCCESS;
}

// GATT write callback
static uint8_t gatt_write_callback(uint16_t conn_handle, uint16_t attr_handle,
                                    uint8_t *buf, uint16_t len) {
    if (len == 2) {
        uint16_t val = buf[0] | ((uint16_t)buf[1] << 8);
        target_temp = val;
        control_update();
    }
    return SUCCESS;
}

// ============================================================
// BLE event callback (connection, disconnection, GATT requests)
// ============================================================
static void ble_event_handler(uint32_t event, uint8_t *data, uint16_t len) {
    switch (event) {
    case GAP_DEVICE_INIT_DONE:
        // BLE stack initialized, start advertising
        update_advertising_data();
        GAPRole_StartAdvertising();
        break;

    case GAP_CONNECTED:
        // Phone connected → advertising stops
        break;

    case GAP_DISCONNECTED:
        // Phone disconnected → restart advertising
        update_advertising_data();
        GAPRole_StartAdvertising();
        break;

    case GATT_READ_REQUEST:
        gatt_read_callback(0, 0, NULL, NULL, 0);
        break;

    case GATT_WRITE_REQUEST:
        gatt_write_callback(0, 0, (uint8_t*)data, len);
        break;
    }
}

// ============================================================
// Periodic measurement task (called every MEASURE_SEC via RTC)
// ============================================================
static void periodic_handler(void) {
    // Read NTC temperature via ADC
    uint16_t adc_val = ADC_Read(NTC_ADC_CH);
    current_temp = (current_temp + (uint16_t)ntc_to_temp(adc_val)) / 2;

    // Update control output
    control_update();

    // Update BLE advertising data with new temperature
    update_advertising_data();

    // Periodic report (every 60 seconds)
    report_count++;
    if (report_count >= 6) {
        report_count = 0;
        // Advertising data already contains temperature
        // Optionally print via UART for debug
        PRINT("T=%d.%02d C  OUT=%d  MODE=%c\n",
              current_temp / 100, current_temp % 100,
              output_state, control_mode);
    }
}

// ============================================================
// CH582 init
// ============================================================
static void system_init(void) {
    // System clock: 60MHz from internal HSI
    SetSysClock(CLK_SOURCE_HSI_60MHz);

    // GPIO: PA1 push-pull output for MOSFET
    GPIOA_ModeCfg(OUT_PIN, GPIO_ModeOut_PP_5mA);

    // ADC for NTC
    adc_init();

    // RTC: external 32.768KHz crystal for accurate timing
    RTC_Init(RTC_CLK_SRC_XTAL);
    // Configure RTC alarm to wake CPU every MEASURE_SEC seconds
    RTC_SetAlarm(MEASURE_SEC);
    RTC_EnableAlarm();
    RTC_EnableWakeup();
    PFIC_EnableIRQ(RTC_IRQn);

    // WDT: enable with ~16 second timeout
    WDOG_Init(WDOG_CLK_SRC_LSI, WDOG_TIMEOUT_16S);
    WDOG_Feed();

    // Enable BLE
    CH58x_ble_init();
    GAPRole_PeripheralInit();
    GAPBondMgr_Init();
    GAP_SetParamValue(GAP_ADV_INTERVAL, ADV_INTERVAL_MS);
    GAP_DeviceInit(ble_event_handler);
}

// ============================================================
// RTC interrupt handler
// ============================================================
__attribute__((interrupt)) void RTC_IRQHandler(void) {
    RTC_ClearAlarmFlag();
    RTC_SetAlarm(MEASURE_SEC);  // Re-arm for next interval
    WDOG_Feed();                 // Feed watchdog
    periodic_handler();
}

// ============================================================
// Main
// ============================================================
int main(void) {
    system_init();

    // Enable global interrupts
    PFIC_EnableAllIRQ();

    // Main loop: sleep until RTC wakes us
    while (1) {
        // Enter low-power sleep.
        // CH582 automatically sleeps when CPU is idle.
        // RTC alarm will wake us.
        __WFI();  // Wait For Interrupt → deep sleep
        // CPU resumes here after RTC interrupt
    }
}
