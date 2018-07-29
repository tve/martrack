// Copyright (c) 2018 by Thorsten von Eicken
//
// Marine tracker, v1

static constexpr int gps_tx_target = 10 * 1000; // target milliseconds between updates

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <jee/nmea.h>
#include <jee/spi-rf96lora.h>
#include <jee/varint.h>
#include <jee/spi-st7565r.h>
#include <jee/spi-flash.h>
#include "gps/track.h"
#include "gps/fence.h"

LoRaConfig &lora_conf = lora_bw125cr47sf10;

// ===== GPIO pins and hardware peripherals

PinA<1>  batPin;       // battery voltage divider (50% divider)
PinA<15> led;          // LED on pcb, active low
PinB<0>  dispReset;    // display and radio reset, active low
PinB<5>  dispDC;       // display data/command line

UartBufDev< PinA<9>, PinA<10>, 80 > console;              // usart1 on FTDI connector
UartBufDev< PinA<2>, PinA<3>, 200 > gps_uart;             // usart2 for GPS, intr-buffered
UartDev< PinA<2>, PinA<3> >         gps_uart_unbuf;       // usart2 for GPS, unbuffered for init
UartBufDev< PinB<3>, PinB<4>, 80 >  ble_uart;             // usart5 for bluetooth (BLE)

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<15>, 0 > spiDisp;  // spi1 with display select
SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiRf;     // spi1 with radio select
SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<14>, 0 > spiFlash; // spi1 with flash select
ST7565R< decltype(spiDisp), decltype(dispDC) > disp;      // display driver
RF96lora< decltype(spiRf) > radio;                        // radio driver
SpiFlash< decltype(spiFlash) > emem;                      // external dataflash, W25Q128

I2cBus< PinB<7>, PinB<6> > bus;                           // standard I2C pins for SDA and SCL

ADC<1> batVcc;                                            // ADC to measure battery voltage

// Logging
typedef struct {
    NMEAfix fix;
    uint8_t hr;
} LogEntry;
#include "logger/logger.h"
Logger< decltype(emem) > logger;                          // logger going to external flash

// ===== Helper functions for peripherals

// Console

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// BLE

#include "ser-hm11.h"

// Display and fonts

#if 1
// Using proportional fonts
#include <gfx/gfx.h>
#include <gfx/fonts/FreeSans10px7b.h>
#include <gfx/fonts/FreeSans16px7b.h>
#include <gfx/fonts/FreeSansBold16px7b.h>
#include <gfx/fonts/FreeSans20px7b.h>

template < typename DISP >
struct ST7565R_GFX : public GFXcanvas1<128, 64> {
    ST7565R_GFX() : GFXcanvas1() { fillScreen(0); };
    void display() {
        uint8_t band[128];
        uint8_t *buffer = getBuffer();
        for (int y=0; y<8; y++) {
          memset(band, 0, 128);
          for (int b=0; b<8; b++) {
              for (int x=0; x<128; x++) {
                  band[x] |= ((buffer[x>>3] >> (7-(x&7))) & 1) << b;
              }
              buffer += 128>>3;
          }
          DISP::copyBand(0, y*8, band, 128);
        }
        fillScreen(0);
    }
};

ST7565R_GFX< decltype(disp) > gfx;

int textWidth(const char *str) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
    return w;
}

static char str[16];
static int strpos;
static void strput(int ch) { str[strpos++] = ch; }
// returns text width in pixels
int sprintf(const char* fmt, ...) {
    strpos = 0;
    va_list ap; va_start(ap, fmt); veprintf(strput, fmt, ap); va_end(ap);
    str[strpos++] = 0;
    return textWidth(str);
}

#else
// Using simple tiny 5x7 font
#include <jee/text-font.h>
Font5x7< decltype(disp) > text_disp;

int lcd_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(text_disp.putc, fmt, ap); va_end(ap);
    return 0;
}

void lcd_xy(uint16_t x, uint16_t y) {
    text_disp.x = 6*x;
    text_disp.y = 8*y;
}
#endif

// GPS

static uint8_t gps_cksum;
static void gps_putc(int c) { gps_cksum ^= c; gps_uart.putc(c); };

int gps_printf(const char* fmt, ...) {
    gps_cksum = 0;
    gps_uart.putc('$');
    va_list ap; va_start(ap, fmt); veprintf(gps_putc, fmt, ap); va_end(ap);
    gps_uart.putc('*');
    int8_t d = gps_cksum/16; gps_uart.putc(d>9 ? 'A'+d-10 : '0'+d);
           d = gps_cksum%16; gps_uart.putc(d>9 ? 'A'+d-10 : '0'+d);
    gps_uart.putc('\r');
    gps_uart.putc('\n');
    return 0;
}

NMEA nmea;
Track track;

constexpr int nmea_vals = 10;
uint8_t nmea_packet[4+5*nmea_vals];

// nmeaMakePacket converts the NMEA info and heart rate into a varint-encoded packet.
// It returns the number of bytes placed into the buffer.
// Packet format:
// UTC date (DDMMYY), time (dHHMMSS, d=deciseconds), lat [deg*1E6], lon [deg*1E6], alt [m*10],
// horiz-speed [m/s*1E2], course [deg*1E2], sats, hdop [*1E2], hr
int nmeaMakePacket(NMEAfix &nmea, uint8_t hr, uint8_t *buf, int len) {
    int32_t time = nmea.time*100 + nmea.msecs/1000 + nmea.msecs/100%10*1000000;
    int32_t vals[nmea_vals] = { (int32_t)nmea.date, (int32_t)time, nmea.lat*5/3, nmea.lon*5/3,
        nmea.alt, nmea.knots*514/1000, nmea.course, nmea.sats, nmea.hdop, hr,
    };
    //for (int i=0; i<nmea_vals; i++) printf(" %d", vals[i]);
    //printf("\r\n");
    int cnt = encodeVarint(vals, nmea_vals, buf, len);
    //for (int i=0; i<cnt; i++) printf(" %02x", buf[i]);
    //printf("\r\n");
    decodeVarint(buf, cnt, vals, nmea_vals);
    //int c2 = decodeVarint(buf, cnt, vals, nmea_vals);
    //for (int i=0; i<c2; i++) printf(" %d", vals[i]);
    //printf("\r\n");
    return cnt;
}

// echoGPS reads from the GPS and echoes them to the console, until a \n is encountered or 200ms
// elapse.
void echoGPS() {
    uint8_t ch = 0;
    if (!gps_uart.readable()) wait_ms(200);
    do {
        while (!gps_uart.readable()) wait_ms(1);
        while (gps_uart.readable()) {
            ch = gps_uart.getc();
            console.putc(ch);
        }
    } while(ch != '\n');
}

// configGPS listens to the uart and switches it to 38400 baud if it's not there. It assumes that
// the GPS automatically starts sending NMEA stanzas.
void configGPS(uint32_t hz) {
    gps_uart_unbuf.init();
    int baud = 38400;
    // try to get in sync
    gps_uart_unbuf.baud(baud, hz);
    printf("GPS starting at 38400\r\n");
    do {
        if (gps_uart_unbuf.errored()) {
            baud = baud == 38400 ? 9600 : 38400;
            wait_ms(10);
            printf("GPS switching to %d\r\n", baud);
            gps_uart_unbuf.baud(baud, hz);
        }
    } while(!(gps_uart_unbuf.readable() && gps_uart_unbuf.getc() == '\n'));
    printf("GPS sync at %d\r\n", baud);
    // switch to 38400 if necessary
    if (baud != 38400) {
        printf("telling GPS to switch to 38400\r\n");
        static const char cmd[] = "$PMTK251,38400*27\r\n";
        for (uint32_t i=0; i<sizeof(cmd); i++) gps_uart_unbuf.putc(cmd[i]);
        echoGPS();
    }
    // init interrupt-driven buffered uart
    gps_uart.init();
    gps_uart.baud(38400, hz);
    echoGPS();
    // Query release version
    gps_printf("PMTK605"); printf("Release: "); echoGPS();
    // Only output GPRMC and GPGGA. The bits are:
    // GLL, RMC, VTG, GGA, GSA, GSV
    gps_printf("PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"); echoGPS();
    // Set pedestrian mode
    gps_printf("PMTK886,1"); echoGPS();
    // Set 4Hz reporting
    gps_printf("PMTK220,250"); echoGPS();
    // Query nav threshold
    gps_printf("PMTK447"); printf("Nav threshold: "); echoGPS();
}

// I2C

template< typename T >
void detectI2c (T bus) {
    for (int i = 0; i < 128; i += 16) {
        printf("%02x:", i);
        for (int j = 0; j < 16; ++j) {
            int addr = i + j;
            if (0x08 <= addr && addr <= 0x77) {
                bool ack = bus.start(addr<<1);
                bus.stop();
                printf(ack ? " %02x" : " --", addr);
            } else
                printf("   ");
        }
        printf("\r\n");
    }
}

// ADC

template< typename T >
static uint16_t avgAdc (T chan) {
    batVcc.read(chan);  // ignore first reading
    uint16_t sum = 0;
    for (int i = 0; i < 10; ++i)
        sum += batVcc.read(chan);
    return (sum + 5) / 10;
}

// batVoltage returns the battery voltage in mV
static int batVoltage () {
    return (avgAdc(batPin) * 6600) / 4095;  // result in mV, with 1:2 divider
}

// Misc

static const char spinner[] = "~\\|/";

void printSizes() {
    int16_t x1, y1;
    uint16_t w, h;

    gfx.setFont(&FreeSans10px7b);
    gfx.getTextBounds("00:00:00 0s", 0, 0, &x1, &y1, &w, &h);
    printf("FreeSans10px7b -- 00:00:00 0s : %d,%d %d,%d\r\n", x1, y1, w, h);

    gfx.setFont(&FreeSansBold16px7b);
    gfx.getTextBounds("100bpm", 0, 0, &x1, &y1, &w, &h);
    printf("FreeSansBold16px7b -- 100bpm : %d,%d %d,%d\r\n", x1, y1, w, h);
    gfx.getTextBounds("10.0kn", 0, 0, &x1, &y1, &w, &h);
    printf("FreeSansBold16px7b -- 10.0kn : %d,%d %d,%d\r\n", x1, y1, w, h);

    gfx.getTextBounds("300*", 0, 0, &x1, &y1, &w, &h);
    printf("FreeSans16px7b -- 300* : %d,%d %d,%d\r\n", x1, y1, w, h);

    wait_ms(5000);
}

void printLogger() {
    printf("Logger state: size=%d count=%d free=%d first=%d next=%d\r\n",
            logger.total, logger.count(), logger.size()-logger.count(), logger.first, logger.next);
}


void printGPS() {
    NMEAfix &fix = nmea.fix;
    printf("\r\n** 20%02d-%02d-%02d %02d:%02d:%02d.%03d\r\n",
            fix.date%100, fix.date/100%100, fix.date/10000,
            fix.time/100, fix.time%100, fix.msecs/1000, fix.msecs%1000);
    int32_t lat_int = fix.lat/600000;
    int32_t lat_frac = ((fix.lat>0?fix.lat:-fix.lat)%600000) * 5 / 3;
    int32_t lon_int = fix.lon/600000;
    int32_t lon_frac = ((fix.lon>0?fix.lon:-fix.lon)%600000) * 5 / 3;
    printf("   %d.%06dN/S %d.%06dE/W %d.%dm %d.%02dkn %d.%02ddeg\r\n",
            lat_int, lat_frac, lon_int, lon_frac, fix.alt/10, fix.alt%10,
            fix.knots/100, fix.knots%100, fix.course/100, fix.course%100);
    printf("   %d.%02dmph %d.%02ddeg %d:%02d\r\n",
            track.speed/447, track.speed*100/447%100, track.course/100, track.course%100,
            track.time/60, track.time%60);
    printf("   %d sats, HDOP:%d.%02d\r\n",
            fix.sats, fix.hdop/100, fix.hdop%100);
}

// Set-up

static const char msg[] = "MarTrack v0.3";
static int msgLen = 0;

void setup() {
    led.mode(Pinmode::out_2mhz);
    led = 0;

    //int hz = 2100000;
    uint32_t hz = fullSpeedClock();
    console.init();
    console.baud(115200, hz);
    wait_ms(10);
    printf("\r\n===== %s starting ====\r\n\n", msg);
    wait_ms(100);

    batPin.mode(Pinmode::in_analog);
    batVcc.init();

    // Init display and say hello
reinit:
    spiDisp.init(); // need to init all SPI devices before reset so selects are deasserted
    spiRf.init();
    spiFlash.init();
    dispReset = 0;
    dispReset.mode(Pinmode::out_2mhz); // also resets radio!
    wait_ms(10);
    dispReset = 1;
    wait_ms(10);
    disp.init();
    disp.clear();

    gfx.setFont(&FreeSans10px7b);
    msgLen = textWidth(msg);
    gfx.setCursor(127-msgLen, 63);
    gfx.printf(msg);
    gfx.display();
    wait_ms(100);
    printf("Display ready\r\n");
    //printSizes();

    printf("Logger =====\r\n");
    emem.init();
    bool lOk = logger.init(32);
    printf("Logger init: %s\r\n", lOk ? "OK" : "ERR");
    //logger.eraseAll();
    printLogger();

    printf("LoRa radio =====\r\n");
    if (!radio.init(61, 0xcb, lora_conf, 432600)) goto reinit;
    radio.txPower(20); // <================================== !
    printf("Noise: %ddB\r\n", radio.noiseFloor());

    // switch uart baud rate to 9600
    printf("setting up GPS =====\r\n");
    configGPS(hz);

    printf("Bluetooth =====\r\n");
    ble_init(hz);

    // reduce the slew rate on all the SPI pins because otherwise we take a 10dB hit
    // in LoRa RX sensitivity!
    PinA<7>::mode(Pinmode::out_2mhz);
    PinA<5>::mode(Pinmode::out_2mhz);
    PinA<4>::mode(Pinmode::out_2mhz);
    PinA<15>::mode(Pinmode::out_2mhz);
    PinB<0>::mode(Pinmode::out_2mhz);
    PinB<5>::mode(Pinmode::out_2mhz);
    PinC<14>::mode(Pinmode::out_2mhz);
    PinC<15>::mode(Pinmode::out_2mhz);

    //wait_ms(2000);
}

int main () {
    setup();

    printf("Starting main loop =====\r\n");

    uint32_t gps_tx_interval = gps_tx_target; // current interval in ms
    //uint32_t gps_tx_last = 0;                 // tick of last tx
    NMEAfix gps_fix;                          // last fix
    uint32_t gps_fix_last = 0;                // tick of last fix
    uint32_t gps_log_last = 0;                // tick of last time we logged GPS coords
    uint32_t disp_last = 0;                   // tick of last display
    uint8_t hr = 0;                           // current heart rate, 0 if none
    uint8_t hr_spin = 0;
    uint8_t gps_spin = 0;
    uint8_t rf_spin = 0;
    int8_t rx_margin = -100;
    int16_t gw_rssi = 0;
    int8_t gw_margin = -100;
    int16_t noise = radio.noiseFloor();
    uint8_t radioState = 0; // 0=idle, 1=busy

    constexpr int knotsNum = 10;      // number of readings to keep
    uint16_t knotsHist[knotsNum];     // last N readings for min/max
    for (int i=0; i<knotsNum; i++) knotsHist[i] = 0;

    bool txOn = false;

    while (1) {
        // Handle GPS
        while (gps_uart.readable()) {
            uint8_t ch = gps_uart.getc();
            //console.putc(ch); // echo GPS to console
            bool new_fix = nmea.parse(ch);
            if (new_fix && nmea.valid) {
                gps_fix = nmea.fix;
                gps_fix_last = ticks;

                if (ticks - gps_log_last > 950) {
                    gps_log_last = gps_fix_last;
                    track.addPoint(nmea.fix);
                    LogEntry le = { nmea.fix, hr };
                    logger.pushEntry(le);

                    for (int i=0; i<knotsNum-1; i++) knotsHist[i] = knotsHist[i+1];
                    knotsHist[knotsNum-1] = nmea.fix.knots;

                    // print to serial
                    printGPS();
                    if (++gps_spin >= sizeof(spinner)-1) gps_spin = 0;
                }
            }
        }
        if (ticks - gps_fix_last > 3000) {
            //printf("*** NO FIX\r\n");
            memset(&gps_fix, 0, sizeof(gps_fix));
        }

        // TX on radio
        if (logger.count() > 20) txOn = true;
        if (logger.count() == 0) txOn = false;
        txOn = true;
        if (txOn && radioState == 0 && logger.count() > 0) {
            uint8_t packet[64];
            packet[0] = 0x80 + 4; // gps packet type
            LogEntry le;
            if (!logger.firstEntry(&le)) continue;
            int cnt = nmeaMakePacket(le.fix, hr, packet+1, 120);
            radio.addInfo(packet+1+cnt);
            cnt += 3; // total length with packet type and 2 info bytes
            uint8_t hdr = (1<<5) + 4; // request ack, we're node 4
            radio.send(hdr, packet, cnt);
            radioState = 1;
            printf("** sent %d bytes\r\n", cnt);
        }

        // Check for ACK on radio
        if (radioState == 1) {
            uint8_t ackBuf[10];
            int ack = radio.getAck(ackBuf, 10);
            if (ack >= 0) {
                noise = radio.noiseFloor();
                radioState = 0;
                if (ack == 0) {
                    // no ACK, quickly retry if first failure, else exponential back-off
                    if (gps_tx_interval == gps_tx_target) gps_tx_interval /= 10;
                    //else gps_tx_interval *= 2;
                    if (gps_tx_interval > 10*gps_tx_target) gps_tx_interval = 10*gps_tx_target;
                    printf("** TIMEOUT, new interval=%dms\r\n", gps_tx_interval);
                } else {
                    // got ACK, use target interval
                    gps_tx_interval = gps_tx_target;
                    if (++rf_spin >= sizeof(spinner)-1) rf_spin = 0;
                    logger.shiftEntry();
                    if (logger.count() == 0) logger.save();
                }

                if (ack >= 3) {
                    int fei = 128 * (int)(int8_t)(ackBuf[ack-1]);
                    gw_margin = (int16_t)(ackBuf[ack-2] & 0x3f);
                    gw_rssi = ack > 3 ? -(int16_t)(ackBuf[ack-3]) : 0;
                    rx_margin = radio.margin;
                    printf("*** ACK from %x: %ddB (%ddBm) %dHz, local RX %ddB (%ddBm) %dHz, corr %dHz noise: %ddB\r\n",
                        ackBuf[0]&0x1f, gw_margin, gw_rssi, fei, rx_margin, radio.rssi, radio.fei,
                        radio.actFreq-radio.nomFreq, noise);
                    gfx.setFont(&FreeSans10px7b);
                    gfx.setCursor(0, 28);
                    gfx.printf("#");
                } else {
                    gw_rssi = 0;
                    gw_margin = -100;
                    rx_margin = -100;
                }
            }
        }

        // Handle BLE
        uint8_t new_hr = ble_heart_rate();
        if (new_hr != 0) hr = new_hr;

        // Display
        if (ticks - disp_last > 500) {
            int batV = batVoltage();
            // logo
            gfx.setFont(&FreeSans10px7b);
            gfx.setCursor(127-msgLen, 63);
            gfx.printf(msg);
            gfx.setCursor(0, 63);
            gfx.printf("%d.%02dV %c", batV/1000, batV%1000/10,
                spinner[ticks/500%(sizeof(spinner)-1)]);
            //printf("%d.%02dV %d\r\n", batV/1000, batV%1000/10, ticks/500%100);

            // heart rate
            gfx.setFont(&FreeSansBold16px7b);
            if (hr > 10) {
                int w = sprintf("%3d", hr);
                gfx.setCursor(31-w, 14);
                gfx.printf("%s", str);
                gfx.setFont(&FreeSans10px7b);
                gfx.setCursor(33, 13);
                gfx.printf("bpm %c", spinner[hr_spin]);
                if (++hr_spin >= sizeof(spinner)-1) hr_spin = 0;
            } else if (hr == 1) {
                gfx.setCursor(1, 14);
                gfx.printf("no skin");
            } else {
                gfx.setCursor(4, 14);
                gfx.printf("no hr");
            }

            // speed
            int mph = (int)gps_fix.knots * 1152 / 1000;
            gfx.setFont(&FreeSansBold16px7b);
            int w = sprintf("%2d.%1d", mph/100, mph/10%10);
            gfx.setCursor(96-w, 14);
            gfx.printf("%s", str);
            gfx.setFont(&FreeSans10px7b);
            gfx.setCursor(96, 13);
            gfx.printf("mph %c", spinner[gps_spin]);
            //printf("*** Display %d.%dmph %ddeg ticks=%d gps=%d %c\r\n",
            //        mph/100, mph/10%10, gps_fix.course/100, ticks, gps_fix_last, spinner[gps_spin]);

            // speed history
            uint16_t minKn=32000, maxKn=0;
            for (int i=0; i<knotsNum; i++) {
                if (knotsHist[i] < minKn) minKn = knotsHist[i];
                if (knotsHist[i] > maxKn) maxKn = knotsHist[i];
            }
            uint16_t minMph = minKn * 1152 / 1000;
            uint16_t maxMph = maxKn * 1152 / 1000;
            gfx.setCursor(64, 30);
            gfx.setFont(&FreeSans16px7b);
            if (minMph < 1000)
                gfx.printf("%d.%d-%d.%d", minMph/100, minMph/10%10, maxMph/100, maxMph/10%10);
            else
                gfx.printf("%d-%d.%d", minMph/100, maxMph/100, maxMph/10%10);

            // heading
            gfx.setFont(&FreeSans10px7b);
            w = sprintf("%d", gps_fix.course/100);
            gfx.setCursor(44-w, 53);
            gfx.printf("%sdeg", str);
            //gfx.setFont(&FreeSans10px7b);
            //gfx.setCursor(97, 29);
            //gfx.printf("deg");

            // time and sats
            gfx.setFont(&FreeSans10px7b);
            gfx.setCursor(0, 43);
            gfx.printf("%02d:%02d:%02d %dsat",
                (gps_fix.time/100+17)%24, gps_fix.time%100, gps_fix.msecs/1000, gps_fix.sats);

            // radio info
            gfx.setFont(&FreeSans10px7b);
            gfx.setCursor(7, 28);
            if (rx_margin != -100) {
                gfx.printf("%2d/%2ddB %c", rx_margin, gw_margin, spinner[rf_spin]);
            } else {
                gfx.printf("%4ddB %c", noise, spinner[rf_spin]);
            }

            // tracker info
            gfx.setFont(&FreeSans10px7b);
            gfx.setCursor(0, 53);
            gfx.printf("%dfl", logger.count());

            gfx.writeFastHLine(0,  0, 128, 1);
            gfx.writeFastHLine(0,  1, 128, 1);
            gfx.writeFastHLine(0, 16, 128, 1);
            gfx.writeFastHLine(0, 17, 128, 1);
            gfx.writeFastVLine(  0, 0, 16, 1);
            gfx.writeFastVLine( 62, 0, 64, 1);
            gfx.writeFastVLine(127, 0, 16, 1);
            gfx.writeFastHLine(0, 33, 128, 1);

            gfx.display();
            disp_last = ticks;
            led = 1-led;
        }

    }

}
