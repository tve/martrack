// Copyright (c) 2018 by Thorsten von Eicken
//
// GPS track recorder and optimizer.

#include <jee/util-date.h>

struct Track {
    Track();
    void addPoint(NMEAfix &nmea);

    uint32_t speed; // speed in mm/s
    uint32_t course; // degrees*100
    uint32_t distance; // track distance in m
    uint32_t time; // track duration in seconds

    long start_t; // start time (secs since 1/1/2000
};

Track::Track() : speed(0), course(0), distance(0), time(0), start_t(0) {
}

void Track::addPoint(NMEAfix &nmea) {
    speed = (uint32_t)nmea.knots * 100000 / 19438; // knots -> mm/s (* 0.0514444)
    course = nmea.course;
    distance = 0;
    DateTime now = DateTime(
            nmea.date % 100, (uint8_t)(nmea.date / 10000), (uint8_t)(nmea.date / 100 % 100),
            (uint8_t)(nmea.time / 100), (uint8_t)(nmea.time % 100), (uint8_t)(nmea.msecs/1000));
    long now_t = now.get();
    if (start_t == 0) start_t = now_t;
    time = now_t - start_t;
}
