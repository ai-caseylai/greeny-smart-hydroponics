/**
 * Kids Activity Tracker — CH582M + ICM-42670-P 6-axis IMU
 *
 * Wristband for children's physical activity monitoring.
 * Tracks steps & activity intensity. Stores per-minute records in flash.
 * BLE sync to parent's phone. 1-2 week battery on 150mAh LiPo.
 *
 * Hardware:
 *   CH582M QFN28     — RISC-V BLE 5.0 MCU
 *   ICM-42670-P      — 6-axis IMU (I2C addr 0x68)
 *   150mAh LiPo      — USB-C charging via TP4056
 *   WS2812 RGB LED   — status indicator
 *   Tactile button   — sync trigger
 *
 * I2C pins:  PA2=SDA, PA3=SCL
 * LED pin:   PA4
 * Button:    PA5 (active low, internal pull-up)
 *
 * Storage:  ~280KB ring buffer in CH582 internal flash
 *           = 35,000 records × 8 bytes = ~24 days at 1 record/min
 *
 * BLE GATT Service:
 *   Activity Service (UUID 0x1800-based custom)
 *     Device Info:    battery %, firmware version
 *     Live Data:      accel XYZ, steps, activity level (Notify, 1Hz)
 *     Sync Control:   start/stop/clear sync (Write)
 *     Sync Data:      stream stored records (Notify)
 */

#include "CH58x_common.h"
#include "CH58xBLE_LIB.h"
#include "gap.h"

// ============================================================
// Constants
// ============================================================
#define IMU_SAMPLE_HZ     25      // IMU read rate (Hz)
#define STEP_MIN_INTERVAL 200     // Min ms between steps (kids: up to 300/min)
#define RECORD_INTERVAL   60      // Save record every 60 seconds
#define FLASH_RECORD_MAX  35000   // Max records in flash ring buffer
#define BATTERY_MV_MAX    4200    // LiPo full charge
#define BATTERY_MV_MIN    3200    // LiPo cutoff

// Flash storage layout (CH582 internal flash, 448KB total)
// Code+BLE stack: 0x00000000 - 0x00028000 (160KB)
// Activity storage: 0x00028000 - 0x00070000 (320KB reserved, ~280KB usable)
#define FLASH_STORAGE_BASE  0x00028000
#define FLASH_SECTOR_SIZE   4096    // 4KB per sector
#define FLASH_PAGE_SIZE     256     // 256B per page (CH582 min write unit)
#define RECORD_SIZE         8       // bytes per activity record

// Activity intensity levels
#define INTENSITY_LOW    0
#define INTENSITY_MEDIUM 1
#define INTENSITY_HIGH   2

// ============================================================
// IMU (ICM-42670-P) I2C driver
// ============================================================
#define IMU_ADDR    0x68    // 7-bit I2C address (AD0=GND)
#define IMU_WHOAMI  0x75    // WHO_AM_I register → expect 0x67

// ICM-42670-P register map
#define REG_DEVICE_CONFIG  0x11
#define REG_PWR_MGMT0      0x4E
#define REG_ACCEL_CONFIG0  0x50
#define REG_GYRO_CONFIG0   0x4F
#define REG_ACCEL_DATA_X1  0x1F  // Accel X[15:8]
#define REG_GYRO_DATA_X1   0x25  // Gyro X[15:8]
#define REG_INT_CONFIG     0x14
#define REG_FIFO_CONFIG    0x16
#define REG_INT_STATUS     0x2D
#define REG_TEMP_DATA1     0x1D

// ---- I2C helpers (CH582 HAL) ----
static void imu_write_reg(uint8_t reg, uint8_t val) {
    I2C_WriteReg(IMU_ADDR, reg, &val, 1);
}

static uint8_t imu_read_reg(uint8_t reg) {
    uint8_t val;
    I2C_ReadReg(IMU_ADDR, reg, &val, 1);
    return val;
}

static void imu_read_burst(uint8_t reg, uint8_t *buf, uint8_t len) {
    I2C_ReadReg(IMU_ADDR, reg, buf, len);
}

// ---- IMU init ----
static bool imu_init(void) {
    // I2C init: PA2=SDA, PA3=SCL, 400kHz
    GPIOA_ModeCfg(GPIO_Pin_2|GPIO_Pin_3, GPIO_ModeIN_PU);
    I2C_Init(I2C_Mode_I2C, 400000, I2C_Ack_Enable);

    // Verify IMU
    uint8_t who = imu_read_reg(IMU_WHOAMI);
    if (who != 0x67) {
        PRINT("IMU not found (WHOAMI=0x%02X, expect 0x67)\n", who);
        return false;
    }

    // Soft reset
    imu_write_reg(REG_DEVICE_CONFIG, 0x01);
    for (volatile uint32_t i = 0; i < 10000; i++);  // ~10ms

    // Power management: turn on accel + gyro in low-noise mode
    imu_write_reg(REG_PWR_MGMT0, 0x0F);  // Accel LN + Gyro LN + Temp on

    // Accel config: ±4g, 25Hz ODR, low-power averaging
    imu_write_reg(REG_ACCEL_CONFIG0, (0x04 << 5) | 0x04);  // ±4g, ODR=25Hz

    // Gyro config: ±500dps, 25Hz ODR
    imu_write_reg(REG_GYRO_CONFIG0, (0x02 << 5) | 0x04);  // ±500dps, ODR=25Hz

    // Interrupt: data ready on INT1
    imu_write_reg(REG_INT_CONFIG, 0x01);  // INT1 = data ready

    // FIFO: disabled (we poll at 25Hz)
    imu_write_reg(REG_FIFO_CONFIG, 0x00);

    return true;
}

// ---- Read 6-axis data ----
// Returns accel in mg, gyro in mdps (milli-degrees/sec)
// accel[3]: X,Y,Z in milli-g
// gyro[3]:  X,Y,Z in milli-dps
static void imu_read_6axis(int16_t accel[3], int16_t gyro[3]) {
    uint8_t buf[12];
    imu_read_burst(REG_ACCEL_DATA_X1, buf, 12);

    // Accel: 16-bit signed, ±4g → 1 LSB = 0.122mg → convert to mg
    // buf[0:1]=AX, buf[2:3]=AY, buf[4:5]=AZ
    for (int i = 0; i < 3; i++) {
        int16_t raw = (int16_t)((buf[i*2] << 8) | buf[i*2+1]);
        accel[i] = (int16_t)(((int32_t)raw * 1000) / 8192);  // mg
    }
    // Gyro: 16-bit signed, ±500dps → 1 LSB = 0.0153dps → convert to mdps
    // buf[6:7]=GX, buf[8:9]=GY, buf[10:11]=GZ
    for (int i = 0; i < 3; i++) {
        int16_t raw = (int16_t)((buf[6+i*2] << 8) | buf[7+i*2]);
        gyro[i] = (int16_t)(((int32_t)raw * 1000) / 6554);  // mdps
    }
}

// ============================================================
// Step Detection Algorithm
// ============================================================
// Simple peak detection on accelerometer magnitude.
// Dynamic threshold adapts to user's movement pattern.

static int16_t accel_mag_history[8];  // 250ms sliding window @ 25Hz
static uint8_t mag_idx;
static int32_t mag_sum;
static int16_t step_threshold = 1200;  // Dynamic threshold (mg above 1G)
static uint32_t last_step_ms;
static uint32_t step_count;
static uint8_t  activity_seconds;  // Active seconds in current minute

// ---- Running average filter ----
static int16_t running_add(int16_t val) {
    mag_sum -= accel_mag_history[mag_idx];
    mag_sum += val;
    accel_mag_history[mag_idx] = val;
    mag_idx = (mag_idx + 1) & 0x07;
    return (int16_t)(mag_sum / 8);
}

// ---- Step detection callback (called at IMU sample rate) ----
static void step_detect(int16_t ax, int16_t ay, int16_t az, uint32_t now_ms) {
    // Magnitude of acceleration vector (in mg)
    int32_t mag_sq = (int32_t)ax*ax + (int32_t)ay*ay + (int32_t)az*az;
    int16_t mag = (int16_t)(mag_sq >> 10);  // Approx sqrt in mg (rough)

    // Low-pass filter
    int16_t mag_filt = running_add(mag);

    // Remove gravity: mag_filt = |mag - 1000mg|
    int16_t mag_ac = mag_filt - 1000;
    if (mag_ac < 0) mag_ac = -mag_ac;
    if (mag_ac < 200) mag_ac = 200;  // Noise floor

    // Track activity
    if (mag_ac > 800) {
        activity_seconds++;  // Will be normalized later
    }

    // Step detection: positive-going zero crossing above threshold
    static int16_t prev_mag;
    static bool    was_low;

    if (mag_ac > step_threshold && was_low) {
        // Potential step: check timing
        if (now_ms - last_step_ms > STEP_MIN_INTERVAL) {
            step_count++;
            last_step_ms = now_ms;

            // Adaptive threshold: track peak magnitude
            if (mag_ac > step_threshold * 2)
                step_threshold += 50;   // Increase (running)
            else if (step_threshold > 800 && mag_ac < step_threshold * 3/2)
                step_threshold -= 20;   // Decrease (walking/rest)
        }
    }

    was_low = (mag_ac < step_threshold * 3/4);
    prev_mag = mag_ac;
}

// ============================================================
// Activity Record (8 bytes per minute)
// ============================================================
typedef struct __attribute__((packed)) {
    uint32_t timestamp;   // Unix epoch (minutes)
    uint16_t steps;       // Steps this minute
    uint8_t  active_sec;  // Seconds of activity (>threshold)
    uint8_t  intensity;   // 0=low, 1=medium, 2=high
} activity_record_t;

// ============================================================
// Flash Storage (ring buffer)
// ============================================================
static uint32_t flash_write_offset;   // Current write position
static uint32_t flash_record_count;   // Total records written

static void storage_init(void) {
    // Scan flash to find last written record
    flash_write_offset = FLASH_STORAGE_BASE;
    flash_record_count = 0;

    for (uint32_t addr = FLASH_STORAGE_BASE;
         addr < FLASH_STORAGE_BASE + FLASH_SECTOR_SIZE * 70;
         addr += RECORD_SIZE) {
        // Check if this slot has data (>0xFF means programmed)
        uint8_t *p = (uint8_t *)addr;
        if (p[0] == 0xFF && p[1] == 0xFF) {
            flash_write_offset = addr;
            break;
        }
        flash_record_count++;
    }

    // If full, wrap to beginning
    if (flash_record_count >= FLASH_RECORD_MAX) {
        flash_write_offset = FLASH_STORAGE_BASE;
        flash_record_count = 0;
        // Erase first sector
        FLASH_EraseSector(FLASH_STORAGE_BASE);
    }
}

static void storage_write(activity_record_t *rec) {
    // Write 8-byte record to flash
    uint8_t buf[RECORD_SIZE];
    buf[0] = (rec->timestamp >> 0)  & 0xFF;
    buf[1] = (rec->timestamp >> 8)  & 0xFF;
    buf[2] = (rec->timestamp >> 16) & 0xFF;
    buf[3] = (rec->timestamp >> 24) & 0xFF;
    buf[4] = rec->steps & 0xFF;
    buf[5] = (rec->steps >> 8) & 0xFF;
    buf[6] = rec->active_sec;
    buf[7] = rec->intensity;

    FLASH_Write(flash_write_offset, buf, RECORD_SIZE);
    flash_write_offset += RECORD_SIZE;
    flash_record_count++;

    // Handle sector boundary and wrap
    if (flash_write_offset >= FLASH_STORAGE_BASE + FLASH_SECTOR_SIZE * 70) {
        flash_write_offset = FLASH_STORAGE_BASE;
        flash_record_count = 0;
        FLASH_EraseSector(FLASH_STORAGE_BASE);
    }
}

static uint32_t storage_read(uint32_t index, activity_record_t *rec) {
    if (index >= flash_record_count) return 0;

    uint32_t addr;
    if (flash_record_count >= FLASH_RECORD_MAX) {
        addr = flash_write_offset + index * RECORD_SIZE;
        if (addr >= FLASH_STORAGE_BASE + FLASH_SECTOR_SIZE * 70)
            addr -= FLASH_SECTOR_SIZE * 70;
    } else {
        addr = FLASH_STORAGE_BASE + index * RECORD_SIZE;
    }

    uint8_t *p = (uint8_t *)addr;
    rec->timestamp   = p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    rec->steps       = p[4] | ((uint16_t)p[5] << 8);
    rec->active_sec  = p[6];
    rec->intensity   = p[7];
    return 1;
}

// ============================================================
// BLE GATT Service
// ============================================================
static uint8_t  ble_connected;
static uint8_t  sync_active;
static uint32_t sync_index;
static uint8_t  live_notify_enabled;

// Battery voltage via ADC internal channel
static uint8_t battery_percent(void) {
    uint16_t vcc = ADC_ReadInternalVCC();  // mV
    if (vcc > BATTERY_MV_MAX) vcc = BATTERY_MV_MAX;
    if (vcc < BATTERY_MV_MIN) vcc = BATTERY_MV_MIN;
    return (uint8_t)((uint32_t)(vcc - BATTERY_MV_MIN) * 100 /
                     (BATTERY_MV_MAX - BATTERY_MV_MIN));
}

// ---- Advertising data ----
static uint8_t adv_data[] = {
    0x02, 0x01, 0x06,                          // Flags
    0x08, 0x09, 'K','i','d','s','A','c','t',   // Name: "KidsAct"
};

// ---- BLE event handler ----
static void ble_handler(uint32_t event, uint8_t *data, uint16_t len) {
    switch (event) {
    case GAP_DEVICE_INIT_DONE:
        GAP_SetAdvData(adv_data, sizeof(adv_data));
        GAPRole_StartAdvertising();
        break;
    case GAP_CONNECTED:
        ble_connected = 1;
        break;
    case GAP_DISCONNECTED:
        ble_connected = 0;
        sync_active = 0;
        GAPRole_StartAdvertising();
        break;
    case GATT_WRITE_REQUEST:
        // Handle sync control commands
        if (len >= 1) {
            switch (data[0]) {
            case 0x01:  // Start sync
                sync_active = 1;
                sync_index = 0;
                break;
            case 0x02:  // Stop sync
                sync_active = 0;
                break;
            case 0x03:  // Clear storage
                FLASH_EraseSector(FLASH_STORAGE_BASE);
                flash_write_offset = FLASH_STORAGE_BASE;
                flash_record_count = 0;
                break;
            case 0x10:  // Enable live data notify
                live_notify_enabled = 1;
                break;
            case 0x11:  // Disable live data notify
                live_notify_enabled = 0;
                break;
            }
        }
        break;
    }
}

// ============================================================
// LED control (WS2812 / simple GPIO)
// ============================================================
#define LED_PIN  GPIO_Pin_4
#define LED_PORT GPIOA

static void led_init(void) {
    GPIOA_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);
    GPIOA_ResetBits(LED_PIN);
}

static void led_set(uint8_t on) {
    if (on) GPIOA_SetBits(LED_PORT, LED_PIN);
    else    GPIOA_ResetBits(LED_PORT, LED_PIN);
}

// Blink pattern: count × 100ms on, 200ms off
static void led_blink(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        led_set(1); for (volatile uint32_t j = 0; j < 600000; j++);
        led_set(0); for (volatile uint32_t j = 0; j < 1200000; j++);
    }
}

// ============================================================
// Button
// ============================================================
#define BTN_PIN  GPIO_Pin_5
#define BTN_PORT GPIOA

static void button_init(void) {
    GPIOA_ModeCfg(BTN_PIN, GPIO_ModeIN_PU);  // Pull-up, active low
}

static bool button_pressed(void) {
    return !GPIOA_ReadPortPin(BTN_PIN);
}

// ============================================================
// System init
// ============================================================
static void system_init(void) {
    // Clock: 60MHz HSI
    SetSysClock(CLK_SOURCE_HSI_60MHz);

    // GPIO
    led_init();
    button_init();

    // I2C for IMU
    GPIOA_ModeCfg(GPIO_Pin_2|GPIO_Pin_3, GPIO_ModeIN_PU);

    // ADC for battery
    ADC_Init(ADC_CHANNEL_VCC, ADC_Mode_Single, ADC_PGA_0, ADC_Data_12bit);

    // RTC for timestamping (external 32.768KHz)
    RTC_Init(RTC_CLK_SRC_XTAL);

    // WDT
    WDOG_Init(WDOG_CLK_SRC_LSI, WDOG_TIMEOUT_16S);

    // BLE init
    CH58x_ble_init();
    GAPRole_PeripheralInit();
    GAPBondMgr_Init();
    GAP_DeviceInit(ble_handler);
    GAP_SetParamValue(GAP_ADV_INTERVAL, 320);  // 200ms advertising

    // IMU init
    if (!imu_init()) {
        // Blink LED 3 times = IMU error
        led_blink(3);
    }
}

// ============================================================
// Convert RTC to Unix-ish timestamp (seconds since boot)
// ============================================================
static uint32_t rtc_seconds(void) {
    return RTC_GetCounter();  // CH582 RTC counter in seconds
}

// ============================================================
// Main
// ============================================================
int main(void) {
    system_init();
    storage_init();
    PFIC_EnableAllIRQ();

    uint32_t last_imu_read = 0;
    uint32_t last_record_save = 0;
    uint32_t last_ble_notify = 0;
    uint32_t now;

    uint32_t minute_steps = 0;
    uint8_t  minute_active_sec = 0;
    uint8_t  minute_intensity = 0;

    // LED: boot OK
    led_blink(1);

    while (1) {
        now = rtc_seconds();

        // ---- IMU reading & step detection ----
        if (now - last_imu_read >= 1) {  // 1 second tick (or use timer)
            last_imu_read = now;

            int16_t accel[3], gyro[3];
            imu_read_6axis(accel, gyro);
            step_detect(accel[0], accel[1], accel[2], now * 1000);

            // Accumulate minute stats
            minute_steps = (uint16_t)step_count;
            minute_active_sec = activity_seconds;
            // Classify intensity
            if (minute_active_sec > 40)
                minute_intensity = INTENSITY_HIGH;
            else if (minute_active_sec > 15)
                minute_intensity = INTENSITY_MEDIUM;
            else
                minute_intensity = INTENSITY_LOW;

            // ---- BLE live data notify ----
            if (ble_connected && live_notify_enabled &&
                now - last_ble_notify >= 1) {
                last_ble_notify = now;
                // Send live data via BLE notify
                uint8_t live[16];
                live[0] = (accel[0] >> 8) & 0xFF; live[1] = accel[0] & 0xFF;
                live[2] = (accel[1] >> 8) & 0xFF; live[3] = accel[1] & 0xFF;
                live[4] = (accel[2] >> 8) & 0xFF; live[5] = accel[2] & 0xFF;
                live[6] = (gyro[0] >> 8) & 0xFF;  live[7] = gyro[0] & 0xFF;
                live[8] = (gyro[1] >> 8) & 0xFF;  live[9] = gyro[1] & 0xFF;
                live[10]= (gyro[2] >> 8) & 0xFF;  live[11]= gyro[2] & 0xFF;
                live[12]= (step_count >> 0) & 0xFF;
                live[13]= (step_count >> 8) & 0xFF;
                live[14]= (step_count >> 16) & 0xFF;
                live[15]= (step_count >> 24) & 0xFF;
                GATT_Notify(0, live, 16);  // Send via BLE
            }
        }

        // ---- Save minute record to flash ----
        if (now - last_record_save >= RECORD_INTERVAL) {
            last_record_save = now;

            activity_record_t rec;
            rec.timestamp   = now / 60;  // Minutes since boot
            rec.steps       = minute_steps;
            rec.active_sec  = minute_active_sec;
            rec.intensity   = minute_intensity;
            storage_write(&rec);

            // Reset minute counters
            step_count -= minute_steps;  // Keep running total
            activity_seconds = 0;

            // Brief LED flash on record save
            led_set(1);
            for (volatile uint32_t i = 0; i < 60000; i++);
            led_set(0);
        }

        // ---- BLE sync: stream stored records ----
        if (ble_connected && sync_active) {
            activity_record_t rec;
            if (storage_read(sync_index, &rec)) {
                uint8_t buf[RECORD_SIZE];
                buf[0] = (rec.timestamp >> 0)  & 0xFF;
                buf[1] = (rec.timestamp >> 8)  & 0xFF;
                buf[2] = (rec.timestamp >> 16) & 0xFF;
                buf[3] = (rec.timestamp >> 24) & 0xFF;
                buf[4] = rec.steps & 0xFF;
                buf[5] = (rec.steps >> 8) & 0xFF;
                buf[6] = rec.active_sec;
                buf[7] = rec.intensity;
                GATT_Notify(0, buf, RECORD_SIZE);
                sync_index++;
            } else {
                // All records sent → send end marker
                uint8_t end_marker[] = {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
                GATT_Notify(0, end_marker, RECORD_SIZE);
                sync_active = 0;
            }
        }

        // ---- Button: toggle sync mode ----
        if (button_pressed()) {
            if (ble_connected) {
                sync_active = !sync_active;
                sync_index = 0;
                // Debounce
                for (volatile uint32_t i = 0; i < 6000000; i++);
            }
        }

        // ---- Sleep until next loop ----
        __WFI();  // Woken by RTC tick
        WDOG_Feed();
    }
}
