// Copyright (c) 2018 by Thorsten von Eicken
//
// Test RF performance

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <string.h>
#include <jee/spi-rf96lora.h>
#include <jee/i2c-ssd1306.h>
#include <jee/text-font.cpp>
#include <jee/text-font.h>

LoRaConfig &lora_conf = lora_bw125cr47sf10;

// ===== GPIO pins and hardware peripherals

PinA<15> led;          // LED on pcb, active low

UartBufDev< PinA<9>, PinA<10>, 80 > console;              // usart1 on FTDI connector

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiRf;   // spi1 with radio select
RF96lora< decltype(spiRf) > radio;                        // radio driver

I2cBus< PinB<7>, PinB<6> > i2c;                           // standard I2C pins for SDA and SCL
SSD1306< decltype(i2c) > oled;
Font5x7< decltype(oled) > oled_txt;

// ===== Helper functions for peripherals

// Console

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

// Display

int oled_printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(oled_txt.putc, fmt, ap); va_end(ap);
    return 0;
}

// Set-up

static int freq[] = { 432600, 433600 };

void setup() {
    led.mode(Pinmode::out);
    led = 0;

    //i2c.init();
    oled.init();
    oled.clear();

    //int hz = 2100000;
    uint32_t hz = fullSpeedClock();
    console.init();
    console.baud(115200, hz);
    wait_ms(10);
    printf("\r\n===== RF test starting ====\r\n\n");
    oled_printf("***  RF test  ***\n*****************\nloc/rem  loc/rem");
    wait_ms(100);

    printf("LoRa radio =====\r\n");
    spiRf.init();
    radio.init(61, 0xcb, lora_conf, 432600);
    radio.txPower(12); // <================================== !
    printf("Noise: %ddB\r\n", radio.noiseFloor());
    PinA<7>::mode(Pinmode::out_2mhz);
    PinA<5>::mode(Pinmode::out_2mhz);
    PinA<4>::mode(Pinmode::out_2mhz);
}

uint16_t seq = 0;
int16_t noise = -100;

constexpr int N = 20;
int count = 0;
int32_t rmargin, lmargin, lnoise, rrssi, lrssi;

uint8_t s1=0, s2=0;
const uint8_t spinner[] = {'/', '~', '\\', '|'};

uint8_t toggle = 0;

void loop() {
    noise = (noise*3 + radio.noiseFloor() - 2) / 4; // -2 for rounding


    uint8_t packet[5];
    uint8_t cnt = 5;
    packet[0] = 0x80 + 5; // ? packet type
    packet[1] = seq&0xff;
    packet[2] = seq>>8;
    radio.addInfo(packet+3);
    uint8_t hdr = (1<<5) + 5; // request ack, we're node 5
    radio.send(hdr, packet, cnt);
    printf("sent %d %dB @%ddBm... ", toggle, cnt, radio.txpow);

    uint8_t ackBuf[10];
    int ack;
    if (toggle) {
        while (radio.mode != 6) radio.getAck(ackBuf, 10);
        wait_ms(400);
        while ((ack = radio.getAck(ackBuf, 10)) < 0) ;
    } else {
        while ((ack = radio.getAck(ackBuf, 10)) < 0) ;
    }

    if (ack >= 4) {
        int fei = 128 * (int)(int8_t)(ackBuf[ack-1]);
        int margin = ackBuf[ack-2] & 0x3f;
        int rssi = ack > 4 ? -(int)ackBuf[2] : 0;
        printf("ACK from %x: %ddB (%ddBm) %dHz, local RX %ddB %ddBm/%ddBm %dHz, SNR: %d/%d, corr %dHz\r\n",
            ackBuf[0]&0x1f, margin, rssi, fei, radio.margin, radio.rssi, noise, radio.fei,
            radio.snr, radio.lna, radio.actFreq-radio.nomFreq);
        oled_txt.y = 8; oled_txt.x = 115;
        oled_printf("%c", spinner[s1++]);
        oled_txt.y = 28 + 8*toggle; oled_txt.x = 0;
        oled_printf("%2d/%2ddB %d/%ddBm", radio.margin, margin, radio.rssi, rssi);
        if (s1 > 3) s1=0;

        rmargin += margin;
        lmargin += radio.margin;
        lnoise += noise;
        rrssi += rssi;
        lrssi += radio.rssi;
        count++;

        if (count == N) {
            printf("** remote: %ddB %ddBm, local %ddB %ddBm (%ddBm)\r\n",
                    rmargin/N, rrssi/N, lmargin/N, lrssi/N, lnoise/N);
            oled_txt.y = 48; oled_txt.x = 0;
            oled_printf("%2ddB %2ddB %ddB %c", lmargin/N, rmargin/N, lnoise/N, spinner[s2++]);
            if (s2 > 3) s2 = 0;
            rmargin = lmargin = lnoise = rrssi = lrssi = 0;
            count = 0;
        }
        //printf("noise: %ddBm\r\n", radio.noiseFloor());
        led = 1-led;
    } else {
        printf(" no ACK\r\n");
    }

    toggle = 1 - toggle;
    //radio.frequency(freq[toggle]);
    wait_ms(10);
}

int main () {
    setup();
    rmargin = lmargin = lnoise = rrssi = lrssi = 0;

    printf("===== Starting main loop\r\n");
    while (1) loop();
}
