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
#include <jee/text-font.h>
#include "ser-hm11.h"

// Console

UartBufDev< PinA<9>, PinA<10>, 80 > console;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// Display

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<15>, 0 > spiDisp;
PinB<5> dispDC;
PinB<0> dispReset;
ST7565R< decltype(spiDisp), decltype(dispDC) > disp;
Font5x7< decltype(disp) > text_disp;

int lcd_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(text_disp.putc, fmt, ap); va_end(ap);
    return 0;
}

void lcd_xy(uint16_t x, uint16_t y) {
    text_disp.x = 6*x;
    text_disp.y = 8*y;
}

// GPS

UartBufDev< PinA<2>, PinA<3>, 85 > gps_uart;

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

// I2C

I2cBus< PinB<7>, PinB<6> > bus;  // standard I2C pins for SDA and SCL
PinA<15> led;

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiRf;
RF96lora< decltype(spiRf) > radio;

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

constexpr int nmea_vals = 9;
uint8_t nmea_packet[4+5*nmea_vals];

// nmeaMakePacket converts the NMEA info into a varint-encoded packet, returns bytes placed into
// buffer.  Format:
// UTC date (DDMMYY), time (dHHMMSS, d=deciseconds), lat [deg*1E6], lon [deg*1E6], alt [m*10],
// horiz-speed [m/s*1E2], course [deg*1E2], sats, hdop [*1E2]
int nmeaMakePacket(NMEA &nmea, uint8_t *buf, int len) {
    int32_t time = nmea.time*100 + nmea.msecs/1000 + nmea.msecs/100%10*1000000;
    int32_t vals[nmea_vals] = { (int32_t)nmea.date, (int32_t)time, nmea.lat*5/3, nmea.lon*5/3,
        nmea.alt, nmea.knots*514/1000, nmea.course,
        nmea.sats, nmea.hdop
    };
    for (int i=0; i<nmea_vals; i++) printf(" %d", vals[i]);
    printf("\r\n");
    int cnt = encodeVarint(vals, nmea_vals, buf, len);
    for (int i=0; i<cnt; i++) printf(" %02x", buf[i]);
    printf("\r\n");
    int c2 = decodeVarint(buf, cnt, vals, nmea_vals);
    for (int i=0; i<c2; i++) printf(" %d", vals[i]);
    printf("\r\n");
    return cnt;
}

int main () {
    led.mode(Pinmode::out);
    led = 0;
    console.init();
    printf("\r\n===== marine tracker v0.2 starting ====\r\n\n");
    uint32_t hz = fullSpeedClock();
    console.baud(115200, hz);
    //wait_ms(500);

    // Init display and say hello
    dispReset = 0;
    dispReset.mode(Pinmode::out);
    wait_ms(1);
    dispReset = 1;
    wait_ms(10);
    spiDisp.init();
    disp.init();
    disp.clear();
    lcd_printf("Marine tracker v0.2\n");
    printf("Display ready\r\n");

    printf("I2C bus =====\r\n");
    detectI2c(bus);

    printf("LoRa radio =====\r\n");
    radio.init(61, 0xcb, lorawan_bw125sf8, 432600);
    radio.txPower(2); // <================================== !

    // switch uart baud rate to 9600
    printf("setting up GPS\r\n");
    gps_uart.init();
    gps_uart.baud(9600, hz);
    wait_ms(100);
    gps_printf("PMTK605");
    gps_printf("PMTK605");
    // Only output GPRMC and GPGGA. The bits are:
    // GLL, RMC, VTG, GGA, GSA, GSV
    gps_printf("PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");

    printf("Bluetooth =====\r\n");
    ble_init(hz);

    printf("Starting main loop =====\r\n");

    uint32_t gps_tx_interval = gps_tx_target; // current interval in ms
    uint32_t gps_tx_last = 0;                 // tick of last tx
    uint8_t hr_spin = 0;
    uint8_t gps_spin = 0;
    static const char spinner[] = "-\\|/";

    while (1) {
        // Handle GPS
        uint8_t ch = gps_uart.getc();
        //console.putc(ch);
        bool fix = nmea.parse(ch);
        if (fix && ticks > gps_tx_last+gps_tx_interval-50) {
            gps_tx_last = ticks;

            // print to serial
            printf("\r\n** 20%02d-%02d-%02d %02d:%02d:%02d.%03d\r\n",
                    nmea.date%100, nmea.date/100%100, nmea.date/10000,
                    nmea.time/100, nmea.time%100, nmea.msecs/1000, nmea.msecs%1000);
            int32_t lat_int = nmea.lat/600000;
            int32_t lat_frac = ((nmea.lat>0?nmea.lat:-nmea.lat)%600000) * 5 / 3;
            int32_t lon_int = nmea.lon/600000;
            int32_t lon_frac = ((nmea.lon>0?nmea.lon:-nmea.lon)%600000) * 5 / 3;
            printf("   %d.%06dN/S %d.%06dE/W %d.%dm %d.%02dkn %d.%02ddeg\r\n",
                    lat_int, lat_frac, lon_int, lon_frac, nmea.alt/10, nmea.alt%10,
                    nmea.knots/100, nmea.knots%100, nmea.course/100, nmea.course%100);
            printf("   %d sats, HDOP:%d.%02d\r\n",
                    nmea.sats, nmea.hdop/100, nmea.hdop%100);

            // display on LCD
            lcd_xy(0, 7);
            lcd_printf("%02d:%02d:%02d -- %d sats",
                nmea.time/100, nmea.time%100, nmea.msecs/1000, nmea.sats);
            lcd_xy(0, 5);
            lcd_printf("%d.%02dkn %d.%02ddeg %c",
                nmea.knots/100, nmea.knots%100, nmea.course/100, nmea.course%100,
                spinner[gps_spin++]);
            if (gps_spin >= sizeof(spinner)-1) gps_spin = 0;

            // TX on radio
#if 0
            uint8_t gpsPacket[128];
            gpsPacket[0] = 0x80 + 4; // gps packet type
            int cnt = nmeaMakePacket(nmea, gpsPacket+1, 120);
            radio.addInfo(gpsPacket+1+cnt);
            cnt += 3; // total length with packet type and 2 info bytes
            uint8_t hdr = (1<<5) + 4; // request ack, we're node 4
            radio.send(hdr, gpsPacket, cnt);
            int ack;
            uint8_t ackBuf[10];
            do {
                ack = radio.getAck(ackBuf, 10);
            } while (ack == -1);

            if (ack > 0) {
                // success, use target interval
                gps_tx_interval = gps_tx_target;
            } else {
                // no ACK, quickly retry if first failure, else exponential back-off
                if (gps_tx_interval == gps_tx_target) gps_tx_interval /= 10;
                else gps_tx_interval *= 2;
                if (gps_tx_interval > 10*gps_tx_target) gps_tx_interval = 10*gps_tx_target;
            }

            printf("** sent %d bytes, ack=%d, interval=%dms\r\n", cnt, ack, gps_tx_interval);
#endif
        }

        // Handle BLE
        uint8_t hr = ble_heart_rate();
        if (hr > 10) {
            lcd_xy(0, 3);
            lcd_printf("                   ");
            lcd_xy(0, 2);
            lcd_printf("HR %3d bpm %c", hr, spinner[hr_spin++]);
        } else if (hr == 1) {
            lcd_xy(0, 3);
            lcd_printf("no skin contact    ");
        }
        if (hr_spin >= sizeof(spinner)-1) hr_spin = 0;
    }

}
