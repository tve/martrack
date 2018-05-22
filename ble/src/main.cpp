// Copyright (c) 2018 by Thorsten von Eicken

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <jee/spi-st7565r.h>
#include <jee/text-font.h>

// Console

UartBufDev< PinA<9>, PinA<10>, 80 > console;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// Display

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiDisp;
PinB<0> dispDC;
PinB<1> dispReset;
ST7565R< decltype(spiDisp), decltype(dispDC) > disp;
Font5x7< decltype(disp) > text_disp;

int lcd_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(text_disp.putc, fmt, ap); va_end(ap);
    return 0;
}

// string equality
bool streq(const uint8_t s1[], const uint8_t s2[]) {
    while (*s1 == *s2) {
        if (*s1 == 0) return true;
        s1++; s2++;
    }
    return false;
}

// Bluetooth module uart

UartBufDev< PinA<2>, PinA<3>, 80 > ble_uart;
PinB<7> ble_reset;

int ble_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(ble_uart.putc, fmt, ap); va_end(ap);
    return 0;
}

uint8_t ble_buf[80];

uint32_t ble_get_resp() {
    uint32_t t = ticks;
    uint32_t t0 = t;
    uint32_t len = 0;
    uint32_t delay = 1000; // wait in ms for initial char
    while (ticks-t < delay && len < sizeof(ble_buf)) {
        if (ble_uart.readable()) {
            ble_buf[len++] = ble_uart.getc();
            t = ticks;
            delay = 10; // 10ms for subsequent chars
        }
    }
    ble_buf[len] = 0;
    printf("... got %d %dms <%s>\r\n", len, ticks-t0, ble_buf);
    return len;
}

bool ble_try_connect() {
    while (ble_uart.readable()) ble_uart.getc();
    printf("BLE: AT");
    ble_printf("AT");
    ble_get_resp();
    if (!streq(ble_buf, (uint8_t*)&"OK")) return false;

    printf("BLE: version");
    ble_printf("AT+VERS?");
    ble_get_resp();
    return streq(ble_buf, (uint8_t*)&"OK+Get:HMSoft V121")
        || streq(ble_buf, (uint8_t*)&"HMSoft V604");
}

bool ble_connect_38400(uint32_t hz) {
    ble_reset = 0;
    ble_reset.mode(Pinmode::out);
    wait_ms(200);
    ble_reset = 1;
    wait_ms(1000);

    ble_uart.baud(38400, hz);
    if (ble_try_connect()) return true;
    if (ble_try_connect()) return true;
    printf("... trying 9600\r\n");
    wait_ms(200);
    ble_uart.baud(9600, hz);
    if (!ble_try_connect()) return false;
    printf("... switching to 38400\r\n");
    ble_printf("AT+BAUD2");
    uint32_t l = ble_get_resp();
    if (l != 8 || !streq(ble_buf, (uint8_t*)&"OK+Set:2")) return false;
    ble_printf("AT+RESET");
    wait_ms(200);
    while (ble_uart.readable()) ble_uart.getc();
    ble_uart.baud(38400, hz);
    for (int i=0; i<20; i++) {
        wait_ms(500);
        if (ble_try_connect()) return true;
    }
    return false;
}

void ble_factory_reset() {
    while (1) {
        printf("BLE: renew");
        ble_printf("AT+RENEW");
        ble_get_resp();
        if (streq(ble_buf, (uint8_t*)"OK+RENEW")) return;
        wait_ms(2000);
    }
}

void ble_info() {
    printf("== info\r\n");

    printf("BLE name: ");
    ble_printf("AT+NAME?");
    ble_get_resp();

    printf("BLE version: ");
    ble_printf("AT+VERR?");
    ble_get_resp();

    printf("BLE: op mode");
    ble_printf("AT+MODE?");
    ble_get_resp();

    printf("BLE: immediate mode");
    ble_printf("AT+IMME?");
    ble_get_resp();

    printf("BLE: notifications");
    ble_printf("AT+NOTI?");
    ble_get_resp();

    printf("BLE: master");
    ble_printf("AT+ROLE?");
    ble_get_resp();

    printf("BLE: notify mode");
    ble_printf("AT+NOTP?");
    ble_get_resp();

    printf("BLE: address");
    ble_printf("AT+ADDR?");
    ble_get_resp();

    printf("BLE: UUID");
    ble_printf("AT+UUID?");
    ble_get_resp();

    printf("BLE: CHAR");
    ble_printf("AT+CHAR?");
    ble_get_resp();

    printf("BLE: FFE2");
    ble_printf("AT+FFE2?");
    ble_get_resp();

    printf("BLE: discovery show");
    ble_printf("AT+SHOW?");
    ble_get_resp();

#if 0
    printf("BLE: power management");
    ble_printf("AT+PWRM?");
    ble_get_resp();

    printf("BLE: speed");
    ble_printf("AT+HIGH?");
    ble_get_resp();
#endif

    printf("BLE version: ");
    ble_printf("AT+VERR?");
    ble_get_resp();
}

bool ble_master() {
    uint32_t l;

    printf("== master\r\n");

    printf("BLE: set high speed");
    ble_printf("AT+HIGH1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    //if (l == 0) return 0;

    printf("BLE: set immediate mode");
    ble_printf("AT+IMME1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    printf("BLE: show names in disc");
    ble_printf("AT+SHOW1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    printf("BLE: enable notifications");
    ble_printf("AT+NOTI1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    printf("BLE: mode 0");
    ble_printf("AT+MODE0");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    printf("BLE: check master");
    ble_printf("AT+ROLE?");
    ble_get_resp();
    if (streq(ble_buf, (uint8_t*)&"OK+Get:1")) return true;

    printf("BLE: switching to master");
    ble_printf("AT+ROLE1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

#if 0
    printf("BLE: soft reset");
    ble_printf("AT+RESET");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;
#endif

    wait_ms(1000); // time for module to act on switch to master...
    for (int i=0; i<10; i++) {
        if (ble_try_connect()) break;
        wait_ms(500); // some more patience...
    }

    printf("BLE: check master");
    ble_printf("AT+ROLE?");
    l = ble_get_resp();
    return streq(ble_buf, (uint8_t*)&"OK+Get:1");
}

int ble_scan() {
    printf("== scan\r\n");

    printf("BLE: set 5 sec scan timeout");
    ble_printf("AT+SCAN5");
    ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    //if (l == 0) return 0;

    printf("BLE: scanning\r\n");
    ble_printf("AT+DISC?");
    for (int i=0; i<10; i++) {
        ble_get_resp();
        if (streq(ble_buf, (uint8_t*)&"OK+DISCE")) return 1;
        wait_ms(500);
    }
    return 0;
}

bool ble_set_uuids(uint16_t uuid, uint16_t charact) {
    printf("== set uuids\r\n");

    ble_printf("AT+COMP1");
    ble_get_resp();
    if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return false;

    ble_printf("AT+UUID0x%04x", uuid);
    int l = ble_get_resp();
    if (l != 13) return false;

    ble_printf("AT+CHAR0x%04x", charact);
    l = ble_get_resp();
    return l == 13;
}

bool ble_conn() {
    printf("== conn\r\n");

#if 1
    char *addr = "0022D0C01DCA"; // polar
#else
    char *addr = "F08BB25976B0"; // simulation
#endif
    printf("BLE: connect to %s", addr);
    ble_printf("AT+CON%s", addr);
    for (int i=0; i<10; i++) {
        uint32_t l = ble_get_resp();
        if (streq(ble_buf, (uint8_t*)&"OK+CONNA")) return true;
        if (l > 0) return false;
        wait_ms(500);
    }

    return 0;
}

int main () {
#if 0
    int hz = fullSpeedClock();
#else
    enableSysTick();
    int hz = 8000000;
#endif
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
    //wait_ms(2000);
    printf("display ready\r\n");
    lcd_printf("Hello world!\n============\n");

    printf("===== init BLE =====\r\n");
    ble_uart.init();
    while (1) {
#if 1
        if (ble_connect_38400(hz)) break;
        wait_ms(500);
        if (ble_connect_38400(hz)) break;
#else
        ble_uart.baud(9600, hz); wait_ms(10);
        if (ble_try_connect()) break;
        wait_ms(500);
        if (ble_try_connect()) break;
        wait_ms(500);
        if (ble_try_connect()) break;
#endif
        ble_factory_reset(); wait_ms(2000);
    }
    lcd_printf("BLE synced\n");

    printf("===== switching to master mode =====\r\n");
    ble_info();
    bool ok = ble_master();
    //if (ok) ble_info();
    ok = ble_set_uuids(0x180D, 0x2A37);
    wait_ms(500);

    //ble_scan();

    while (true) {
        while (!ble_conn()) wait_ms(1000);
        uint32_t t = ticks;
        do {
            int l = ble_get_resp();
            if (l > 0) {
                for (int i=0; i<l; i++) printf(" 0x%02x=%03d", ble_buf[i], ble_buf[i]);
                printf("\r\n");
                uint8_t flags = ble_buf[0];
                if (l > 1 && (flags == 0x16 || flags == 0x14)) {
                    // Heart rate flags look right
                    uint8_t hr = 0;
                    uint32_t rr = 0;
                    if ((flags&0x01) == 0) hr = ble_buf[1];
                    if ((flags&0x10) != 0 && l > 3) {
                        rr = ble_buf[2] | ((uint32_t)ble_buf[3]<<8);
                        rr = rr * 1000 / 1024;
                    }
                    printf("Heart rate: %d bpm, RR-interval: %d ms\r\n", hr, rr);
                    lcd_printf("HR: %d bpm\n", hr);
                }
            }
            if (streq(ble_buf, (uint8_t*)&"OK+LOST")) break;
        } while (ticks-t < 12000 && !streq(ble_buf, (uint8_t*)&"OK+CONNF"));
        while (true) {
            ble_printf("AT");
            ble_get_resp();
            if (streq(ble_buf, (uint8_t*)&"OK")) break;
            wait_ms(500);
        }
    }
}
