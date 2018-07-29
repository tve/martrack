Ocean Sports Tracker
====================

Real-time GPS tracker for surfskiing (or kayaking, paddling, rowing) with real-time display as well
as LoRa and LTE radios for safety.

See https://hackaday.io/project/159732-ocean-sports-fitness-tracker

Contents
========

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
============

- The code in general depends on TvE's fork of Jeeh: https://github.com/tve/jeeh, which is a library
  to access the HW of the STM32L0 chip used. The reason for using this lib is that it's very small
  and thus edit-compile-upload-test cycles are very fast (no upload wait).
- Everything is built using platform.io

License
=======

This repository carries an MIT License.

Copyright 2018 by Thorsten von Eicken

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
