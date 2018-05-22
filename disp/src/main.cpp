// Copyright (c) 2018 by Thorsten von Eicken

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <jee/spi-st7565r.h>
#include <jee/text-font.h>
#include <gfx/gfx.h>
#include <gfx/fonts/FreeSans9px7b.h>
#include <gfx/fonts/FreeSans10px7b.h>
#include <gfx/fonts/FreeSans14px7b.h>
#include <gfx/fonts/FreeSans16px7b.h>
#include <gfx/fonts/FreeSansBold14px7b.h>
#include <gfx/fonts/FreeSans20px7b.h>
#include <gfx/fonts/FreeSans24px7b.h>
#include <gfx/fonts/FreeSans30px7b.h>

// Console

UartBufDev< PinA<9>, PinA<10>, 80 > console;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// Display

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<15>, 0 > dispSpi;
PinB<5> dispDC;
PinB<0> dispReset;
ST7565R< decltype(dispSpi), decltype(dispDC) > disp;
//Font5x7< decltype(disp) > text_disp;
//GFXcanvas1<128, 64> gfx;

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

#if 0
int lcd_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(text_disp.putc, fmt, ap); va_end(ap);
    return 0;
}
#endif

int main () {
#if 1
    int hz = fullSpeedClock();
#else
    enableSysTick();
    int hz = 8000000;
#endif
    console.init();
    console.baud(115200, hz);
    printf("\r\n===== display tester starting ====\r\n\n");

    dispReset = 0;
    dispReset.mode(Pinmode::out);
    wait_ms(2);
    dispReset = 1;
    wait_ms(10);

    dispSpi.init();
    disp.init();
    disp.clear();
    //lcd_printf("Hello world!\n============\n");
    //wait_ms(2000);

    printf("display ready\r\n");

    gfx.setFont(&FreeSans9px7b);
    gfx.setCursor(0, 6);
    gfx.printf("Hello fox!(9)");

    gfx.setFont(&FreeSans10px7b);
    gfx.setCursor(0, 15);
    gfx.printf("Hello fox!(10)");

    gfx.setFont(&FreeSans14px7b);
    gfx.setCursor(0, 26);
    gfx.printf("Hello!(14)");

    gfx.setFont(&FreeSansBold14px7b);
    gfx.setCursor(64, 15);
    gfx.printf("Boldf(14)");

    gfx.setFont(&FreeSans16px7b);
    gfx.setCursor(0, 39);
    gfx.printf("Hello!(16)");

    gfx.setFont(&FreeSans20px7b);
    gfx.setCursor(0, 62);
    gfx.printf("20");

    gfx.setFont(&FreeSans24px7b);
    gfx.setCursor(30, 62);
    gfx.printf("24");

    gfx.setFont(&FreeSans30px7b);
    gfx.setCursor(70, 62);
    gfx.printf("30");

    gfx.display();

    printf("do you see something?\r\n");

    while(1) ;
}
