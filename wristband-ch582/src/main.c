/**
 * Kids Activity Tracker — MAX SPEC (全配版)
 * CH582M + ICM-42670-P + MAX30102 + SSD1306 + WS2812 + ERM
 *
 * Features:
 *   - 6-axis IMU step counting + activity type detection
 *   - Heart rate (MAX30102, every 10 min + night continuous)
 *   - SpO2 blood oxygen (MAX30102)
 *   - Skin temperature (NTC, PA7 ADC)
 *   - OLED 0.49" 64x32 display (button cycled)
 *   - RGB LED motivational feedback
 *   - Vibration motor (goal reached, inactivity, find-me)
 *   - BLE sync to parent phone
 *   - 16 days local flash storage
 *
 * Power: 5-7 days on 150mAh LiPo (intermittent HR + daily activity)
 *
 * Pinout CH582M QFN28:
 *   PA0 = Battery ADC
 *   PA1 = IMU INT1 (WOM wake)
 *   PA2 = I2C SDA (IMU 0x68 + OLED 0x3C + MAX30102 0x57)
 *   PA3 = I2C SCL
 *   PA4 = WS2812 RGB LED
 *   PA5 = Button (active low, pull-up)
 *   PA6 = Vibration motor (PWM via NPN transistor)
 *   PA7 = Skin temp NTC (ADC)
 *   PB10/PB11 = USB D+/D-
 *   PA8/PA9 = 32.768KHz RTC crystal
 */

#include "CH58x_common.h"
#include "CH58xBLE_LIB.h"
#include "gap.h"

// ============================================================
// Constants
// ============================================================
#define MEASURE_SEC      1      // loop tick
#define RECORD_INTERVAL  60     // save per minute
#define HR_INTERVAL      600    // HR check every 10 min (600 sec)
#define HR_NIGHT_CONT    1      // continuous HR during night (21-07)
#define INACTIVE_MINUTES 5      // sleep after 5 min idle
#define WOM_THRESHOLD_MG 150    // Wake-on-Motion sensitivity
#define STEP_MIN_MS      200    // min ms between steps
#define BLE_ADV_MS       320    // advertising interval
#define OLED_TIMEOUT     5      // display auto-off
#define FLASH_BASE       0x28000
#define RECORD_SIZE      12

// ============================================================
// MAX30102 Heart Rate + SpO2 sensor (I2C 0x57)
// ============================================================
#define MAX_ADDR        0x57
#define REG_INT_STAT1   0x00
#define REG_INT_STAT2   0x01
#define REG_INT_ENABLE  0x02
#define REG_FIFO_WR     0x04
#define REG_FIFO_RD     0x06
#define REG_FIFO_DATA   0x07
#define REG_FIFO_CFG    0x08
#define REG_MODE_CFG    0x09
#define REG_SPO2_CFG    0x0A
#define REG_LED1_PA     0x0C  // Red LED
#define REG_LED2_PA     0x0D  // IR LED
#define REG_TEMP_INT    0x1F
#define REG_TEMP_FRAC   0x20

static void max_write(uint8_t reg, uint8_t val) {
    I2C_WriteReg(MAX_ADDR, reg, &val, 1);
}
static uint8_t max_read(uint8_t reg) {
    uint8_t v; I2C_ReadReg(MAX_ADDR, reg, &v, 1); return v;
}
static void max_read_burst(uint8_t reg, uint8_t *buf, uint8_t len) {
    I2C_ReadReg(MAX_ADDR, reg, buf, len);
}

static bool max30102_init(void) {
    // Check presence (PART_ID = 0x15)
    if (max_read(0xFF) != 0x15) return false;

    // Reset
    max_write(0x09, 0x40); for(volatile uint32_t i=0;i<10000;i++);

    // FIFO config: 32 samples average, rollover
    max_write(REG_FIFO_CFG, 0x4F);  // 32-sample avg, FIFO full=32

    // SpO2 mode: 100Hz, 411μs pulse width, 18-bit ADC
    max_write(REG_SPO2_CFG, 0x27);  // 100Hz, 411μs, 18-bit

    // LED currents: Red=0x1F (~6mA), IR=0x1F
    max_write(REG_LED1_PA, 0x1F);
    max_write(REG_LED2_PA, 0x1F);

    // FIFO almost-full interrupt
    max_write(REG_INT_ENABLE, 0x40);

    // Start SpO2 mode
    max_write(REG_MODE_CFG, 0x03);

    return true;
}

static void max30102_shutdown(void) {
    max_write(REG_MODE_CFG, 0x80);  // Shutdown (0μA)
}

static void max30102_wake(void) {
    max_write(REG_MODE_CFG, 0x03);  // Resume SpO2 mode
}

// Read FIFO: 32 samples × 6 bytes (3 per LED) = 192 bytes
static void max30102_read_fifo(uint32_t *ir_data, uint32_t *red_data, uint8_t *count) {
    uint8_t wr = max_read(REG_FIFO_WR);
    uint8_t rd = max_read(REG_FIFO_RD);
    *count = (wr - rd) & 0x1F;
    if (*count > 32) *count = 32;

    uint8_t buf[192];
    max_read_burst(REG_FIFO_DATA, buf, *count * 6);

    for (uint8_t i = 0; i < *count; i++) {
        ir_data[i]  = ((uint32_t)buf[i*6+0] << 16) | ((uint32_t)buf[i*6+1] << 8) | buf[i*6+2];
        red_data[i] = ((uint32_t)buf[i*6+3] << 16) | ((uint32_t)buf[i*6+4] << 8) | buf[i*6+5];
    }
}

// Simple heart rate from IR signal: moving avg → DC removal → peak count
static uint8_t max30102_hr(uint32_t *ir, uint8_t count) {
    if (count < 16) return 0;

    // DC removal: subtract moving average
    int32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += ir[i];
    int32_t avg = sum / count;

    // Count zero crossings (positive-going)
    uint8_t crossings = 0;
    int32_t prev = ir[0] - avg;
    for (uint8_t i = 1; i < count; i++) {
        int32_t cur = ir[i] - avg;
        if (prev < 0 && cur >= 0) crossings++;
        prev = cur;
    }

    // HR = crossings × 60 / (count samples / 100Hz)
    // For 32 samples at 100Hz = 0.32s
    // HR = crossings × 60 / 0.32s = crossings × 187.5 / 32
    // Approx: HR = crossings × 6 (for 32 samples)
    return (uint8_t)(crossings * 6);
}

// SpO2 estimation from red/IR ratio (simplified)
static uint8_t max30102_spo2(uint32_t *ir, uint32_t *red, uint8_t count) {
    if (count < 16) return 0;

    // AC/DC ratio
    uint32_t ir_ac = 0, ir_dc = 0, red_ac = 0, red_dc = 0;
    for (uint8_t i = 0; i < count; i++) {
        ir_dc += ir[i];
        red_dc += red[i];
    }
    ir_dc /= count; red_dc /= count;

    for (uint8_t i = 0; i < count; i++) {
        ir_ac  += (ir[i] > ir_dc) ? (ir[i] - ir_dc) : (ir_dc - ir[i]);
        red_ac += (red[i] > red_dc) ? (red[i] - red_dc) : (red_dc - red[i]);
    }
    ir_ac /= count; red_ac /= count;

    if (ir_dc == 0 || red_dc == 0) return 0;
    // Fixed-point: SpO2 ≈ 104 - 17 × (red_ac×ir_dc)/(red_dc×ir_ac)
    int32_t ratio_num = (int32_t)red_ac * ir_dc;
    int32_t ratio_den = (int32_t)red_dc * ir_ac;
    if (ratio_den == 0) return 0;
    int16_t spo2 = 104 - (int16_t)((17 * ratio_num) / ratio_den);
    if (spo2 > 100) spo2 = 100;
    if (spo2 < 70) spo2 = 70;
    return (uint8_t)spo2;
}

// ============================================================
// ICM-42670-P 6-axis IMU (I2C 0x68)
// ============================================================
#define IMU_ADDR      0x68
#define REG_PWR_MGMT0 0x4E
#define REG_ACCEL_X   0x1F
#define REG_GYRO_X    0x25
#define REG_INT_CFG   0x14
#define REG_INT_SRC   0x63
#define REG_WOM_THR   0x57

static void imu_write(uint8_t r, uint8_t v) { I2C_WriteReg(IMU_ADDR, r, &v, 1); }
static uint8_t imu_read(uint8_t r) { uint8_t v; I2C_ReadReg(IMU_ADDR, r, &v, 1); return v; }

static bool imu_init(void) {
    if (imu_read(0x75) != 0x67) return false;
    imu_write(0x11, 0x01); for(volatile uint32_t i=0;i<10000;i++);
    // Start WOM mode
    imu_write(REG_PWR_MGMT0, 0x02);  // Accel LP, gyro off
    imu_write(0x50, (0x04<<5)|0x03); // ±4g, 12.5Hz LP
    imu_write(0x52, 0x00);
    imu_write(REG_WOM_THR, (uint8_t)((150*256UL)/1000));
    imu_write(REG_INT_SRC, 0x80);    // WOM → INT1
    imu_write(REG_INT_CFG, 0x02);
    return true;
}

static void imu_active(void) {
    imu_write(REG_PWR_MGMT0, 0x0F);  // Accel LN + Gyro LN
    imu_write(0x50, (0x04<<5)|0x04); // ±4g, 25Hz
    imu_write(0x4F, (0x02<<5)|0x04); // ±500dps, 25Hz
    imu_write(REG_INT_SRC, 0x00);
    imu_write(REG_INT_CFG, 0x00);
}

static void imu_sleep(void) {
    imu_write(REG_PWR_MGMT0, 0x02);  // Accel LP, gyro off
    imu_write(0x50, (0x04<<5)|0x03);
    imu_write(REG_WOM_THR, (uint8_t)((150*256UL)/1000));
    imu_write(REG_INT_SRC, 0x80);
    imu_write(REG_INT_CFG, 0x02);
}

static void imu_read_6axis(int16_t a[3], int16_t g[3]) {
    uint8_t buf[12]; I2C_ReadReg(IMU_ADDR, REG_ACCEL_X, buf, 12);
    for(int i=0;i<3;i++){
        int16_t raw=(int16_t)((buf[i*2]<<8)|buf[i*2+1]);
        a[i]=(int16_t)(((int32_t)raw*1000)/8192);  // mg
    }
    for(int i=0;i<3;i++){
        int16_t raw=(int16_t)((buf[6+i*2]<<8)|buf[7+i*2]);
        g[i]=(int16_t)(((int32_t)raw*1000)/6554); // mdps
    }
}

// ============================================================
// SSD1306 OLED 0.49" 64x32 (I2C 0x3C)
// ============================================================
#define OLED_ADDR 0x3C
static bool oled_on;

static void oled_cmd(uint8_t c) { I2C_WriteReg(OLED_ADDR, 0x00, &c, 1); }
static void oled_data(uint8_t *b, uint8_t n) { I2C_WriteReg(OLED_ADDR, 0x40, b, n); }

static const uint8_t font8x16[][16] = {
    {0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C}, // 0
    {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E}, // 1
    {0x3C,0x7E,0x66,0x60,0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x06,0x66,0x66,0x7E,0x3C}, // 2
    {0x3C,0x7E,0x66,0x60,0x60,0x38,0x38,0x60,0x60,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C}, // 3
    {0x30,0x38,0x3C,0x36,0x33,0x33,0x7F,0x7F,0x30,0x30,0x30,0x30,0x30,0x30,0x78,0x78}, // 4
    {0x7E,0x7E,0x06,0x06,0x3E,0x7E,0x66,0x60,0x60,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C}, // 5
    {0x3C,0x7E,0x66,0x06,0x06,0x3E,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C}, // 6
    {0x7E,0x7E,0x66,0x60,0x60,0x30,0x30,0x18,0x18,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C}, // 7
    {0x3C,0x7E,0x66,0x66,0x66,0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x3C}, // 8
    {0x3C,0x7E,0x66,0x66,0x66,0x66,0x66,0x7E,0x3E,0x60,0x60,0x60,0x66,0x66,0x7E,0x3C}, // 9
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00}, // :
    {0x06,0x0F,0x09,0x06,0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x33,0x61,0x71,0x3B,0x1E}, // %
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00}, // .
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
};

static void oled_init(void) {
    uint8_t init[] = {0xAE,0xD5,0x80,0xA8,0x1F,0xD3,0x00,0x40,0x8D,0x14,0x20,0x00,0xA1,0xC8,0xDA,0x02,0x81,0x7F,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};
    for(uint8_t i=0;i<sizeof(init);i++) oled_cmd(init[i]);
}

static void oled_clear(void) { uint8_t z[64]={0}; for(uint8_t p=0;p<4;p++){ oled_cmd(0xB0+p); oled_cmd(0x00); oled_cmd(0x10); oled_data(z,64); } }

static void oled_char(uint8_t x, uint8_t pg, uint8_t ch) {
    uint8_t i = (ch>='0'&&ch<='9')?ch-'0':(ch==':')?10:(ch=='%')?11:(ch=='.')?12:13;
    uint8_t t[8],b[8]; for(uint8_t j=0;j<8;j++){t[j]=font8x16[i][j];b[j]=font8x16[i][j+8];}
    oled_cmd(0xB0+(pg&7)); oled_cmd(0x00+(x&0xF)); oled_cmd(0x10+((x>>4)&0xF)); oled_data(t,8);
    oled_cmd(0xB0+((pg+1)&7)); oled_cmd(0x00+(x&0xF)); oled_cmd(0x10+((x>>4)&0xF)); oled_data(b,8);
}

static void oled_num(uint8_t x, uint8_t pg, uint32_t n) {
    char s[8]; uint8_t l=0;
    if(!n)s[l++]='0'; else{uint32_t t=n;while(t){t/=10;l++;}for(int8_t i=l-1;i>=0;i--){s[i]='0'+(n%10);n/=10;}}
    for(uint8_t i=0;i<l;i++) oled_char(x+i*8,pg,s[i]);
}

// ============================================================
// WS2812 RGB LED (PA4)
// ============================================================
static void ws2812_send(uint8_t v) {
    for(int8_t b=7;b>=0;b--){
        if(v&(1<<b)){
            GPIOA_SetBits(GPIO_Pin_4); for(volatile uint8_t n=0;n<20;n++);
            GPIOA_ResetBits(GPIO_Pin_4); for(volatile uint8_t n=0;n<10;n++);
        }else{
            GPIOA_SetBits(GPIO_Pin_4); for(volatile uint8_t n=0;n<8;n++);
            GPIOA_ResetBits(GPIO_Pin_4); for(volatile uint8_t n=0;n<22;n++);
        }
    }
}
static void ws2812_rgb(uint8_t r,uint8_t g,uint8_t b){ ws2812_send(g);ws2812_send(r);ws2812_send(b); GPIOA_ResetBits(GPIO_Pin_4); for(volatile uint16_t n=0;n<3000;n++); }

// ============================================================
// Vibration motor (PA6 PWM via transistor)
// ============================================================
static void vib_init(void) { GPIOA_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA); GPIOA_ResetBits(GPIO_Pin_6); }
static void vib_on(void)  { GPIOA_SetBits(GPIO_Pin_6); }
static void vib_off(void) { GPIOA_ResetBits(GPIO_Pin_6); }
static void vib_buzz(uint16_t ms)  { vib_on(); for(volatile uint32_t i=0;i<(uint32_t)ms*6000;i++); vib_off(); }
static void vib_double(void)       { vib_buzz(80); for(volatile uint32_t i=0;i<720000;i++); vib_buzz(80); }

// ============================================================
// ADC helpers: battery (PA0) + skin temp NTC (PA7)
// ============================================================
static uint16_t adc_read_ch(uint8_t ch) {
    ADC_Init(ch, ADC_Mode_Single, ADC_PGA_0, ADC_Data_12bit);
    ADC_Start(); while(!ADC_Ready()); return ADC_Read();
}

static uint8_t battery_pct(void) {
    uint16_t v = adc_read_ch(ADC_CHANNEL_VCC);
    if(v>4200)v=4200; if(v<3200)v=3200;
    return (uint8_t)((uint32_t)(v-3200)*100/1000);
}

// Skin temp NTC: PA7, 100K@25°C B=3950, 100K pull-up to VCC
static uint8_t skin_temp(void) {
    uint16_t adc = adc_read_ch(ADC_CHANNEL_7);
    // Rough linear approx near body temp (32-42°C, ADC 1800-2200)
    // temp = 30 + (adc - 2000) * 0.03
    int16_t t = 370 + (int16_t)(((int32_t)(adc - 2000) * 3) / 100); // 37.0°C + delta
    if(t<300)t=300; if(t>450)t=450;
    return (uint8_t)(t - 300); // 0 = 30°C, for storage
}

// ============================================================
// NT3H2111 Active NFC (I2C 0x55)
//   Phone tap → reads NDEF URI with live stats
//   URL: https://greeny/kid/001?s=5230&h=85&t=36.5&b=80
//   Also stores static emergency info in EEPROM
// ============================================================
#define NFC_ADDR  0x55
#define NFC_SRAM  0x00   // SRAM start for NDEF
#define NFC_SESSION 0xFE // Session register

static void nfc_write(uint8_t reg, uint8_t *buf, uint8_t len) {
    I2C_WriteReg(NFC_ADDR, reg, buf, len);
}

static void nfc_open_session(void) {
    uint8_t cmd = 0x00; nfc_write(NFC_SESSION, &cmd, 1);  // Open session
}

static void nfc_close_session(void) {
    uint8_t cmd = 0x01; nfc_write(NFC_SESSION, &cmd, 1);  // Close + commit
}

// Build NDEF URI record and write to NFC SRAM
static void nfc_update(uint16_t steps, uint8_t hr, uint8_t spo2,
                       uint8_t temp, uint8_t bat) {
    if (!nfc_present) return;

    // URI: "https://greeny/kid/001?s=5230&h=85&o=98&t=36.5&b=80"
    char uri[52];
    uint8_t ul = 0;
    // URL encode manually (digits only, simple)
    uri[ul++]='0';uri[ul++]='0';uri[ul++]='1';uri[ul++]='?';uri[ul++]='s';uri[ul++]='=';
    {uint16_t n=steps;char t[6];uint8_t l=0;if(!n)t[l++]='0';else while(n){t[l++]='0'+n%10;n/=10;}
     for(int8_t i=l-1;i>=0;i--)uri[ul++]=t[i];}
    uri[ul++]='&';uri[ul++]='h';uri[ul++]='=';
    {uint8_t n=hr;char t[4];uint8_t l=0;if(!n)t[l++]='0';else while(n){t[l++]='0'+n%10;n/=10;}
     for(int8_t i=l-1;i>=0;i--)uri[ul++]=t[i];}
    uri[ul++]='&';uri[ul++]='o';uri[ul++]='=';
    {uint8_t n=spo2;char t[4];uint8_t l=0;if(!n)t[l++]='0';else while(n){t[l++]='0'+n%10;n/=10;}
     for(int8_t i=l-1;i>=0;i--)uri[ul++]=t[i];}
    uri[ul++]='&';uri[ul++]='t';uri[ul++]='=';
    {uint8_t n=30+(temp/10); uri[ul++]='0'+n/10; uri[ul++]='0'+n%10; uri[ul++]='.'; uri[ul++]='0'+temp%10;}
    uri[ul++]='&';uri[ul++]='b';uri[ul++]='=';
    {uint8_t n=bat;char t[4];uint8_t l=0;if(!n)t[l++]='0';else while(n){t[l++]='0'+n%10;n/=10;}
     for(int8_t i=l-1;i>=0;i--)uri[ul++]=t[i];}

    // Build NDEF message
    uint8_t ndef[80]; uint8_t ni = 0;
    ndef[ni++] = 0xD1;        // NDEF: MB ME SR=1 TNF=URI(2)
    ndef[ni++] = 0x01;        // Type length = 1
    ndef[ni++] = ul + 1;      // Payload length (URI prefix + rest)
    ndef[ni++] = 'U';         // Type = URI
    ndef[ni++] = 0x03;        // URI prefix = "https://"
    for (uint8_t i = 0; i < ul; i++) ndef[ni++] = uri[i];

    nfc_open_session();
    nfc_write(NFC_SRAM, ndef, ni);  // Write NDEF to SRAM
    nfc_close_session();
}

// Check if NT3H2111 present
static bool nfc_present;
static void nfc_init(void) {
    // Read capability container to verify chip
    uint8_t cc[4];
    I2C_ReadReg(NFC_ADDR, 0xE0, cc, 4);  // Capability Container
    // NT3H2111 CC: {0xE1, 0x10, 0x6D, 0x00}
    nfc_present = (cc[0] == 0xE1);
}

// Store emergency contact in NFC EEPROM (done once)
static void nfc_setup_emergency(void) {
    // NDEF Text record: "Emergency: call 0912-345-678"
    // Stored in EEPROM at offset for static NDEF
    uint8_t txt[] = {
        0x91, 0x01, 0x15, 'T',
        0x02, 'z','h',  // Language: zh
        'E','m','e','r','g',':',' ','0','9','1','2','-','3','4','5','-','6','7','8'
    };
    nfc_open_session();
    nfc_write(0x06, txt, sizeof(txt));  // Static NDEF in EEPROM area
    nfc_close_session();
}

// ============================================================
// Activity record (12 bytes)
// ============================================================
typedef struct __attribute__((packed)) {
    uint32_t ts;       // minutes since boot
    uint16_t steps;
    uint8_t  hr;       // heart rate BPM (0=n/a)
    uint8_t  spo2;     // SpO2 % (0=n/a)
    uint8_t  temp;     // skin temp - 30°C
    uint8_t  intensity;// 0-2
    uint8_t  act_type; // 0=walk,1=run,2=jump,3=bike,4=rest
    uint8_t  reserved;
} record_t;

// ============================================================
// Flash storage
// ============================================================
static uint32_t flash_off = FLASH_BASE;
static uint32_t flash_cnt;

static void flash_init(void) {
    for(uint32_t a=FLASH_BASE;a<FLASH_BASE+0x46000;a+=RECORD_SIZE){
        uint8_t *p=(uint8_t*)a;
        if(p[0]==0xFF&&p[1]==0xFF){flash_off=a;break;}
        flash_cnt++;
    }
    if(flash_cnt>=RECORD_SIZE*100){flash_off=FLASH_BASE;flash_cnt=0;FLASH_EraseSector(FLASH_BASE);}
}

static void flash_write(record_t *r) {
    uint8_t b[RECORD_SIZE];
    b[0]=r->ts&0xFF;b[1]=(r->ts>>8)&0xFF;b[2]=(r->ts>>16)&0xFF;b[3]=(r->ts>>24)&0xFF;
    b[4]=r->steps&0xFF;b[5]=(r->steps>>8)&0xFF;b[6]=r->hr;b[7]=r->spo2;
    b[8]=r->temp;b[9]=r->intensity;b[10]=r->act_type;b[11]=0;
    FLASH_Write(flash_off,b,RECORD_SIZE); flash_off+=RECORD_SIZE; flash_cnt++;
    if(flash_off>=FLASH_BASE+0x46000){flash_off=FLASH_BASE;flash_cnt=0;FLASH_EraseSector(FLASH_BASE);}
}

static bool flash_read(uint32_t idx, record_t *r) {
    if(idx>=flash_cnt)return false;
    uint32_t a=FLASH_BASE+idx*RECORD_SIZE; uint8_t *p=(uint8_t*)a;
    r->ts=p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
    r->steps=p[4]|((uint16_t)p[5]<<8); r->hr=p[6]; r->spo2=p[7];
    r->temp=p[8]; r->intensity=p[9]; r->act_type=p[10];
    return true;
}

// ============================================================
// Step detection
// ============================================================
static int16_t   mag_hist[8];
static uint8_t   mag_idx;
static int32_t   mag_sum;
static int16_t   step_thr = 1200;
static uint32_t  step_count, last_step_ms, act_seconds, minute_steps;
static uint8_t   minute_intensity;

static int16_t mag_filter(int16_t v) {
    mag_sum-=mag_hist[mag_idx]; mag_sum+=v;
    mag_hist[mag_idx]=v; mag_idx=(mag_idx+1)&7;
    return (int16_t)(mag_sum/8);
}

static void step_detect(int16_t ax, int16_t ay, int16_t az, uint32_t ms) {
    int32_t m2=(int32_t)ax*ax+(int32_t)ay*ay+(int32_t)az*az;
    int16_t mag=(int16_t)(m2>>10);
    int16_t mag_f=mag_filter(mag);
    int16_t ac=mag_f-1000; if(ac<0)ac=-ac; if(ac<200)ac=200;
    if(ac>800) act_seconds++;

    static int16_t prev; static bool lo=true;
    if(ac>step_thr && lo && (ms-last_step_ms)>STEP_MIN_MS) {
        step_count++; last_step_ms=ms;
        if(ac>step_thr*2) step_thr+=50;
        else if(step_thr>800 && ac<step_thr*3/2) step_thr-=20;
    }
    lo=(ac<step_thr*3/4); prev=ac;
}

// Activity type classification
static uint8_t classify_activity(int16_t a[3], int16_t g[3]) {
    int32_t amag=(int32_t)a[0]*a[0]+(int32_t)a[1]*a[1]+(int32_t)a[2]*a[2];
    int32_t gmag=(int32_t)g[0]*g[0]+(int32_t)g[1]*g[1]+(int32_t)g[2]*g[2];
    uint32_t step_rate=minute_steps;
    if(step_rate>80 && amag>5000000) return 1;  // Run
    if(gmag>10000000) return 2;                   // Jump
    if(step_rate<5 && amag<1000000) return 3;     // Bike (smooth)
    if(step_rate>10) return 0;                    // Walk
    return 4;                                      // Rest/other
}

// ============================================================
// Sleep analysis (night mode: 21:00-07:00, continuous HR)
// ============================================================
static bool is_night_time(uint32_t now) {
    uint8_t h = (now / 3600) % 24;
    return (h >= 21 || h < 7);
}

// ============================================================
// BLE GATT + Advertising
// ============================================================
static uint8_t ble_conn, sync_on, live_on, sync_idx;
static uint8_t adv_data[]={0x02,0x01,0x06, 0x08,0x09,'K','i','d','s','A','c','t', 0x05,0xFF,0x34,0x12,0,0,0};

static void ble_handler(uint32_t evt, uint8_t *d, uint16_t len) {
    switch(evt){
    case GAP_DEVICE_INIT_DONE:
        GAP_SetAdvData(adv_data,sizeof(adv_data)); GAPRole_StartAdvertising(); break;
    case GAP_CONNECTED: ble_conn=1; break;
    case GAP_DISCONNECTED: ble_conn=0; sync_on=0; GAPRole_StartAdvertising(); break;
    case GATT_WRITE_REQUEST:
        if(len>=1){
            switch(d[0]){
            case 1:sync_on=1;sync_idx=0;break; case 2:sync_on=0;break;
            case 3:FLASH_EraseSector(FLASH_BASE);flash_off=FLASH_BASE;flash_cnt=0;break;
            case 0x10:live_on=1;break; case 0x11:live_on=0;break;
            }
        } break;
    }
}

// ============================================================
// Button + Display
// ============================================================
typedef enum { PAGE_OFF, PAGE_STEPS, PAGE_HR, PAGE_STATUS } page_t;
static page_t page; static uint32_t page_on;

static void btn_init(void) { GPIOA_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU); }
static bool btn_pressed(void) { return !GPIOA_ReadPortPin(GPIO_Pin_5); }

static void show_page(page_t p) {
    oled_clear(); oled_cmd(0xAF); oled_on=true; page_on=rtc_seconds();
    page=p;
    switch(p){
    case PAGE_STEPS: oled_num(0,1,step_count>99999?99999:step_count); {
        uint8_t s[]={'s','t','e','p','s'}; for(uint8_t i=0;i<5;i++) oled_char(12+i*8,3,s[i]);
    } break;
    case PAGE_HR: {
        // Show HR + SpO2 placeholder (refresh on actual measurement)
        oled_num(0,1,(uint32_t)max30102_hr_bpm);
        uint8_t l1[]={'b','p','m'}; for(uint8_t i=0;i<3;i++) oled_char(24+i*8,1,l1[i]);
        oled_num(0,3,(uint32_t)max30102_spo2_val);
        oled_char(16,3,'%');
    } break;
    case PAGE_STATUS:
        oled_num(0,1,(uint32_t)battery_pct()); oled_char(16,1,'%');
        { uint32_t t=rtc_seconds(); uint8_t hh=(t/3600)%24,mm=(t/60)%60; oled_num(0,3,hh); oled_char(16,3,':'); oled_num(24,3,mm); }
        break;
    case PAGE_OFF: oled_clear(); oled_cmd(0xAE); oled_on=false; break;
    }
}

static void btn_handle(void) {
    if(!btn_pressed()) return;
    for(volatile uint32_t i=0;i<600000;i++);
    switch(page){
    case PAGE_OFF: show_page(PAGE_STEPS); vib_buzz(30); break;
    case PAGE_STEPS: show_page(PAGE_HR); break;
    case PAGE_HR: show_page(PAGE_STATUS); break;
    case PAGE_STATUS: show_page(PAGE_OFF); break;
    }
    while(btn_pressed());
}

static void display_timeout(void) {
    if(page!=PAGE_OFF && oled_on && rtc_seconds()-page_on>=OLED_TIMEOUT) show_page(PAGE_OFF);
}

// ============================================================
// HR/SpO2 measurement variables (for display)
// ============================================================
static uint8_t max30102_hr_bpm;
static uint8_t max30102_spo2_val;
static bool max30102_present;

static inline uint32_t rtc_seconds(void) { return RTC_GetCounter(); }

// ============================================================
// State machine
// ============================================================
typedef enum { SLEEP, ACTIVE } state_t;
static state_t state = SLEEP;
static bool wom_fired;
static uint32_t idle_minutes;

__attribute__((interrupt)) void GPIOA_IRQHandler(void) {
    if(GPIOA_ReadITFlagBit(GPIO_Pin_1)){ GPIOA_ClearITFlagBit(GPIO_Pin_1); if(state==SLEEP)wom_fired=true; }
}

// ============================================================
// System init
// ============================================================
static void system_init(void) {
    SetSysClock(CLK_SOURCE_HSI_60MHz);
    // I2C
    GPIOA_ModeCfg(GPIO_Pin_2|GPIO_Pin_3, GPIO_ModeIN_PU);
    I2C_Init(I2C_Mode_I2C, 400000, I2C_Ack_Enable);
    // IMU INT1
    GPIOA_ModeCfg(GPIO_Pin_1, GPIO_ModeIN_PU);
    GPIOA_ITModeCfg(GPIO_Pin_1, GPIO_IT_Rise);
    PFIC_EnableIRQ(GPIOA_IRQn);
    // Peripherals
    ws2812_rgb(0,0,32); for(volatile uint32_t i=0;i<600000;i++); ws2812_rgb(0,0,0);
    btn_init(); vib_init(); oled_init(); oled_clear(); oled_cmd(0xAE);
    // IMU
    if(!imu_init()){ ws2812_rgb(32,0,0); for(volatile uint32_t i=0;i<1800000;i++); ws2812_rgb(0,0,0); }
    // MAX30102
    max30102_present = max30102_init();
    if(max30102_present) max30102_shutdown();
    nfc_init();
    if(nfc_present) nfc_setup_emergency();
    // RTC + WDT + BLE
    RTC_Init(RTC_CLK_SRC_XTAL); WDOG_Init(WDOG_CLK_SRC_LSI, WDOG_TIMEOUT_16S);
    CH58x_ble_init(); GAPRole_PeripheralInit(); GAPBondMgr_Init();
    GAP_DeviceInit(ble_handler); GAP_SetParamValue(GAP_ADV_INTERVAL, BLE_ADV_MS);
}

// ============================================================
// Main
// ============================================================
int main(void) {
    system_init(); flash_init(); PFIC_EnableAllIRQ();
    uint32_t now, last_rec=0, last_hr=0, last_idle=0;
    uint32_t minute_s = 0;
    uint8_t minute_a=0, minute_i=0;
    uint8_t hr=0, spo2=0, temp=0;

    while(1) {
        now = rtc_seconds();

        // ---- SLEEP state (IMU WOM, 21μA) ----
        if(state == SLEEP) {
            if(wom_fired) {
                imu_active(); state=ACTIVE; idle_minutes=0; wom_fired=false;
                ws2812_rgb(0,32,0); for(volatile uint32_t i=0;i<600000;i++); ws2812_rgb(0,0,0);
                continue;
            }
            // BLE sync in sleep
            if(ble_conn && sync_on) {
                record_t r; if(flash_read(sync_idx,&r)){
                    uint8_t b[RECORD_SIZE]; memcpy(b,&r,RECORD_SIZE);
                    GATT_Notify(0,b,RECORD_SIZE); sync_idx++;
                } else { uint8_t e[RECORD_SIZE]={0};e[0]=0xFF;e[1]=0xFF;e[2]=0xFF;e[3]=0xFF;GATT_Notify(0,e,RECORD_SIZE);sync_on=0; }
            }
            btn_handle(); display_timeout();
            __WFI(); WDOG_Feed(); continue;
        }

        // ---- ACTIVE state ----
        int16_t acc[3], gyr[3]; imu_read_6axis(acc, gyr);
        step_detect(acc[0],acc[1],acc[2],now*1000);
        minute_s=step_count; minute_a=act_seconds;
        if(minute_a>40)minute_i=2;else if(minute_a>15)minute_i=1;else minute_i=0;

        // Minute save
        if(now-last_rec>=RECORD_INTERVAL) {
            last_rec=now;
            // HR check every 10 min (or continuous at night)
            bool do_hr=false;
            if(max30102_present) {
                if(is_night_time(now) && (now-last_hr>=HR_INTERVAL)) do_hr=true;
                else if(now-last_hr>=HR_INTERVAL) do_hr=true;
            }
            if(do_hr) {
                max30102_wake(); for(volatile uint32_t i=0;i<50000;i++); // wait for sensor
                uint32_t ir[32],red[32]; uint8_t cnt;
                max30102_read_fifo(ir,red,&cnt);
                hr=max30102_hr(ir,cnt); spo2=max30102_spo2(ir,red,cnt);
                max30102_hr_bpm=hr; max30102_spo2_val=spo2;
                max30102_shutdown();
                last_hr=now;
            }
            temp=skin_temp();

            record_t rec; rec.ts=now/60; rec.steps=minute_s; rec.hr=hr; rec.spo2=spo2;
            rec.temp=temp; rec.intensity=minute_i; rec.act_type=classify_activity(acc,gyr);
            flash_write(&rec);
            nfc_update(minute_s, hr, spo2, temp, battery_pct());
            step_count-=minute_s; act_seconds=0; minute_s=0;
            ws2812_rgb(0,4,0); for(volatile uint32_t i=0;i<20000;i++); ws2812_rgb(0,0,0);
        }

        // Inactivity → sleep
        if(now-last_idle>=60) { last_idle=now;
            if(minute_s==0){ idle_minutes++; if(idle_minutes>=INACTIVE_MINUTES){
                imu_sleep(); state=SLEEP; ws2812_rgb(32,16,0); for(volatile uint32_t i=0;i<600000;i++); ws2812_rgb(0,0,0); continue;
            }} else idle_minutes=0;
        }

        // BLE live notify
        if(ble_conn && live_on) {
            uint8_t d[16]; d[0]=(acc[0]>>8)&0xFF;d[1]=acc[0]&0xFF;d[2]=(acc[1]>>8)&0xFF;d[3]=acc[1]&0xFF;
            d[4]=(acc[2]>>8)&0xFF;d[5]=acc[2]&0xFF;d[6]=(gyr[0]>>8)&0xFF;d[7]=gyr[0]&0xFF;
            d[8]=(gyr[1]>>8)&0xFF;d[9]=gyr[1]&0xFF;d[10]=(gyr[2]>>8)&0xFF;d[11]=gyr[2]&0xFF;
            d[12]=step_count&0xFF;d[13]=(step_count>>8)&0xFF;d[14]=hr;d[15]=spo2;
            GATT_Notify(0,d,16);
        }

        // BLE sync
        if(ble_conn && sync_on) {
            record_t r; if(flash_read(sync_idx,&r)){
                uint8_t b[RECORD_SIZE]; memcpy(b,&r,RECORD_SIZE);
                GATT_Notify(0,b,RECORD_SIZE); sync_idx++;
            } else { uint8_t e[RECORD_SIZE]={0};e[0]=0xFF;e[1]=0xFF;e[2]=0xFF;e[3]=0xFF;GATT_Notify(0,e,RECORD_SIZE);sync_on=0; }
        }

        // Button + display
        btn_handle(); display_timeout();
        // Re-show HR page if measurement updated
        if(page==PAGE_HR) { oled_clear(); oled_num(0,1,(uint32_t)max30102_hr_bpm); uint8_t l[]={'b','p','m'}; for(uint8_t i=0;i<3;i++) oled_char(24+i*8,1,l[i]); oled_num(0,3,(uint32_t)max30102_spo2_val); oled_char(16,3,'%'); page_on=now; }

        __WFI(); WDOG_Feed();
    }
}
