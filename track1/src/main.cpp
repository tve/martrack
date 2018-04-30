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

int gps_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(gps_uart.putc, fmt, ap); va_end(ap);
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
    gps_printf("\r\n");
    gps_printf("$PMTK605*31\r\n");

    printf("printing GPS\r\n");
    while (1) {
        uint8_t ch = gps_uart.getc();
        console.putc(ch);
#if 1
        bool fix = nmea.parse(ch);
        if (fix) {
            printf("\r\n20%02d-%02d-%02d %02d:%02d:%02d.%03d %d.%04dN/S %d.%04dE/W %d.%02dkn %d.%02ddeg\r\n",
                    nmea.date%100, nmea.date/100%100, nmea.date/10000,
                    nmea.time/100, nmea.time%100, nmea.msecs/1000, nmea.msecs%1000,
                    nmea.lat/10000, nmea.lat%10000, nmea.lon/10000, nmea.lon%10000,
                    nmea.knots/100, nmea.knots%100, nmea.course/100, nmea.course%100);
        }
#endif
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

