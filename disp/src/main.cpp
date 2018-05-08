// Copyright (c) 2018 by Thorsten von Eicken

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <jee/spi-st7565r.h>
#include <jee/text-font.h>

UartBufDev< PinA<9>, PinA<10> > console;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiDisp;
PinB<0> dispDC;
PinB<1> dispReset;
ST7565R< decltype(spiDisp), decltype(dispDC) > disp;
Font5x7< decltype(disp) > text_disp;

int lcd_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(text_disp.putc, fmt, ap); va_end(ap);
    return 0;
}

int main () {
    int hz = fullSpeedClock();
    console.init();
    //console.baud(115200, hz);
    printf("\r\n===== display tester starting ====\r\n\n");

    dispReset = 0;
    dispReset.mode(Pinmode::out);
    wait_ms(1);
    dispReset = 1;
    wait_ms(10);

    spiDisp.init();
    disp.init();
    disp.clear();
    wait_ms(2000);
    printf("display ready\r\n");

    // start printing incrementing integers
    int i = 0;
    while (true) {
        lcd_printf(" %d", ++i);
        wait_ms(100);
    }
}
