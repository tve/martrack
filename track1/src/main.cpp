// Show a map of devices found on the I2C bus.

#include <jee.h>

UartDev< PinA<9>, PinA<10> > console;
UartDev< PinA<2>, PinA<3> > gps;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

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
    printf("\r\n===== marine tracker v1 starting ====\r\n\n");
    led.mode(Pinmode::out);
    led = 0;
    int hz = fullSpeedClock();
    console.baud(115200, hz);
    wait_ms(500);
    led = 1;

    detectI2c(bus);

    printf("printing GPS\r\n");

    // switch uart baud rate to 9600
    gps.baud(9600, hz);
    printf("gps.cr1=0x%x\r\n", MMIO32(gps.cr1));
    printf("gps.brr=%d\r\n", MMIO32(gps.brr));
    printf("console.brr=%d\r\n", MMIO32(console.brr));
    printf("ccipr=0x%x\r\n", MMIO32(Periph::rcc+0x4C));

    for (int i=0; i<20; i++) gps.putc('A');
    printf("printing GPS\r\n");

    while (1) {
        uint8_t ch = gps.getc();
        console.putc(ch);
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

