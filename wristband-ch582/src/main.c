/**
 * Kids Activity Tracker — CH582M + ICM-42670-P 6-axis IMU
 *
 * Wristband for children's physical activity monitoring.
 * Tracks steps & activity intensity. Stores per-minute records in flash.
 * BLE sync to parent's phone.
 *
 * Power: IMU Wake-on-Motion (WOM) + inactivity detection.
 *   SLEEP:  IMU WOM mode (~20μA) + MCU deep sleep (~1μA) = 21μA
 *   ACTIVE: IMU 25Hz full (~0.7mA) + MCU recording (~4mA) = 4.7mA
 *   Child active ~2h/day: average ~350μA → 150mAh LiPo = ~18 days
 *
 * Hardware:
 *   CH582M QFN28     — RISC-V BLE 5.0 MCU
 *   ICM-42670-P      — 6-axis IMU (I2C addr 0x68)
 *   150mAh LiPo      — USB-C charging via TP4056
 *   LED + Button     — status indicator + sync trigger
 *
 * I2C pins:  PA2=SDA, PA3=SCL
 * IMU INT1:  PA1 (GPIO interrupt, wakes MCU from deep sleep)
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
#define IMU_SAMPLE_HZ     25      // IMU read rate (Hz) in ACTIVE mode
#define IMU_WOM_ODR_HZ    12      // IMU ODR in WOM sleep mode
#define STEP_MIN_INTERVAL 200     // Min ms between steps (kids: up to 300/min)
#define RECORD_INTERVAL   60      // Save record every 60 seconds
#define FLASH_RECORD_MAX  35000   // Max records in flash ring buffer
#define BATTERY_MV_MAX    4200    // LiPo full charge
#define BATTERY_MV_MIN    3200    // LiPo cutoff
#define INACTIVE_MINUTES  5       // Minutes without steps → return to SLEEP
#define WOM_THRESHOLD_MG  150     // WOM wake threshold in mg (medium sensitivity)

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
#define REG_PWR_MGMT0_ACCEL_LP  0x02  // Accel Low-Power mode
#define REG_PWR_MGMT0_ACCEL_LN  0x03  // Accel Low-Noise mode
#define REG_ACCEL_CONFIG0  0x50
#define REG_GYRO_CONFIG0   0x4F
#define REG_ACCEL_CONFIG1  0x52  // Accel LP averaging
#define REG_GYRO_CONFIG0_STANDBY 0x00  // Gyro off
#define REG_ACCEL_DATA_X1  0x1F  // Accel X[15:8]
#define REG_GYRO_DATA_X1   0x25  // Gyro X[15:8]
#define REG_INT_CONFIG     0x14
#define REG_INT_SOURCE0    0x63  // Interrupt source routing
#define REG_WOM_CONFIG     0x57  // WOM threshold
#define REG_SMD_CONFIG     0x58  // Significant Motion Detect
#define REG_FIFO_CONFIG    0x16
#define REG_INT_STATUS     0x2D
#define REG_INT_STATUS_WOM 0x80  // WOM interrupt flag
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

    // IMU INT1 pin: PA1 as input with interrupt (wakes MCU from deep sleep)
    GPIOA_ModeCfg(GPIO_Pin_1, GPIO_ModeIN_PU);
    GPIOA_ITModeCfg(GPIO_Pin_1, GPIO_IT_Rise);   // Rising edge = WOM detected
    PFIC_EnableIRQ(GPIOA_IRQn);                    // Enable GPIOA interrupt

    // Verify IMU
    uint8_t who = imu_read_reg(IMU_WHOAMI);
    if (who != 0x67) {
        PRINT("IMU not found (WHOAMI=0x%02X, expect 0x67)\n", who);
        return false;
    }

    // Soft reset
    imu_write_reg(REG_DEVICE_CONFIG, 0x01);
    for (volatile uint32_t i = 0; i < 10000; i++);  // ~10ms

    // Start in WOM sleep mode (ultra low power)
    imu_enter_wom_mode();

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
// Device state machine
// ============================================================
typedef enum {
    STATE_SLEEP  = 0,  // IMU WOM + MCU deep sleep (21μA)
    STATE_ACTIVE = 1,  // IMU full + MCU recording (4.7mA)
} device_state_t;

static device_state_t device_state = STATE_SLEEP;
static uint32_t inactive_minutes;    // consecutive minutes with no steps
static bool     wom_triggered;       // set by GPIO interrupt

// ---- Enter WOM sleep mode ----
static void imu_enter_wom_mode(void) {
    // Stop gyro (saves ~0.5mA)
    imu_write_reg(REG_PWR_MGMT0, REG_PWR_MGMT0_ACCEL_LP);  // Accel LP mode, gyro off

    // Accel: low ODR for WOM
    // ODR = 12.5Hz, ±4g, LP averaging
    uint8_t odr_lp = 0x03;  // 12.5Hz in LP mode
    imu_write_reg(REG_ACCEL_CONFIG0, (0x04 << 5) | odr_lp);  // ±4g, LP ODR
    imu_write_reg(REG_ACCEL_CONFIG1, 0x00);  // 1x averaging (lowest power)

    // WOM threshold: WOM_THRESHOLD_MG / (1000/8192) ≈ WOM_THRESHOLD_MG * 8 / 1000
    // For 150mg: 150 * 256 / 1000 ≈ 38 LSBs
    uint8_t wom_thr = (uint8_t)((WOM_THRESHOLD_MG * 256UL) / 1000);
    imu_write_reg(REG_WOM_CONFIG, wom_thr);  // WOM_X | WOM_Y | WOM_Z (bits 6-4)

    // Route WOM interrupt to INT1
    imu_write_reg(REG_INT_SOURCE0, 0x80);  // WOM interrupt → INT1

    // INT1: push-pull, active high, cleared by status read
    imu_write_reg(REG_INT_CONFIG, 0x02);  // INT1 push-pull pulsed

    device_state = STATE_SLEEP;
}

// ---- Enter full active mode ----
static void imu_enter_active_mode(void) {
    // Accel + Gyro in low-noise mode
    imu_write_reg(REG_PWR_MGMT0, 0x0F);  // Accel LN + Gyro LN + Temp

    // Accel: ±4g, 25Hz ODR
    imu_write_reg(REG_ACCEL_CONFIG0, (0x04 << 5) | 0x04);

    // Gyro: ±500dps, 25Hz ODR
    imu_write_reg(REG_GYRO_CONFIG0, (0x02 << 5) | 0x04);

    // Disable WOM interrupt routing
    imu_write_reg(REG_INT_SOURCE0, 0x00);
    imu_write_reg(REG_INT_CONFIG, 0x00);  // INT1 disabled

    inactive_minutes = 0;
    wom_triggered = false;
    device_state = STATE_ACTIVE;
}

// ---- GPIO interrupt handler: IMU INT1 wakes MCU from SLEEP ----
// PA1 = IMU INT1, rising edge = WOM detected
__attribute__((interrupt)) void GPIOA_IRQHandler(void) {
    if (GPIOA_ReadITFlagBit(GPIO_Pin_1)) {
        GPIOA_ClearITFlagBit(GPIO_Pin_1);
        if (device_state == STATE_SLEEP) {
            wom_triggered = true;
        }
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
// WS2812 RGB LED (PA4, 1-wire bit-bang)
// ============================================================
#define WS2812_PIN  GPIO_Pin_4
#define WS2812_PORT GPIOA

// WS2812 timing @ 60MHz: 0=400nsH+850nsL, 1=800nsH+450nsL
// 1 cycle = 16.7ns. Use ~12 NOP cycles per bit decision.
static void ws2812_send_byte(uint8_t byte) {
    for (int8_t bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            // 1-bit: ~800ns high, 450ns low
            GPIOA_SetBits(WS2812_PIN);
            for (volatile uint8_t n = 0; n < 20; n++);  // ~800ns
            GPIOA_ResetBits(WS2812_PIN);
            for (volatile uint8_t n = 0; n < 10; n++);  // ~450ns
        } else {
            // 0-bit: ~400ns high, 850ns low
            GPIOA_SetBits(WS2812_PIN);
            for (volatile uint8_t n = 0; n < 8; n++);   // ~400ns
            GPIOA_ResetBits(WS2812_PIN);
            for (volatile uint8_t n = 0; n < 22; n++);  // ~850ns
        }
    }
}

static void ws2812_set(uint8_t r, uint8_t g, uint8_t b) {
    ws2812_send_byte(g);  // GRB order
    ws2812_send_byte(r);
    ws2812_send_byte(b);
    // Reset: >50μs low
    GPIOA_ResetBits(WS2812_PIN);
    for (volatile uint16_t n = 0; n < 3000; n++);
}

static void ws2812_init(void) {
    GPIOA_ModeCfg(WS2812_PIN, GPIO_ModeOut_PP_5mA);
    ws2812_set(0, 0, 0);
}

// ============================================================
// SSD1306 OLED 0.49" 64x32 (I²C 0x3C, shared bus with IMU)
// ============================================================
#define OLED_ADDR  0x3C
#define OLED_WIDTH 64
#define OLED_HEIGHT 32
#define OLED_PAGES (OLED_HEIGHT / 8)  // 4 pages

static bool oled_on;

static void oled_write_cmd(uint8_t cmd) {
    I2C_WriteReg(OLED_ADDR, 0x00, &cmd, 1);  // 0x00 = command mode
}

static void oled_write_data(uint8_t *buf, uint8_t len) {
    I2C_WriteReg(OLED_ADDR, 0x40, buf, len);  // 0x40 = data mode
}

static void oled_init(void) {
    // SSD1306 init sequence for 64x32
    uint8_t init[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Clock div
        0xA8, 0x1F, // Mux ratio = 31 (32 rows)
        0xD3, 0x00, // Display offset = 0
        0x40,       // Start line = 0
        0x8D, 0x14, // Charge pump ON
        0x20, 0x00, // Horizontal addressing
        0xA1,       // Segment remap
        0xC8,       // COM scan direction
        0xDA, 0x02, // COM pins
        0x81, 0x7F, // Contrast
        0xD9, 0xF1, // Pre-charge
        0xDB, 0x40, // VCOM detect
        0xA4,       // Display from RAM
        0xA6,       // Normal (non-inverted)
        0xAF,       // Display ON
    };
    for (uint8_t i = 0; i < sizeof(init); i++)
        oled_write_cmd(init[i]);
    oled_on = true;
}

static void oled_clear(void) {
    uint8_t zero[64] = {0};
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        oled_write_cmd(0xB0 + page);  // Set page
        oled_write_cmd(0x00);         // Column low
        oled_write_cmd(0x10);         // Column high
        oled_write_data(zero, OLED_WIDTH);
    }
}

// 8x16 digit font (numbers 0-9, :, space) — packed as 16 bytes each
static const uint8_t font_8x16[][16] = {
    // 0
    {0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C},
    // 1
    {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E},
    // 2
    {0x3C,0x7E,0x66,0x60,0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x06,0x66,0x66,0x7E,0x3C},
    // 3
    {0x3C,0x7E,0x66,0x60,0x60,0x38,0x38,0x60,0x60,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C},
    // 4
    {0x30,0x38,0x3C,0x36,0x33,0x33,0x7F,0x7F,0x30,0x30,0x30,0x30,0x30,0x30,0x78,0x78},
    // 5
    {0x7E,0x7E,0x06,0x06,0x3E,0x7E,0x66,0x60,0x60,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C},
    // 6
    {0x3C,0x7E,0x66,0x06,0x06,0x3E,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C},
    // 7
    {0x7E,0x7E,0x66,0x60,0x60,0x30,0x30,0x18,0x18,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C},
    // 8
    {0x3C,0x7E,0x66,0x66,0x66,0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C},
    // 9
    {0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x7E,0x3E,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C},
    // : colon
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    // % percent
    {0x06,0x0F,0x09,0x06,0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x33,0x61,0x71,0x3B,0x1E},
    // . dot
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    // space
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

static void oled_draw_char(uint8_t x, uint8_t page, uint8_t ch) {
    uint8_t idx;
    if (ch >= '0' && ch <= '9') idx = ch - '0';
    else if (ch == ':') idx = 10;
    else if (ch == '%') idx = 11;
    else if (ch == '.') idx = 12;
    else idx = 13;  // space

    oled_write_cmd(0xB0 + (page & 0x07));    // Set page
    oled_write_cmd(0x00 + (x & 0x0F));        // Column low
    oled_write_cmd(0x10 + ((x >> 4) & 0x0F)); // Column high

    uint8_t top[8], bot[8];
    for (uint8_t i = 0; i < 8; i++) {
        top[i] = font_8x16[idx][i];
        bot[i] = font_8x16[idx][i + 8];
    }
    oled_write_data(top, 8);
    oled_write_cmd(0xB0 + ((page + 1) & 0x07));
    oled_write_cmd(0x00 + (x & 0x0F));
    oled_write_cmd(0x10 + ((x >> 4) & 0x0F));
    oled_write_data(bot, 8);
}

// Draw big number string at position (x page offset)
static void oled_draw_num(uint8_t x, uint8_t page, uint32_t num) {
    char buf[10];
    uint8_t len = 0;
    if (num == 0) { buf[len++] = '0'; }
    else {
        uint32_t n = num;
        while (n) { n /= 10; len++; }
        for (int8_t i = len - 1; i >= 0; i--) {
            buf[i] = '0' + (num % 10);
            num /= 10;
        }
    }
    for (uint8_t i = 0; i < len; i++)
        oled_draw_char(x + i * 8, page, buf[i]);
}

// ---- OLED page: show steps ----
static void oled_show_steps(void) {
    oled_clear();
    oled_draw_num(8, 1, step_count > 99999 ? 99999 : step_count);
    // "steps" label on line 3
    uint8_t lbl[] = {'s','t','e','p','s'};
    for (uint8_t i = 0; i < 5; i++)
        oled_draw_char(12 + i * 8, 3, lbl[i]);
}

// ---- OLED page: show battery + time ----
static void oled_show_status(void) {
    oled_clear();
    uint8_t bat = battery_percent();
    oled_draw_num(0, 1, bat);
    oled_draw_char(16, 1, '%');
    uint32_t t = rtc_seconds();
    uint8_t h = (t / 3600) % 24;
    uint8_t m = (t / 60) % 60;
    oled_draw_num(0, 3, h);
    oled_draw_char(16, 3, ':');
    oled_draw_num(24, 3, m);
}

// ============================================================
// Button + Display handler
// ============================================================
#define BTN_PIN      GPIO_Pin_5
#define OLED_TIMEOUT 5   // seconds OLED stays on after button press

static void button_init(void) {
    GPIOA_ModeCfg(BTN_PIN, GPIO_ModeIN_PU);
}

static bool button_pressed(void) {
    return !GPIOA_ReadPortPin(BTN_PIN);
}

// Button press cycles display: OFF → Steps → Status → OFF
typedef enum { DISP_OFF, DISP_STEPS, DISP_STATUS } display_page_t;
static display_page_t display_page = DISP_OFF;
static uint32_t display_on_time;  // seconds when display was turned on

static void button_handle(void) {
    if (!button_pressed()) return;

    // Debounce
    for (volatile uint32_t i = 0; i < 600000; i++);

    switch (display_page) {
    case DISP_OFF:
        display_page = DISP_STEPS;
        oled_show_steps();
        display_on_time = rtc_seconds();
        // Green flash = activity reward
        ws2812_set(0, 32, 0);
        for (volatile uint32_t j = 0; j < 300000; j++);
        ws2812_set(0, 0, 0);
        break;
    case DISP_STEPS:
        display_page = DISP_STATUS;
        oled_show_status();
        display_on_time = rtc_seconds();
        break;
    case DISP_STATUS:
        display_page = DISP_OFF;
        oled_clear();
        oled_write_cmd(0xAE);  // Display OFF
        oled_on = false;
        break;
    }

    // Wait for button release
    while (button_pressed());
}

// Auto-off display after timeout
static void display_timeout_check(void) {
    if (display_page != DISP_OFF && oled_on) {
        if (rtc_seconds() - display_on_time >= OLED_TIMEOUT) {
            display_page = DISP_OFF;
            oled_clear();
            oled_write_cmd(0xAE);
            oled_on = false;
        }
    }
}

// ============================================================
// System init
// ============================================================
static void system_init(void) {
    // Clock: 60MHz HSI
    SetSysClock(CLK_SOURCE_HSI_60MHz);

    // GPIO + peripherals
    ws2812_init();
    button_init();
    oled_init();
    oled_clear();
    oled_write_cmd(0xAE);  // OLED off (save power)

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
                ws2812_set(32,0,0); for(volatile uint32_t _i=0;_i<1800000;_i++); ws2812_set(0,0,0); // Red x3 = error
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

    uint32_t now, last_record_save = 0, last_inactive_check = 0;
    uint32_t minute_steps = 0;
    uint8_t  minute_active_sec = 0;
    uint8_t  minute_intensity = 0;

    // Start in SLEEP mode: IMU WOM + MCU deep sleep
    // IMU INT1 will wake us when child moves
    ws2812_set(0,0,32); for(volatile uint32_t _i=0;_i<600000;_i++); ws2812_set(0,0,0); // Boot OK

    while (1) {
        now = rtc_seconds();

        // ====================================================
        // STATE: SLEEP (IMU WOM mode, 21μA)
        //   IMU watches for motion. INT1 → wakes MCU.
        //   MCU deep sleeps. No IMU polling needed.
        // ====================================================
        if (device_state == STATE_SLEEP) {

            // Check if WOM triggered (GPIO interrupt set flag)
            if (wom_triggered) {
                // Vibration detected → switch to ACTIVE
                imu_enter_active_mode();
                ws2812_set(0,32,0); for(volatile uint32_t _i=0;_i<900000;_i++); ws2812_set(0,0,0); // Woke up
                continue;      // Skip sleep, go measure
            }

            // In SLEEP: still handle BLE if phone is connected
            if (ble_connected && sync_active) {
                activity_record_t rec;
                if (storage_read(sync_index, &rec)) {
                    uint8_t buf[RECORD_SIZE];
                    buf[0] = (rec.timestamp>>0)&0xFF;  buf[1] = (rec.timestamp>>8)&0xFF;
                    buf[2] = (rec.timestamp>>16)&0xFF; buf[3] = (rec.timestamp>>24)&0xFF;
                    buf[4] = rec.steps&0xFF;            buf[5] = (rec.steps>>8)&0xFF;
                    buf[6] = rec.active_sec;            buf[7] = rec.intensity;
                    GATT_Notify(0, buf, RECORD_SIZE);
                    sync_index++;
                } else {
                    uint8_t end[] = {0xFF,0xFF,0xFF,0xFF,0,0,0,0};
                    GATT_Notify(0, end, RECORD_SIZE);
                    sync_active = 0;
                }
            }

            // Deep sleep: WFI + GPIO interrupt wakes us
            // No RTC timer needed in SLEEP (IMU INT1 handles wake)
            // But we wake periodically from BLE advertising events
            __WFI();
            WDOG_Feed();
            continue;  // Back to SLEEP check
        }

        // ====================================================
        // STATE: ACTIVE (IMU full 25Hz + MCU recording, 4.7mA)
        // ====================================================
        // Read IMU & detect steps
        int16_t accel[3], gyro[3];
        imu_read_6axis(accel, gyro);
        step_detect(accel[0], accel[1], accel[2], now * 1000);

        minute_steps = (uint16_t)step_count;
        minute_active_sec = activity_seconds;
        if (minute_active_sec > 40) minute_intensity = INTENSITY_HIGH;
        else if (minute_active_sec > 15) minute_intensity = INTENSITY_MEDIUM;
        else minute_intensity = INTENSITY_LOW;

        // ---- Save minute record ----
        if (now - last_record_save >= RECORD_INTERVAL) {
            last_record_save = now;
            activity_record_t rec;
            rec.timestamp  = now / 60;
            rec.steps      = minute_steps;
            rec.active_sec = minute_active_sec;
            rec.intensity  = minute_intensity;
            storage_write(&rec);
            step_count -= minute_steps;
            activity_seconds = 0;
            ws2812_set(0,8,0); for(volatile uint32_t i=0;i<30000;i++); ws2812_set(0,0,0);
        }

        // ---- Inactivity check: return to SLEEP ----
        if (now - last_inactive_check >= 60) {
            last_inactive_check = now;
            if (minute_steps == 0) {
                inactive_minutes++;
                if (inactive_minutes >= INACTIVE_MINUTES) {
                    // 5+ minutes no movement → back to SLEEP
                    imu_enter_wom_mode();
                    ws2812_set(32,16,0); for(volatile uint32_t _i=0;_i<900000;_i++); ws2812_set(0,0,0); // Sleeping
                    continue;
                }
            } else {
                inactive_minutes = 0;  // Reset counter on activity
            }
        }

        // ---- BLE live data notify (ACTIVE only) ----
        if (ble_connected && live_notify_enabled) {
            uint8_t live[16];
            live[0]=(accel[0]>>8)&0xFF; live[1]=accel[0]&0xFF;
            live[2]=(accel[1]>>8)&0xFF; live[3]=accel[1]&0xFF;
            live[4]=(accel[2]>>8)&0xFF; live[5]=accel[2]&0xFF;
            live[6]=(gyro[0]>>8)&0xFF;  live[7]=gyro[0]&0xFF;
            live[8]=(gyro[1]>>8)&0xFF;  live[9]=gyro[1]&0xFF;
            live[10]=(gyro[2]>>8)&0xFF; live[11]=gyro[2]&0xFF;
            live[12]=(step_count>>0)&0xFF; live[13]=(step_count>>8)&0xFF;
            live[14]=(step_count>>16)&0xFF; live[15]=(step_count>>24)&0xFF;
            GATT_Notify(0, live, 16);
        }

        // ---- BLE sync handler ----
        if (ble_connected && sync_active) {
            activity_record_t rec;
            if (storage_read(sync_index, &rec)) {
                uint8_t buf[RECORD_SIZE];
                buf[0]=(rec.timestamp>>0)&0xFF;  buf[1]=(rec.timestamp>>8)&0xFF;
                buf[2]=(rec.timestamp>>16)&0xFF; buf[3]=(rec.timestamp>>24)&0xFF;
                buf[4]=rec.steps&0xFF;            buf[5]=(rec.steps>>8)&0xFF;
                buf[6]=rec.active_sec;            buf[7]=rec.intensity;
                GATT_Notify(0, buf, RECORD_SIZE);
                sync_index++;
            } else {
                uint8_t end[] = {0xFF,0xFF,0xFF,0xFF,0,0,0,0};
                GATT_Notify(0, end, RECORD_SIZE);
                sync_active = 0;
            }
        }

        // ---- Button: toggle display, BLE sync ----
        button_handle();
        if (ble_connected && button_pressed() && display_page == DISP_OFF) {
            // Long press when display is off → toggle BLE sync
            sync_active = !sync_active; sync_index = 0;
            ws2812_set(0,0,32); for(volatile uint32_t i=0;i<600000;i++); ws2812_set(0,0,0);
            for (volatile uint32_t i=0; i<6000000; i++);
        }
        display_timeout_check();

        // ---- Sleep 1 second ----
        __WFI();
        WDOG_Feed();
    }
}
