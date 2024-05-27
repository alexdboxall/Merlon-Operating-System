#pragma once

#include <common.h>

/*
 * Although it's called RTC time, it can be used for anything.
 *
 * A time value is in microseconds since 1601.
 */
struct rtctime
{
    uint8_t sec;        // 0-59
    uint8_t min;        // 0-59
    uint8_t hour;       // 0-23
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    int year;           // full year, e.g. `2024`
};

uint64_t TimeStructToValue(struct rtctime t);
struct rtctime TimeValueToStruct(uint64_t t);