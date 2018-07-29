Ocean Sports Tracker
====================

Real-time GPS tracker for surfskiing (or kayaking, paddling, rowing) with real-time display as well
as LoRa and LTE radios for safety.

See https://hackaday.io/project/159732-ocean-sports-fitness-tracker

Contents
--------

- track1 contains the main program for the first version, i.e. prototype, of the tracker.
- logger contains code to log trackpoints to an SPI flash chip and replay the log when there's
  connectivity.
- gps contains (as of yet unused) code to represent GPS tracks and simplify them.
- ble contains test code to use an HM-11 bluetoothe module to get heart-rate data from a polaris 7
  chest strap.
- disp contains test code for the 128x64 LCD
- gw contains a pseudo-LoRa GW that receives tracker packets and sends an ACK for the purpose of
  measuring the link performance (RSSI, SNR, ...).
- rf contains test code for the LoRa module.

Dependencies
------------

- The code in general depends on TvE's fork of Jeeh: https://github.com/tve/jeeh, which is a library
  to access the HW of the STM32L0 chip used. The reason for using this lib is that it's very small
  and thus edit-compile-upload-test cycles are very fast (no upload wait).
- Everything is built using platform.io
