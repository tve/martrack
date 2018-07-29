// Copyright (c) 2018 by Thorsten von Eicken
//
// Test GW to measure performance

int printf(const char* fmt, ...); // forward decl to allow .h files to print for debug

#include <jee.h>
#include <string.h>
#include <jee/spi-rf96lora.h>

LoRaConfig &lora_conf = lora_bw125cr47sf10;

// ===== GPIO pins and hardware peripherals

PinA<15> led;          // LED on pcb, active low

UartBufDev< PinA<9>, PinA<10>, 80 > console;              // usart1 on FTDI connector

SpiGpio< PinA<7>, PinA<6>, PinA<5>, PinA<4>, 0 > spiRf;   // spi1 with radio select
RF96lora< decltype(spiRf) > radio;                        // radio driver

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
    printf("\r\n===== Test GW starting ====\r\n\n");
    wait_ms(100);

    printf("LoRa radio =====\r\n");
    spiRf.init();
    radio.init(61, 0xcb, lora_conf, 433600);
    radio.txPower(12); // <================================== !
    printf("Noise: %ddB\r\n", radio.noiseFloor());
    led = 1;
}

int16_t noise = -100;

void loop() {
    uint8_t packet[256];
    int len = radio.receive(packet, sizeof(packet));
    if (len < 2) return; // nothing...

    int16_t r_margin = 0, r_fei = 0;
    if (len > 4 && packet[1] & 0x80) {
        r_margin = packet[len-2] & 0x3f;
        r_fei = packet[len-1]*128;
        radio.adjustPow(r_margin);
    }

    uint8_t hdr = 0xC0 | (packet[0]&0x1f); // ACK from GW
    packet[0] = 0x80; // packet type 0, got info trailer
    packet[1] = (uint8_t)(-radio.rssi);
    radio.addInfo(packet+2);
    radio.send(hdr, packet, 4);
    led = 1-led;

    printf("RX %2d from %2d (%02x) %ddB: local %ddB (%ddBm) %dHz, TX @%ddBm\r\n",
        len, packet[0]&0x1f, packet[0], r_margin, radio.margin, radio.rssi, radio.fei, radio.txpow);
}

int main () {
    setup();

    printf("===== Starting main loop\r\n");
    while (1) loop();
}

#if 0




    uint8_t cnt = 5;
    packet[0] = 0x80 + 5; // ? packet type
    packet[1] = seq&0xff;
    packet[2] = seq>>8;
    radio.addInfo(packet+3);
    uint8_t hdr = (1<<5) + 5; // request ack, we're node 5
    radio.send(hdr, packet, cnt);
    printf("sent %dB... ", cnt);

    uint8_t ackBuf[10];
    int ack;
    while ((ack = radio.getAck(ackBuf, 10)) < 0) ;

    noise = radio.noiseFloor();

    if (ack == 3) {
        int fei = 128 * (int)(int8_t)(ackBuf[2]);
        int snr = ackBuf[1] & 0x3f;
        uint8_t rx_budget = radio.linkBudget(radio.rssi, noise);
        printf("ACK from %x: %ddB %dHz, local RX %ddB/%ddB/%ddB %dHz, corr %dHz\r\n",
            ackBuf[0]&0x1f, snr, fei, rx_budget, radio.rssi, noise, radio.fei,
            radio.actFreq-radio.nomFreq);
        oled_txt.y = 8; oled_txt.x = 115;
        oled_printf("%c", spinner[s1++]);
        oled_txt.y = 32; oled_txt.x = 0;
        oled_printf("%2ddB %2d/%d/%ddB", snr, rx_budget, radio.rssi, noise);
        if (s1 > 3) s1=0;

        rsnr += snr;
        lsnr += rx_budget;
        lnoise += noise;
        count++;

        if (count == N) {
            printf("** remote: %ddB, local %ddB/%ddB\r\n", rsnr/N, lsnr/N, lnoise/N);
            oled_txt.y = 48; oled_txt.x = 0;
            oled_printf("%2ddB %2ddB %ddB %c", rsnr/N, lsnr/N, lnoise/N, spinner[s2++]);
            if (s2 > 3) s2 = 0;
            rsnr = lsnr = lnoise = 0;
            count = 0;
        }
    } else {
        printf(" no ACK\r\n");
    }

    wait_ms(10);
}

#endif
