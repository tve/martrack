Simple Breadboard Marine Tracker
================================

V1 pinout
---------

Existing pinouts from older Forth-based tracker

Peripherals: radio, gps, imu, flash, led, ftdi, swd

PA0 - disconnected from pad for antenna GND (usart4 tx?)
PA1 - unused (usart4 rx?)
PA2 - GPS RX (usart2)
PA3 - GPS TX (usart2)
PA4 - SEL radio
PA5 - SCK radio, flash
PA6 - MISO radio, flash
PA7 - MOSI radio, flash
PA8 - DIO0 radio
PA9 - RX ftdi (usart1)
PA10 - TX ftdi (usart1)
PA11 - LED
PA12 - 
PA13 - SWD-IO
PA14 - SWD-CLK
PB0 - DIO2
PB1 - DIO3
PB3 - (usart5 tx)
PB4 - (usart5 rx)
PB5 - 
PB6 - SCL IMU
PB7 - SDA IMU
PC14 - flash mem SEL
PC15 -

V2 additions
------------

Desired additional peripherals:
- display, 4-wire SPI requires select and d/c
- cell radio, requires uart
- bluetooth, requires uart
- note: should have resets for radio, display, ble, cell

PA0 - unused          [cell radio rx (usart4 tx)]
PA1 - battery voltage [cell radio tx (usart4 rx?)]
PA2 - GPS RX (usart2)
PA3 - GPS TX (usart2)
PA4 - SEL radio
PA5 - SCK radio, flash, display
PA6 - MISO radio, flash, display
PA7 - MOSI radio, flash, display
PA8 - 
PA9 - RX ftdi (usart1)
PA10 - TX ftdi (usart1)
PA11 - LED
PA12 - 
PA13 - SWD-IO
PA14 - SWD-CLK
PB0 -  display reset
PB1 - 
PB3 - BLE rx (usart5 tx)
PB4 - BLE tx (usart5 rx)
PB5 - display d/c
PB6 - SCL IMU
PB7 - SDA IMU
PC14 - SEL flash mem
PC15 - SEL display
