// Show a map of devices found on the I2C bus.

#include <jee.h>
#include <jee/nmea.h>

UartBufDev< PinA<9>, PinA<10> > console;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

#if 0
UartDev< PinA<2>, PinA<3> > gps_uart;
#else
UartBufDev< PinA<2>, PinA<3>, 85 > gps_uart;
#endif

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

I2cBus< PinB<7>, PinB<6> > bus;  // standard I2C pins for SDA and SCL
PinA<15> led;

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

int main () {
    console.init();
    printf("\r\n===== marine tracker v1 starting ====\r\n\n");
    led.mode(Pinmode::out);
    led = 0;
    int hz = fullSpeedClock();
    console.baud(115200, hz);
    wait_ms(500);
    led = 1;

    detectI2c(bus);

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

    printf("printing GPS\r\n");
    while (1) {
        uint8_t ch = gps_uart.getc();
        console.putc(ch);
        bool fix = nmea.parse(ch);
        if (fix) {
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
            printf("   %d sats, HDOP:%d.%02d~%dm\r\n",
                    nmea.sats, nmea.hdop/100, nmea.hdop%100, 3*nmea.hdop/100);
        }
    }

}

// sample output:
//
//  00:                         -- -- -- -- -- -- -- --
//  10: -- -- -- -- -- -- -- -- -- -- -- -- -- 1D -- --
//  20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//  30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//  40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//  50: 50 51 52 53 54 55 56 57 -- -- -- -- -- -- -- --
//  60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
//  70: -- -- -- -- -- -- -- --

