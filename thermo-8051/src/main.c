/**
 * Greeny Temp Controller — STC8H8K64U USB-direct + JDY-23 BLE + RTC
 *
 * USB CDC Virtual COM: no CH340 needed. Firmware enumerates as /dev/tty.usbmodem*
 * Auto-ISP via "!ISP" command → software reboot into USB bootloader.
 * First flash (blank chip): hold P3.2 button, plug USB, flash with stc8prog.
 * All subsequent flashes: ./burn.sh (fully automatic).
 *
 * Pinout (STC8H8K64U LQFP-32):
 *   P3.0/P3.1  = USB D-/D+  →  USB connector
 *   P1.2/P1.3  = UART2       →  JDY-23 BLE
 *   P1.0/ADC0  = NTC 100K B=3950
 *   P0.2       = MOSFET gate (fan/heater)
 *   P3.2       = ISP button (hold→GND on power-up for initial flash only)
 *   P1.6/P1.7  = 32.768KHz crystal → hardware RTC
 *
 * Build:  sdcc --model-large --std-c99 main.c -o thermo.ihx
 * Flash:  ./burn.sh  (auto via USB CDC → !ISP → bootloader)
 *
 * Protocol:
 *   Host→MCU (CDC):  "T=2500" set target, "H=20" hyst, "O=1/0/A" mode
 *                     "R" status, "S=HHMMSS" set time, "!ISP" reboot to bootloader
 *   MCU→Host (CDC):  "C=2475,O=0,M=A,TS=143025\r\n"
 *   BLE UART2:       same protocol for phone app
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// 8051 standard SFRs
// ============================================================
__sfr __at(0x80) P0;
__sfr __at(0x90) P1;
__sfr __at(0xA0) P2;
__sfr __at(0xB0) P3;
__sfr __at(0x87) PCON;
__sfr __at(0x8E) AUXR;
__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;
__sfr __at(0xA8) IE;
__sfr __at(0xA9) IE2;
__sfr __at(0xB8) IP;
__sfr __at(0xBA) P_SW2;
__sfr __at(0xC1) WDT_CONTR;
__sfr __at(0xD0) PSW;

// ============================================================
// STC8H8K64U extended SFRs (XDATA, P_SW2.7 gated)
// ============================================================
// ADC
#define XA_ADC_CONTR   0xFE01
#define XA_ADC_RES     0xFE02
#define XA_ADC_RESL    0xFE03
#define XA_ADC_CFG     0xFE04

// GPIO mode
#define XA_P0M0        0xFE48
#define XA_P0M1        0xFE49
#define XA_P1M0        0xFE4B
#define XA_P1M1        0xFE4C

// UART2 (JDY-23 BLE)
#define XA_S2CON       0xFE18
#define XA_S2BUF       0xFE17
#define XA_S2CFG       0xFE19  // UART1 baud source select (not used for USB CDC)
#define XA_T4L         0xFE42
#define XA_T4H         0xFE43
#define XA_T4T3M       0xFE44

// RTC
#define XA_RTC_CTRL    0xFEA0
#define XA_RTC_SEC     0xFEA1
#define XA_RTC_MIN     0xFEA2
#define XA_RTC_HOUR    0xFEA3
#define XA_RTC_DAY     0xFEA4
#define XA_RTC_MON     0xFEA5
#define XA_RTC_YEAR    0xFEA6
#define XA_RTC_ALMCTRL 0xFEAA

// IAP / software reset
#define XA_IAP_CONTR   0xFE35

// USB registers
#define XA_USB_BASE    0xF880

// ============================================================
// XDATA register helpers
// ============================================================
static inline void xwr(uint16_t a, uint8_t v) {
    P_SW2 |= 0x80;
    (*(volatile uint8_t __xdata *)(a)) = v;
    P_SW2 &= ~0x80;
}
static inline uint8_t xrd(uint16_t a) {
    uint8_t v;
    P_SW2 |= 0x80;
    v = (*(volatile uint8_t __xdata *)(a));
    P_SW2 &= ~0x80;
    return v;
}

// ============================================================
// Pin definitions
// ============================================================
#define SBIT(n,a,b)  __sbit __at((a)+(b)) n
SBIT(OUT_PIN, 0x80, 2);  // P0.2 MOSFET gate (fan/heater)
SBIT(JDY_PWR, 0x80, 3);  // P0.3 → 1K → AO3401 Gate → JDY-23 VCC switch
SBIT(ISP_BTN, 0xB0, 2);  // P3.2 ISP button (read-only, active low)

// ============================================================
// Constants
// ============================================================
#define FOSC            24000000UL
#define BLE_BAUD        9600
#define MEASURE_SEC     10
#define REPORT_SEC      60
#define DEFAULT_TARGET  250
#define DEFAULT_HYST    20
#define BLE_ON_SEC      3     // JDY-23 powered ON for 3s per cycle (advertise temp)

// ============================================================
// USB CDC — Device/Config/String descriptors
// ============================================================
// We store descriptors in __code to save XDATA RAM.
// Packed structs would be cleaner but SDCC handles __code arrays well.

// ---- Device Descriptor ----
static const __code uint8_t usb_dev_desc[18] = {
    18,             // bLength
    0x01,           // bDescriptorType: DEVICE
    0x00, 0x02,     // bcdUSB 2.0
    0x02,           // bDeviceClass: CDC
    0x00,           // bDeviceSubClass
    0x00,           // bDeviceProtocol
    64,             // bMaxPacketSize0
    0x34, 0x12,     // idVendor 0x1234
    0x01, 0x56,     // idProduct 0x5601
    0x00, 0x01,     // bcdDevice 1.00
    0x01,           // iManufacturer
    0x02,           // iProduct
    0x03,           // iSerialNumber
    0x01            // bNumConfigurations
};

// ---- Config Descriptor (CDC ACM: 2 interfaces, 3 EPs) ----
static const __code uint8_t usb_cfg_desc[67] = {
    // Config descriptor
    9, 0x02, 67, 0x00, 0x02, 0x01, 0x00, 0xC0, 50,
    // Interface 0 (CDC Comm)
    9, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    // Header functional descriptor
    5, 0x24, 0x00, 0x10, 0x01,
    // ACM functional descriptor
    4, 0x24, 0x02, 0x06,
    // Union functional descriptor
    5, 0x24, 0x06, 0x00, 0x01,
    // Call management functional descriptor
    5, 0x24, 0x01, 0x00, 0x01,
    // Endpoint 1 IN (interrupt, notification)
    7, 0x05, 0x81, 0x03, 0x08, 0x00, 0x20,
    // Interface 1 (CDC Data)
    9, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    // Endpoint 2 OUT (bulk, host→device)
    7, 0x05, 0x02, 0x02, 64, 0x00, 0x00,
    // Endpoint 3 IN  (bulk, device→host)
    7, 0x05, 0x83, 0x02, 64, 0x00, 0x00,
};

// ---- String Descriptors ----
static const __code uint8_t usb_str_lang[] = { 4, 0x03, 0x09, 0x04 }; // English US

static const __code uint8_t usb_str_mfr[] = {
    18, 0x03,
    'G',0,'r',0,'e',0,'e',0,'n',0,'y',0,' ',0,'L',0,'a',0,'b',0
};

static const __code uint8_t usb_str_prod[] = {
    34, 0x03,
    'T',0,'e',0,'m',0,'p',0,' ',0,'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,
    'l',0,'e',0,'r',0,' ',0,'U',0,'S',0,'B',0
};

static const __code uint8_t usb_str_ser[] = {
    12, 0x03,
    '0',0,'0',0,'0',0,'1',0
};

// ============================================================
// USB Hardware Register map
// ============================================================
#define USBOFF(n)  (XA_USB_BASE + (n))

// USB registers (write macros)
#define USB_WR(o,v)  xwr(USBOFF(o), (v))
#define USB_RD(o)    xrd(USBOFF(o))

// Key USB register offsets (adjust per datasheet)
#define OFF_USBCLK   0x00   // USB clock divider
#define OFF_USBCON   0x01   // USB control
#define OFF_USBST    0x02   // USB status
#define OFF_USBINTF  0x05   // USB interrupt flag
#define OFF_USBINTE  0x06   // USB interrupt enable
#define OFF_USBEPIDX 0x09   // Endpoint index select
#define OFF_USBEPCS  0x0A   // Endpoint control/status
#define OFF_USBTXCNT 0x0E   // TX byte count
#define OFF_USBFIFO  0x10   // FIFO data (read/write)
#define OFF_USBMISC  0x1C   // Miscellaneous

// USB clock values
#define USBCLK_48M    0x02   // 48MHz from internal PLL
#define USBCLK_ENABLE 0x80

// USB control bits
#define USB_PU_EN     0x04   // Pull-up enable
#define USB_ENABLE    0x80

// Endpoint control bits
#define EP_READY      0x80
#define EP_STALL      0x40
#define EP_IN         0x08
#define EP_OUT        0x00
#define EP_BULK       0x02
#define EP_INT        0x04

// ============================================================
// USB CDC state
// ============================================================
static __xdata uint8_t  usb_configured;
static __xdata uint8_t  usb_tx_busy;
static __xdata uint8_t  cdc_rx_buf[64];
static __xdata uint8_t  cdc_rx_len;

// ============================================================
// USB helpers
// ============================================================
static void usb_select_ep(uint8_t ep) { USB_WR(OFF_USBEPIDX, ep); }
static void usb_write_fifo(const uint8_t __xdata *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) USB_WR(OFF_USBFIFO, buf[i]);
}
static void usb_read_fifo(uint8_t __xdata *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) buf[i] = USB_RD(OFF_USBFIFO);
}

// ============================================================
// USB endpoint init
// ============================================================
static void usb_ep_init(void) {
    // EP0: Control, 64 bytes
    usb_select_ep(0);
    USB_WR(OFF_USBEPCS, EP_READY);

    // EP1: IN, Interrupt, 8 bytes (CDC notification)
    usb_select_ep(1);
    USB_WR(OFF_USBEPCS, EP_READY | EP_IN | EP_INT);

    // EP2: OUT, Bulk, 64 bytes (host→device data)
    usb_select_ep(2);
    USB_WR(OFF_USBEPCS, EP_READY | EP_OUT | EP_BULK);

    // EP3: IN, Bulk, 64 bytes (device→host data)
    usb_select_ep(3);
    USB_WR(OFF_USBEPCS, EP_READY | EP_IN | EP_BULK);
}

// ============================================================
// USB init
// ============================================================
static void usb_init(void) {
    // Configure USB clock: 48MHz from internal PLL
    USB_WR(OFF_USBCLK, USBCLK_ENABLE | USBCLK_48M);

    // Enable USB interrupts
    USB_WR(OFF_USBINTE, 0x1F);  // Reset, Suspend, Resume, SOF, Transfer-done
    IE2 |= 0x10;                 // Enable USB interrupt in IE2

    // Enable USB PHY with pull-up
    USB_WR(OFF_USBCON, USB_ENABLE | USB_PU_EN);

    // Wait for USB stable
    for (volatile uint16_t i = 0; i < 5000; i++);

    // Init endpoints
    usb_select_ep(0);
    USB_WR(OFF_USBEPCS, EP_READY);
}

// ============================================================
// USB send data to host (EP3 bulk IN)
// ============================================================
static uint8_t usb_cdc_send(const uint8_t __xdata *buf, uint8_t len) {
    if (len == 0 || len > 64) return 0;
    if (usb_tx_busy) return 0;
    usb_tx_busy = 1;

    usb_select_ep(3);
    if (!(USB_RD(OFF_USBEPCS) & EP_READY)) {
        usb_tx_busy = 0;
        return 0;
    }
    USB_WR(OFF_USBTXCNT, len);
    usb_write_fifo(buf, len);
    USB_WR(OFF_USBEPCS, EP_READY | EP_IN | EP_BULK);
    return len;
}

// ============================================================
// USB standard request handlers (EP0)
// ============================================================
static void usb_handle_get_descriptor(void) {
    uint8_t type = cdc_rx_buf[3];  // wValue high byte
    uint8_t idx  = cdc_rx_buf[2];  // wValue low byte

    switch (type) {
    case 0x01: // Device
        usb_select_ep(0);
        USB_WR(OFF_USBTXCNT, 18);
        usb_write_fifo(usb_dev_desc, 18);
        USB_WR(OFF_USBEPCS, EP_READY | EP_IN);
        break;
    case 0x02: // Config
        usb_select_ep(0);
        USB_WR(OFF_USBTXCNT, sizeof(usb_cfg_desc));
        usb_write_fifo(usb_cfg_desc, sizeof(usb_cfg_desc));
        USB_WR(OFF_USBEPCS, EP_READY | EP_IN);
        break;
    case 0x03: // String
        usb_select_ep(0);
        if (idx == 0) {
            USB_WR(OFF_USBTXCNT, usb_str_lang[0]);
            usb_write_fifo(usb_str_lang, usb_str_lang[0]);
        } else if (idx == 1) {
            USB_WR(OFF_USBTXCNT, usb_str_mfr[0]);
            usb_write_fifo(usb_str_mfr, usb_str_mfr[0]);
        } else if (idx == 2) {
            USB_WR(OFF_USBTXCNT, usb_str_prod[0]);
            usb_write_fifo(usb_str_prod, usb_str_prod[0]);
        } else if (idx == 3) {
            USB_WR(OFF_USBTXCNT, usb_str_ser[0]);
            usb_write_fifo(usb_str_ser, usb_str_ser[0]);
        } else {
            USB_WR(OFF_USBEPCS, EP_STALL);
        }
        USB_WR(OFF_USBEPCS, EP_READY | EP_IN);
        break;
    default:
        USB_WR(OFF_USBEPCS, EP_STALL);
        break;
    }
}

static void usb_handle_setup(void) {
    // cdc_rx_buf has the 8-byte setup packet
    uint8_t bmRequestType = cdc_rx_buf[0];
    uint8_t bRequest      = cdc_rx_buf[1];

    switch (bmRequestType) {
    case 0x80: // Standard device IN
        if (bRequest == 0x06) usb_handle_get_descriptor();  // GET_DESCRIPTOR
        else if (bRequest == 0x00) { usb_select_ep(0); USB_WR(OFF_USBEPCS, EP_READY|EP_IN); } // GET_STATUS
        else { usb_select_ep(0); USB_WR(OFF_USBEPCS, EP_STALL); }
        break;

    case 0x00: // Standard device OUT
        if (bRequest == 0x09) { // SET_CONFIGURATION
            usb_configured = cdc_rx_buf[2];  // wValue low byte (0 or 1)
            if (usb_configured) usb_ep_init();
            usb_select_ep(0);
            USB_WR(OFF_USBEPCS, EP_READY);  // ACK (0-length)
        } else if (bRequest == 0x05) { // SET_ADDRESS
            USB_WR(OFF_USBCON, USB_ENABLE | USB_PU_EN | (cdc_rx_buf[2] & 0x7F));
            usb_select_ep(0);
            USB_WR(OFF_USBEPCS, EP_READY);
        } else {
            usb_select_ep(0);
            USB_WR(OFF_USBEPCS, EP_STALL);
        }
        break;

    case 0x21: // CDC class request (interface)
        if (bRequest == 0x22 || bRequest == 0x20) { // SET_LINE_CODING, SET_LINE_CODING
            usb_select_ep(0);
            USB_WR(OFF_USBEPCS, EP_READY);  // ACK
        } else {
            usb_select_ep(0);
            USB_WR(OFF_USBEPCS, EP_STALL);
        }
        break;

    default:
        usb_select_ep(0);
        USB_WR(OFF_USBEPCS, EP_STALL);
        break;
    }
}

// ============================================================
// USB interrupt handler
// ============================================================
void usb_isr(void) __interrupt(19) {  // STC8H USB interrupt vector
    uint8_t intf = USB_RD(OFF_USBINTF);

    if (intf & 0x01) {  // Reset
        USB_WR(OFF_USBINTF, ~0x01);
        usb_configured = 0;
        usb_tx_busy = 0;
        USB_WR(OFF_USBCON, USB_ENABLE | USB_PU_EN);
        usb_select_ep(0);
        USB_WR(OFF_USBEPCS, EP_READY);
    }

    if (intf & 0x08) {  // Transfer done
        USB_WR(OFF_USBINTF, ~0x08);
        uint8_t cs = USB_RD(OFF_USBEPCS);  // Current EP status

        if (!usb_configured) {
            // EP0 setup: read 8-byte setup packet
            if (!(cs & EP_READY)) {
                usb_read_fifo(cdc_rx_buf, 8);
                usb_handle_setup();
            }
        } else {
            // EP2 OUT (host→device data)
            usb_select_ep(2);
            if (!(USB_RD(OFF_USBEPCS) & EP_READY)) {
                cdc_rx_len = 0;
                // Read available data from EP2 FIFO
                for (uint8_t i = 0; i < 64; i++) {
                    if (USB_RD(OFF_USBEPCS) & EP_READY) break;
                    cdc_rx_buf[cdc_rx_len++] = USB_RD(OFF_USBFIFO);
                }
                USB_WR(OFF_USBEPCS, EP_READY | EP_OUT | EP_BULK);  // Re-arm
            }

            // EP3 IN completion
            usb_select_ep(3);
            if (USB_RD(OFF_USBEPCS) & EP_READY) {
                usb_tx_busy = 0;
            }
        }
    }

    if (intf & 0x10) {  // SOF
        USB_WR(OFF_USBINTF, ~0x10);
    }
}

// ============================================================
// UART2 (P1.2/P1.3) — JDY-23 BLE
// ============================================================
static void uart2_init(void) {
    // P1.2 push-pull (TXD2), P1.3 input (RXD2)
    uint8_t m0 = xrd(XA_P1M0);
    uint8_t m1 = xrd(XA_P1M1);
    m0 |= 0x04; m1 &= ~0x0C;
    xwr(XA_P1M0, m0); xwr(XA_P1M1, m1);

    xwr(XA_S2CON, 0x50);  // Mode 1, REN=1

    uint16_t reload = 65536UL - (FOSC / (4UL * BLE_BAUD));
    xwr(XA_T4L, (uint8_t)(reload & 0xFF));
    xwr(XA_T4H, (uint8_t)(reload >> 8));
    uint8_t t4 = xrd(XA_T4T3M);
    t4 |= 0x04 | 0x20;  // T4R=1, T4x12=1
    xwr(XA_T4T3M, t4);
}

static void uart2_putc(char c) {
    uint8_t s;
    do { s = xrd(XA_S2CON); } while (!(s & 0x02));
    s &= ~0x02; xwr(XA_S2CON, s);
    xwr(XA_S2BUF, c);
}

static void uart2_puts(const char *s) { while (*s) uart2_putc(*s++); }

static bool uart2_rx_ready(void) { return (xrd(XA_S2CON) & 0x01); }

static char uart2_getc(void) {
    uint8_t s = xrd(XA_S2CON);
    s &= ~0x01; xwr(XA_S2CON, s);
    return xrd(XA_S2BUF);
}

// ============================================================
// ADC (P1.0, 12-bit)
// ============================================================
static void adc_init(void) {
    uint8_t m1 = xrd(XA_P1M1); m1 |= 0x01; xwr(XA_P1M1, m1);
    xwr(XA_ADC_CFG, 0x0F);
    xwr(XA_ADC_CONTR, 0x80);
}

static uint16_t adc_read(uint8_t ch) {
    xwr(XA_ADC_CONTR, 0x80 | 0x40 | (ch & 0x0F));
    while (!(xrd(XA_ADC_CONTR) & 0x20));
    uint16_t r = ((uint16_t)xrd(XA_ADC_RES) << 8) | xrd(XA_ADC_RESL);
    xwr(XA_ADC_CONTR, 0x00);
    return r & 0x0FFF;
}

// ============================================================
// NTC lookup (100K B=3950, 100K to VCC, NTC to GND)
// ============================================================
static const __code uint16_t ntc_table[][2] = {
    {3134,0},{2963,50},{2727,100},{2499,150},{2278,200},{2160,225},
    {2047,250},{1938,275},{1827,300},{1624,350},{1424,400},{1232,450},{1085,500},
};
#define NTC_LEN (sizeof(ntc_table)/sizeof(ntc_table[0]))

static int16_t ntc_to_temp(uint16_t adc) {
    if (adc >= ntc_table[0][0]) return ntc_table[0][1];
    if (adc <= ntc_table[NTC_LEN-1][0]) return ntc_table[NTC_LEN-1][1];
    for (uint8_t i = 0; i < NTC_LEN-1; i++) {
        if (adc <= ntc_table[i][0] && adc >= ntc_table[i+1][0]) {
            uint16_t rng = ntc_table[i][0] - ntc_table[i+1][0];
            if (rng == 0) return ntc_table[i+1][1];
            return ntc_table[i+1][1] + (int16_t)(
                ((uint32_t)(adc - ntc_table[i+1][0]) *
                 (uint16_t)(ntc_table[i][1] - ntc_table[i+1][1])) / rng);
        }
    }
    return 250;
}

// ============================================================
// RTC (32.768KHz crystal on P1.6/P1.7)
// ============================================================
static uint8_t b2b(uint8_t v) { return ((v/10)<<4)|(v%10); }
static uint8_t b2d(uint8_t v) { return ((v>>4)*10)+(v&0x0F); }
#define RTC_EN  0x01
#define RTC_WAKE 0x02
#define RTC_WR  0x04
#define RTC_ALMF 0x01

static void rtc_init(void) {
    uint8_t m0 = xrd(XA_P1M0), m1 = xrd(XA_P1M1);
    m0 &= ~0xC0; m1 |= 0xC0;  // P1.6/P1.7 input (XTAL)
    xwr(XA_P1M0, m0); xwr(XA_P1M1, m1);
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WR);  // Start RTC
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WAKE);
}

static void rtc_set_time(uint8_t h, uint8_t m, uint8_t s) {
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WR);
    xwr(XA_RTC_HOUR, b2b(h)); xwr(XA_RTC_MIN, b2b(m)); xwr(XA_RTC_SEC, b2b(s));
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WAKE);
}

static void rtc_set_alarm(uint8_t sec) {
    uint8_t s = b2d(xrd(XA_RTC_SEC)), m = b2d(xrd(XA_RTC_MIN)), h = b2d(xrd(XA_RTC_HOUR));
    s += sec;
    while (s >= 60) { s -= 60; m++; }
    while (m >= 60) { m -= 60; h++; }
    while (h >= 24) { h -= 24; }
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WR);
    xwr(XA_RTC_ALMSEC, b2b(s)); xwr(XA_RTC_ALMMIN, b2b(m)); xwr(XA_RTC_ALMHOUR, b2b(h));
    xwr(XA_RTC_CTRL, RTC_EN | RTC_WAKE);
}

static void rtc_read(uint8_t *h, uint8_t *m, uint8_t *s) {
    *h = b2d(xrd(XA_RTC_HOUR)); *m = b2d(xrd(XA_RTC_MIN)); *s = b2d(xrd(XA_RTC_SEC));
}

static bool rtc_alarm_fired(void) { return (xrd(XA_RTC_ALMCTRL) & RTC_ALMF); }
static void rtc_alarm_clear(void) { xwr(XA_RTC_ALMCTRL, 0x00); }

// ============================================================
// WDT
// ============================================================
static void wdt_init(void)  { WDT_CONTR = 0x80 | 0x08 | 0x02; }  // ~16s
static void wdt_feed(void)  { WDT_CONTR |= 0x10; }

// ============================================================
// Software reboot to USB ISP bootloader
// ============================================================
static void reboot_to_isp(void) {
    // Disable interrupts
    EA = 0;
    // Set ISP boot flag and software reset
    // IAP_CONTR: bit6=SWRST, bit5=ISPEN
    xwr(XA_IAP_CONTR, 0x60);
    while (1);  // Wait for reset
}

// ============================================================
// Global state
// ============================================================
static __xdata int16_t  target     = DEFAULT_TARGET;
static __xdata int16_t  current    = 2500;
static __xdata int16_t  hyst       = DEFAULT_HYST;
static __xdata uint8_t  out_state  = 0;
static __xdata uint8_t  op_mode    = 'A';
static __xdata uint8_t  report_cnt   = 0;
static __xdata uint8_t  ble_on_secs  = 0;
static __xdata bool     ble_powered  = false;
static __xdata bool     ble_active   = false;  // true when UART activity (phone connected)
static __xdata uint8_t  cmd_buf[16];
static __xdata uint8_t  cmd_idx      = 0;

// ============================================================
// Control logic
// ============================================================
static void control_update(void) {
    if (op_mode == '1') out_state = 1;
    else if (op_mode == '0') out_state = 0;
    else {
        int16_t up = target + hyst/2, lo = target - hyst/2;
        if (current > up) out_state = 1;
        else if (current < lo) out_state = 0;
    }
    OUT_PIN = out_state;
}

// ============================================================
// Command parser (shared by USB CDC and BLE UART2)
// ============================================================
static void parse_cmd(uint8_t src) {  // src: 'U'=USB, 'B'=BLE
    cmd_buf[cmd_idx] = '\0';

    // !ISP → reboot to USB bootloader
    if (cmd_buf[0] == '!' && cmd_buf[1] == 'I' && cmd_buf[2] == 'S' && cmd_buf[3] == 'P') {
        reboot_to_isp();
        return;
    }

    if (cmd_buf[0] == 'T' && cmd_buf[1] == '=') {
        uint16_t v = 0;
        for (uint8_t i = 2; cmd_buf[i] >= '0' && cmd_buf[i] <= '9'; i++) v = v*10 + (cmd_buf[i]-'0');
        if (v <= 500) target = (int16_t)v;
    } else if (cmd_buf[0] == 'H' && cmd_buf[1] == '=') {
        uint16_t v = 0;
        for (uint8_t i = 2; cmd_buf[i] >= '0' && cmd_buf[i] <= '9'; i++) v = v*10 + (cmd_buf[i]-'0');
        if (v >= 1 && v <= 100) hyst = (int16_t)v;
    } else if (cmd_buf[0] == 'O' && cmd_buf[1] == '=') {
        op_mode = cmd_buf[2];
        if (op_mode == '1') { out_state = 1; OUT_PIN = 1; }
        if (op_mode == '0') { out_state = 0; OUT_PIN = 0; }
    } else if (cmd_buf[0] == 'S' && cmd_buf[1] == '=') {
        uint8_t h = (cmd_buf[2]-'0')*10 + (cmd_buf[3]-'0');
        uint8_t m = (cmd_buf[4]-'0')*10 + (cmd_buf[5]-'0');
        uint8_t s = (cmd_buf[6]-'0')*10 + (cmd_buf[7]-'0');
        rtc_set_time(h, m, s);
    } else if (cmd_buf[0] == 'R') {
        // Report will be sent by caller
    }
}

// ============================================================
// Format status string → buf
// ============================================================
static uint8_t fmt_status(char *buf) {
    uint8_t h, m, s, len = 0;
    rtc_read(&h, &m, &s);

    // "C=2475,O=0,M=A,TS=143025\r\n"
    buf[len++] = 'C'; buf[len++] = '=';
    if (current < 0) { buf[len++] = '-'; }
    buf[len++] = '0' + (current / 1000) % 10;
    buf[len++] = '0' + (current / 100) % 10;
    buf[len++] = '0' + (current / 10) % 10;
    buf[len++] = '0' + (current % 10);
    buf[len++] = ','; buf[len++] = 'O'; buf[len++] = '=';
    buf[len++] = '0' + out_state;
    buf[len++] = ','; buf[len++] = 'M'; buf[len++] = '=';
    buf[len++] = op_mode;
    buf[len++] = ','; buf[len++] = 'T'; buf[len++] = 'S'; buf[len++] = '=';
    buf[len++] = '0' + (h / 10);
    buf[len++] = '0' + (h % 10);
    buf[len++] = '0' + (m / 10);
    buf[len++] = '0' + (m % 10);
    buf[len++] = '0' + (s / 10);
    buf[len++] = '0' + (s % 10);
    buf[len++] = '\r'; buf[len++] = '\n';
    return len;
}

// ============================================================
// Process UART2 (BLE) RX
// ============================================================
static void ble_rx(void) {
    if (!uart2_rx_ready()) return;
    char c = uart2_getc();
    if (c == '\r' || c == '\n') {
        if (cmd_idx > 0) {
            parse_cmd('B');
            // Send response via BLE
            if (cmd_buf[0] == 'R' || cmd_buf[0] == 'T' || cmd_buf[0] == 'H'
                || cmd_buf[0] == 'O' || cmd_buf[0] == 'S') {
                char buf[32];
                uint8_t n = fmt_status(buf);
                for (uint8_t i = 0; i < n; i++) uart2_putc(buf[i]);
            }
            cmd_idx = 0;
        }
    } else if (cmd_idx < sizeof(cmd_buf) - 1) {
        cmd_buf[cmd_idx++] = c;
    }
}

// ============================================================
// Process USB CDC RX (data from EP2)
// ============================================================
static void cdc_process_rx(void) {
    if (cdc_rx_len == 0) return;
    for (uint8_t i = 0; i < cdc_rx_len; i++) {
        char c = cdc_rx_buf[i];
        if (c == '\r' || c == '\n') {
            if (cmd_idx > 0) {
                parse_cmd('U');
                // Send response via USB CDC
                char buf[32];
                uint8_t n = fmt_status(buf);
                usb_cdc_send((uint8_t __xdata *)buf, n);
                cmd_idx = 0;
            }
        } else if (cmd_idx < sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_idx++] = c;
        }
    }
    cdc_rx_len = 0;
}

// ============================================================
// JDY-23 power control (P-MOSFET high-side switch on P0.3)
//   P0.3=LOW  → AO3401 gate pulled LOW → P-MOSFET ON  → JDY-23 powered
//   P0.3=HIGH → AO3401 gate = VCC    → P-MOSFET OFF → JDY-23 0μA
// ============================================================
static void jdy_power_on(void) {
    JDY_PWR = 0;            // P-MOSFET ON
    delay_ms(350);          // JDY-23 boot time (~300ms)
}

static void jdy_power_off(void) {
    JDY_PWR = 1;            // P-MOSFET OFF → 0μA
}

// ============================================================
// Delay (active, init only)
// ============================================================
static void delay_ms(uint16_t ms) {
    while (ms--) { volatile uint16_t i; for (i = 0; i < 1500; i++); }
}

// ============================================================
// Main
// ============================================================
void main(void) {
    // GPIO init
    OUT_PIN  = 0;                           // MOSFET gate OFF
    JDY_PWR  = 1;                           // JDY-23 power OFF
    xwr(XA_P0M0, 0x0C); xwr(XA_P0M1, 0x00); // P0.2, P0.3 push-pull

    // Init all peripherals (JDY-23 is OFF, UART2 will be used later)
    uart2_init();
    adc_init();
    rtc_init();
    wdt_init();
    usb_init();
    EA = 1;

    while (1) {
        // ---- USB CDC commands (always available) ----
        cdc_process_rx();

        // ---- JDY-23 BLE power & broadcast management ----
        // Every REPORT_SEC seconds: power on JDY-23 briefly.
        // Update BLE device name with current temperature.
        // Phone sees "G:24.5C" when scanning — no connection needed.
        // Keep on for BLE_ON_SEC; extend if UART activity (phone connected).

        if (report_cnt == 0 && !ble_powered) {
            jdy_power_on();        // P-MOSFET ON → JDY-23 boots
            ble_powered = true;
            ble_on_secs = 0;
            ble_active  = false;

            // Update BLE advertising name with current temperature
            // AT+NAME=G:24.5C → phone scan shows "G:24.5C"
            uart2_puts("AT+NAME=G:");
            if (current < 0) { uart2_putc('-'); }
            uart2_putc('0' + (current / 1000) % 10);
            uart2_putc('0' + (current / 100) % 10);
            uart2_putc('.');
            uart2_putc('0' + (current / 10) % 10);
            uart2_putc('C');
            uart2_puts("\r\n");
        }

        // Process BLE commands (only when JDY-23 powered)
        if (ble_powered) {
            // Check if there's incoming UART data (phone connected & sent command)
            if (uart2_rx_ready()) {
                ble_active = true;  // Extend BLE ON time
            }
            ble_rx();
        }

        // Power off JDY-23 after window expires (unless phone is actively connected)
        if (ble_powered) {
            ble_on_secs += MEASURE_SEC;
            uint8_t limit = ble_active ? (BLE_ON_SEC + 25) : BLE_ON_SEC;
            if (ble_on_secs >= limit) {
                jdy_power_off();  // P-MOSFET OFF → JDY-23 0μA
                ble_powered = false;
            }
        }

        // ---- Deep sleep until next RTC alarm ----
        rtc_set_alarm(MEASURE_SEC);
        PCON |= 0x02;  // Power Down

        // ---- Woken by RTC alarm ----
        wdt_feed();
        rtc_alarm_clear();

        // Measurement
        uint16_t adc = adc_read(0);
        current = (current + ntc_to_temp(adc)) / 2;
        control_update();
        wdt_feed();

        // Periodic report
        report_cnt++;
        if (report_cnt >= REPORT_SEC / MEASURE_SEC) {
            report_cnt = 0;
            // USB CDC always reports
            char buf[32];
            uint8_t n = fmt_status(buf);
            usb_cdc_send((uint8_t __xdata *)buf, n);
        }
    }
}
