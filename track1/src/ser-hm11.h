// Copyright (c) 2018 by Thorsten von Eicken
// Driver for HM-11 bluetooth module to read "BLE-smart" heart rate monitor

#if 1
#define bledebugf printf
#else
#define bledebugf(a, ...) do {} while()
#endif

// string equality
bool streq(const uint8_t s1[], const uint8_t s2[]) {
    while (*s1 == *s2) {
        if (*s1 == 0) return true;
        s1++; s2++;
    }
    return false;
}

// string prefix equality
bool strneq(const uint8_t s1[], const uint8_t s2[], int n) {
    while (*s1 == *s2 && n-- > 0) {
        if (*s1 == 0) return true;
        if (n == 0) return true;
        s1++; s2++;
    }
    return false;
}

// Bluetooth module uart

//UartBufDev< PinB<3>, PinB<4>, 80 > ble_uart;
//PinB<7> ble_reset;

int ble_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(ble_uart.putc, fmt, ap); va_end(ap);
    return 0;
}

uint8_t ble_buf[40];

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
    bledebugf("... got %d %dms <%s>\r\n", len, ticks-t0, ble_buf);
    return len;
}

// ble_try_resp checks whether a character is available, and if so, it pull everything in that
// appears "consecutively" (with a small gap). This attempts to read one response or one
// notification from the BLE module.
int ble_try_resp() {
    if (!ble_uart.readable()) { return 0; }
    constexpr uint32_t gap_ms = 10; // max gap between chars
    uint32_t t = ticks;
    uint8_t len = 0;
    while (ticks-t < gap_ms && len < sizeof(ble_buf)) { // accumulate chars until gap
        if (ble_uart.readable()) {
            ble_buf[len++] = ble_uart.getc();
            t = ticks;
        }
    }
    ble_buf[len] = 0;
    //bledebugf("BLE[%d] <%s>\r\n", len, ble_buf);
    return len;
}

bool ble_try_connect() {
    while (ble_uart.readable()) ble_uart.getc();
    bledebugf("BLE: AT");
    ble_printf("AT");
    ble_get_resp();
    if (!streq(ble_buf, (uint8_t*)&"OK")) return false;

    bledebugf("BLE: version");
    ble_printf("AT+VERS?");
    ble_get_resp();
    return streq(ble_buf, (uint8_t*)&"OK+Get:HMSoft V121")
        || streq(ble_buf, (uint8_t*)&"HMSoft V604");
}

bool ble_connect_38400(uint32_t hz) {
#if 0
    ble_reset = 0;
    ble_reset.mode(Pinmode::out);
    wait_ms(200);
    ble_reset = 1;
    wait_ms(1000);
#endif

    ble_uart.baud(38400, hz);
    if (ble_try_connect()) return true;
    if (ble_try_connect()) return true;
    bledebugf("... trying 9600\r\n");
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
    bledebugf("== info\r\n");

    bledebugf("BLE name: ");
    ble_printf("AT+NAME?");
    ble_get_resp();

    bledebugf("BLE version: ");
    ble_printf("AT+VERR?");
    ble_get_resp();

    bledebugf("BLE: op mode");
    ble_printf("AT+MODE?");
    ble_get_resp();

    bledebugf("BLE: immediate mode");
    ble_printf("AT+IMME?");
    ble_get_resp();

    bledebugf("BLE: notifications");
    ble_printf("AT+NOTI?");
    ble_get_resp();

    bledebugf("BLE: master");
    ble_printf("AT+ROLE?");
    ble_get_resp();

    bledebugf("BLE: notify mode");
    ble_printf("AT+NOTP?");
    ble_get_resp();

    bledebugf("BLE: address");
    ble_printf("AT+ADDR?");
    ble_get_resp();

    bledebugf("BLE: UUID");
    ble_printf("AT+UUID?");
    ble_get_resp();

    bledebugf("BLE: CHAR");
    ble_printf("AT+CHAR?");
    ble_get_resp();

    bledebugf("BLE: FFE2");
    ble_printf("AT+FFE2?");
    ble_get_resp();

    bledebugf("BLE: discovery show");
    ble_printf("AT+SHOW?");
    ble_get_resp();

#if 0
    bledebugf("BLE: power management");
    ble_printf("AT+PWRM?");
    ble_get_resp();

    bledebugf("BLE: speed");
    ble_printf("AT+HIGH?");
    ble_get_resp();
#endif

    bledebugf("BLE version: ");
    ble_printf("AT+VERR?");
    ble_get_resp();
}

bool ble_master() {
    uint32_t l;

    bledebugf("== master\r\n");

    bledebugf("BLE: set high speed");
    ble_printf("AT+HIGH1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    //if (l == 0) return 0;

    bledebugf("BLE: set immediate mode");
    ble_printf("AT+IMME1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    bledebugf("BLE: show names in disc");
    ble_printf("AT+SHOW1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    bledebugf("BLE: enable notifications");
    ble_printf("AT+NOTI1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    bledebugf("BLE: mode 0");
    ble_printf("AT+MODE0");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

    bledebugf("BLE: check master");
    ble_printf("AT+ROLE?");
    ble_get_resp();
    if (streq(ble_buf, (uint8_t*)&"OK+Get:1")) return true;

    bledebugf("BLE: switching to master");
    ble_printf("AT+ROLE1");
    l = ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    if (l == 0) return 0;

#if 0
    bledebugf("BLE: soft reset");
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

    bledebugf("BLE: check master");
    ble_printf("AT+ROLE?");
    l = ble_get_resp();
    return streq(ble_buf, (uint8_t*)&"OK+Get:1");
}

int ble_scan() {
    bledebugf("== scan\r\n");

    bledebugf("BLE: set 5 sec scan timeout");
    ble_printf("AT+SCAN5");
    ble_get_resp();
    //if (!streq(ble_buf, (uint8_t*)&"OK+Set:1")) return 0;
    //if (l == 0) return 0;

    bledebugf("BLE: scanning\r\n");
    ble_printf("AT+DISC?");
    for (int i=0; i<10; i++) {
        ble_get_resp();
        if (streq(ble_buf, (uint8_t*)&"OK+DISCE")) return 1;
        wait_ms(500);
    }
    return 0;
}

bool ble_set_uuids(uint16_t uuid, uint16_t charact) {
    bledebugf("== set uuids\r\n");

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
    bledebugf("== conn\r\n");

#if 1
    const char *addr = "0022D0C01DCA"; // polar
#else
    const char *addr = "F08BB25976B0"; // simulation
#endif
    bledebugf("BLE: connect to %s", addr);
    ble_printf("AT+CON%s", addr);
    for (int i=0; i<10; i++) {
        uint32_t l = ble_get_resp();
        if (streq(ble_buf, (uint8_t*)&"OK+CONNA")) return true;
        if (l > 0) return false;
        wait_ms(500);
    }

    return 0;
}

void ble_init(uint32_t hz) {
    ble_uart.init();
    while (1) {
#if 1
        // connect and switch to 38400 baud if at 9600
        if (ble_connect_38400(hz)) break;
        wait_ms(500);
        if (ble_connect_38400(hz)) break;
#else
        // conservative: connect at 9600
        ble_uart.baud(9600, hz); wait_ms(10);
        if (ble_try_connect()) break;
        wait_ms(500);
        if (ble_try_connect()) break;
        wait_ms(500);
        if (ble_try_connect()) break;
#endif
        ble_factory_reset(); wait_ms(2000);
    }

    // switch to master mode
    //ble_info();
    ble_master();
    //if (ok) ble_info();
    ble_set_uuids(0x180D, 0x2A37);
    //wait_ms(500);
}

uint8_t ble_state = 0;
uint32_t ble_tick;
char ble_addr[] = "0022D0C01DCA"; // polar

uint8_t ble_heart_rate() {
    uint8_t start_state = ble_state;
    switch (ble_state) {
    case 0: // disconnected, need to send conn request
        ble_printf("AT+CON%s", ble_addr);
        ble_state=10;
        break;
    case 1: // sent "AT" to disconnect, wait for OK response then start over
        if (ble_try_resp() >= 2 && (
                streq(ble_buf, (uint8_t*)&"OK") ||
                streq(ble_buf, (uint8_t*)&"OK+LOST"))) {
            ble_state = 0;
        } else if (ticks-ble_tick > 500) ble_state = 0;
        break;
    case 10: // conn request sent, awaiting resp
        if (ble_try_resp() == 0) {
            if (ticks-ble_tick > 5*1000) { // timeout
                ble_printf("AT"); // make module abort what it's doing
                ble_state = 1;
            }
        } else if (streq(ble_buf, (uint8_t*)&"OK+CONNF") || // connection failed
                streq(ble_buf, (uint8_t*)&"OK+LOST")) { // connection closed
            ble_state = 0;
        } else if (streq(ble_buf, (uint8_t*)&"OK+CONNA")) { // connection started
            // no-op
        } else if (strneq(ble_buf, (uint8_t*)&"OK+CONN", 7)) { // got connected
            // sometimes the first data packet gets glommed onto the OK_CONN, sigh
            printf("BLE conn in %dms\r\n", ticks-ble_tick);
            ble_state++;
        }
        break;
    case 11: // connected, awaiting HR notif
        if (ble_try_resp() > 0) {
            if (streq(ble_buf, (uint8_t*)&"OK+LOST")) { // connection closed
                ble_state = 0;
            } else if (ble_buf[0] == 0x16) { // got heart rate measurement
                ble_tick = ticks; // reset timeout
                printf("BLE hr %d\r\n", ble_buf[1]);
                return ble_buf[1]; // heart rate comes as a uint8_t
#if 0           // code for rr-interval
                uint32_t rr = 0;
                if ((flags&0x10) != 0 && l > 3) {
                    rr = ble_buf[2] | ((uint32_t)ble_buf[3]<<8);
                    rr = rr * 1000 / 1024;
                }
#endif
            } else if (ble_buf[0] == 0x06) { // no skin contact
                ble_tick = ticks; // reset timeout
                printf("BLE: no skin contact\r\n");
                return 1; // poor man's way to signal no skin contact...
            } else if (ble_buf[0] == 'O') { // got come other status change?
                printf("BLE got <%s> while conn\r\n", ble_buf);
            } else {
                printf("BLE got %02x %02x %02x\r\n", ble_buf[0], ble_buf[1], ble_buf[2]);
            }
        } else if (ticks-ble_tick > 5000) {
            ble_printf("AT");
            ble_state = 1;
        }
        break;
    }
    if (start_state != ble_state) {
        //printf("BLE: %d->%d\r\n", start_state, ble_state);
        ble_tick = ticks; // record when we entered a new state
    }
    return 0;
}

#if 0
    printf("===== switching to master mode =====\r\n");

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
#endif
