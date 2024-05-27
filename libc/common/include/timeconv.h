#pragma once

#ifdef COMPILE_KERNEL
#include <common.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#define SECS_PER_DAY 86400

/*
 * A time value is in microseconds since 1601.
 */
struct ostime
{
    uint8_t sec;        // 0-59
    uint8_t min;        // 0-59
    uint8_t hour;       // 0-23
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    int year;           // full year, e.g. `2024`
    int microsec;
};

uint64_t TimeStructToValue(struct ostime t);
struct ostime TimeValueToStruct(uint64_t t);
uint64_t TimeValueToUnixTime(uint64_t t);
uint64_t UnixTimeToTimeValue(uint64_t t);

/**
 * Sunday = 1, Monday = 2, ..., Saturday = 6.
 */
int GetWeekday(uint64_t t);

extern int cumulative_days_in_months[13];