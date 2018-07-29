// Copyright (c) 2018 by Thorsten von Eicken
//
// Test logger

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <string.h>
#include <jee/spi-flash.h>

#define NUM 50
typedef struct {
    uint32_t time;
    uint16_t filler[NUM];
} LogEntry;

#include "logger/logger.h"

// ===== GPIO pins and hardware peripherals

PinA<15> led;          // LED on pcb, active low

UartBufDev< PinA<9>, PinA<10>, 80 > console;              // usart1 on FTDI connector

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<15>, 0 > spiDisp;  // spi1 with display select
SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiRf;     // spi1 with radio select
SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinC<14>, 0 > spiFlash; // spi1 with flash select
SpiFlash< decltype(spiFlash) > emem;                      // external dataflash, W25Q128
Logger< decltype(emem) > logger;                          // logger going to external flash

// ===== Helper functions for peripherals

// Console

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// Set-up

void setup() {
    led.mode(Pinmode::out);
    led = 0;

    //int hz = 2100000;
    uint32_t hz = fullSpeedClock();
    console.init();
    console.baud(115200, hz);
    wait_ms(10);
    printf("\r\n===== logger test starting ====\r\n\n");
    wait_ms(100);

    spiDisp.init();
    spiRf.init();

    spiFlash.init();
    emem.init();
#if 0
    logger.eraseAll();
#else
    for (int i=0; i<10; i++) emem.erase(i<<12);
    EEPROM::write32(32+0, 0);
    EEPROM::write32(32+4, 0);
    EEPROM::write32(32+8, 0);
#endif
    bool lOk = logger.init(32);
    printf("Logger init: %s\r\n", lOk ? "OK" : "ERR");
}

void printState() {
    printf("Logger state: size=%d count=%d free=%d first=%d next=%d\r\n",
            logger.total, logger.count(), logger.size()-logger.count(), logger.first, logger.next);
}

void printLE(LogEntry &le) {
        printf("@%d [", le.time);
        for (int j=0; j<NUM; j++) printf(" %d", le.filler[j]);
        printf("]");
}

bool checkLE(int i, LogEntry &le) {
    if (le.time != (uint32_t)i) return false;
    for (int j=0; j<NUM; j++) if (le.filler[j] != i+j) return false;
    return true;
}

int main () {
    setup();

    printf("===== Starting main loop\r\n");
    printState();
    printf("LogEntry: %dbytes\r\n\n", sizeof(LogEntry));

    for (int k=0; k<12; k++) {
        printf("\n== Write/read loop %d starting\r\n", k);
        for (int i=0; i<49; i++) {
            LogEntry le;
            le.time = i;
            for (int j=0; j<NUM; j++) le.filler[j] = i+j;
            //printf("NE: "); printLE(le); printf("\r\n");
            //printf("Writing %d\r\n", i);
            logger.pushEntry(le);
            memset(&le, 0, sizeof(le));
            bool feOk = logger.firstEntry(&le);
            if (!feOk || !checkLE(0, le)) {
                printf("ERR: "); printLE(le); printf(" %s\r\n\n", feOk ? "OK" : "ERR");
            }
        }
        printState();

        for (int i=0; i<49; i++) {
            LogEntry le;
            bool feOk = logger.firstEntry(&le);
            if (!feOk || !checkLE(i, le)) {
                printf("ERR[%d]: ", i); printLE(le); printf(" %s\r\n\n", feOk ? "OK" : "ERR");
            }
            logger.shiftEntry();
        }
        printState();
        wait_ms(1000);
    }

    int sz = logger.size();
    printf("\n== Writing %d entries\r\n", sz);
    EEPROM::write32(32+8, 0);
    logger.init(32);
    printState();
    for (int i=0; i<sz; i++) {
        LogEntry le;
        le.time = i;
        for (int j=0; j<NUM; j++) le.filler[j] = i+j;
        logger.pushEntry(le);
    }
    printState();
    printf("Reading back %d entries\r\n", sz);
    for (int i=0; i<sz; i++) {
        LogEntry le;
        bool feOk = logger.firstEntry(&le);
        if (!feOk || !checkLE(i, le)) {
            printf("ERR[%d]: ", i); printLE(le); printf(" %s\r\n\n", feOk ? "OK" : "ERR");
        }
        logger.shiftEntry();
    }
    printState();
    wait_ms(1000);

    printf("\r\n===== The end\r\n");
    while (true) ;
}
