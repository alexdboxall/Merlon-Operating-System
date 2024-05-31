#include <machine/cmos.h>
#include <errno.h>
#include <timeconv.h>
#include <log.h>

#define RTC_SECOND  0x00
#define RTC_MINUTE  0x02
#define RTC_HOUR    0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09
#define RTC_WEEKDAY 0x06

#define CURRENT_YEAR 2024

static bool IsUpdateInProgress(void) {
    return ReadCmos(0x0A) & 0x80;
}

static uint8_t BcdToBinary(uint8_t val) {
    return (val & 0xF) + (val >> 4) * 10;
}

static uint8_t BinaryToBcd(uint8_t val) {
    return (val % 10) + ((val / 10) << 4);
}

static int Convert12HourTo24(int hour, bool pm) {
    if (hour == 12) {
        hour = 0;
    }
    if (pm) {
        hour += 12;
    }
    return hour;
}

static int Convert24HourTo12(int hour, bool* pm) {
    *pm = hour >= 12;
    hour %= 12;
    if (hour == 0) {
        hour = 12;
    }
    return hour;
}

/*
 * ACPI.SYS will set this with the correct value if one exists.
 */
int x86_rtc_century_register = -1;
static int GetCenturyRegister(void) {
    return x86_rtc_century_register;
}

static int GetCenturyGuess(int low_year) {
    int year = low_year + (CURRENT_YEAR / 100) * 100;
    if (year < CURRENT_YEAR) year += 100;
    return year / 100;
}

static int GetCentury(int low_year, bool rtc_in_bcd_mode) {
    int guess = GetCenturyGuess(low_year);

    int century_reg = GetCenturyRegister();
    if (century_reg == -1) {
        return guess;        

    } else {
        uint8_t century = ReadCmos(century_reg);
        if (rtc_in_bcd_mode) {
            century = BcdToBinary(century);
        }
        if (century != guess) {
            LogDeveloperWarning("RTC century (and maybe everything else) is probably wrong\n");
        }
        return century;
    }
}

static void SetCentury(int century, bool rtc_in_bcd_mode) {
    int century_reg = GetCenturyRegister();
    if (century_reg != -1) {
        if (rtc_in_bcd_mode) {
            century = BinaryToBcd(century);
        }
        WriteCmos(century_reg, century);
    }
}

static bool AreTimesEqual(struct ostime a, struct ostime b) {
    return a.sec == b.sec && a.min == b.min && a.hour == b.hour &&
           a.day == b.day && a.month == b.month && a.year == b.year;
}

static void ReadTimeState(struct ostime* t)
{
    t->microsec = 0;
    t->sec = ReadCmos(RTC_SECOND);
    t->min = ReadCmos(RTC_MINUTE);
    t->hour = ReadCmos(RTC_HOUR);
    t->day = ReadCmos(RTC_DAY);
    t->month = ReadCmos(RTC_MONTH);
    t->year = ReadCmos(RTC_YEAR);

    uint8_t reg_b = ReadCmos(0x0B);
    bool bcd = !(reg_b & 0x04);
    bool twelve_hour = !(reg_b & 0x02);
    if (bcd) {
        t->sec = BcdToBinary(t->sec);
        t->min = BcdToBinary(t->min);
        t->hour = BcdToBinary(t->hour & 0x7F);
        t->day = BcdToBinary(t->day);
        t->month = BcdToBinary(t->month);
        t->year = BcdToBinary(t->year);
    }
    if (twelve_hour) {
        t->hour = Convert12HourTo24(t->hour, t->hour & 0x80);
    }

    t->year += GetCentury(t->year, bcd) * 100;
}

static void WriteTimeState(struct ostime t)
{
    uint8_t reg_b = ReadCmos(0x0B);
    bool bcd = !(reg_b & 0x04);
    bool twelve_hour = !(reg_b & 0x02);
    bool pm = false;
    if (twelve_hour) {
        t.hour = Convert24HourTo12(t.hour, &pm);
    }
    SetCentury(t.year / 100, bcd);
    t.year %= 100;
    if (bcd) {
        t.sec = BinaryToBcd(t.sec);
        t.min = BinaryToBcd(t.min);
        t.hour = BinaryToBcd(t.hour);
        t.day = BinaryToBcd(t.day);
        t.month = BinaryToBcd(t.month);
        t.year = BinaryToBcd(t.year);
    }
    if (pm) {
        t.hour |= 0x80;
    }
    
    WriteCmos(RTC_SECOND, t.sec);
    WriteCmos(RTC_MINUTE, t.min);
    WriteCmos(RTC_HOUR, t.hour);
    WriteCmos(RTC_DAY, t.day);
    WriteCmos(RTC_MONTH, t.month);
    WriteCmos(RTC_YEAR, t.year);
}

uint64_t ArchGetUtcTime(int64_t timezone_offset) {    
    struct ostime time;
    struct ostime prev;

    while (IsUpdateInProgress()) {
        ;
    }
    ReadTimeState(&time);
    
    // TODO: timeouts!
    do {
        prev = time;
        while (IsUpdateInProgress()) {
            ;
        }
        ReadTimeState(&time);

    } while (!AreTimesEqual(time, prev));

    LogWriteSerial("RTC got %d:%d:%d %d/%d/%d\n", time.hour, time.min, time.sec, time.day, time.month, time.year);

    return TimeStructToValue(time) - timezone_offset;
}

int ArchSetUtcTime(uint64_t time, int64_t timezone_offset) {
    time += timezone_offset;
    
    struct ostime rtime = TimeValueToStruct(time);
    struct ostime readback;

    do {
        WriteTimeState(rtime);
        ReadTimeState(&readback);
    } while (!AreTimesEqual(rtime, readback));

    WriteCmos(RTC_WEEKDAY, GetWeekday(time));
    LogWriteSerial("writing weekday %d (1-7, Sunday=1)\n", GetWeekday(time));

    return 0;
}