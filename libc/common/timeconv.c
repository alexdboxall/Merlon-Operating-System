#include <timeconv.h>


/*
 * Note that these time conversions are not particually quick.
 */

#define SECS_PER_DAY 86400

static bool IsLeapYear(int year) {
	return (year % 4 == 0) && ((year % 100) != 0 || (year % 400) == 0);
}

static int cumulative_days_in_months[] = {
	0,															// 1 Jan
	31,															// 1 Feb
	31 + 28,													// 1 March
	31 + 28 + 31,												// 1 April
	31 + 28 + 31 + 30,											// 1 May
	31 + 28 + 31 + 30 + 31,										// 1 June
	31 + 28 + 31 + 30 + 31 + 30,								// 1 July
	31 + 28 + 31 + 30 + 31 + 30 + 31,							// 1 August
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,						// 1 September
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,					// 1 October
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,			// 1 November
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,		// 1 December
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31,	// Next year
};

static int GetFullLeapYearsSince1601(int year) {
	int leaps = 0;
	int i = 1604;

	while (i < year) {
		if (IsLeapYear(i)) {
			++leaps;
		}
		i += 4;
	}
	return leaps;
}

uint64_t TimeStructToValue(struct ostime t) {
	if (t.year < 1601 || t.year > 2400) {
		return 0;
	}
	if (t.month < 1 || t.month > 12) {
		return 0;
	}
	if (t.day < 1 || t.day > 31) {
		return 0;
	}

	uint64_t total_secs = t.sec;
	total_secs += t.min * 60;
	total_secs += t.hour * 3600;

	uint64_t days = t.day - 1;
	days += cumulative_days_in_months[t.month - 1];
	if (IsLeapYear(t.year) && t.month >= 3) {
		++days;
	}

	days += 365 * (t.year - 1601);
	days += GetFullLeapYearsSince1601(t.year);
	total_secs += days * SECS_PER_DAY;
	return total_secs * 1000000 + t.microsec;
}

struct ostime TimeValueToStruct(uint64_t t) {
	struct ostime res;
	res.microsec = t % 1000000;
	t /= 1000000;
	res.sec = t % 60;
	res.min = (t / 60) % 60;
	res.hour = (t / 3600) % 24;

	int days_left = t / SECS_PER_DAY;
	res.year = 1601;

	while (true) {
		if (res.year > 2400) {
			break;
		}
		res.month = 1;
		for (int i = 1; i <= 12; ++i) {
			int days = cumulative_days_in_months[i];
			int leap = i >= 2 && IsLeapYear(res.year);
			if (days_left >= days + leap) {
				res.month++;
			} else {
				int prior = cumulative_days_in_months[i - 1] + (i - 1 >= 2 && IsLeapYear(res.year));
				days_left -= prior;
				break;
			}
		}
		if (res.month == 13) {
			days_left -= IsLeapYear(res.year) ? 366 : 365; 
			res.year++;
		} else {
			break;
		}
	}

	res.day = days_left + 1;
	return res;
}

/**
 * Sunday = 1, Monday = 2, ..., Saturday = 6.
 */
int GetWeekday(uint64_t t) {
	int days_since_1601 = t / 1000000ULL / SECS_PER_DAY;

	/*
	 * 1 Janurary 1601 was a Monday.
	 */
	return (1 + days_since_1601) % 7 + 1;
}

#define SECS_BETWEEN_1970_AND_1601 11644473600ULL

uint64_t TimeValueToUnixTime(uint64_t t) {
	return (t / 1000000ULL) - SECS_BETWEEN_1970_AND_1601;
}

uint64_t UnixTimeToTimeValue(uint64_t t) {
	return (t + SECS_BETWEEN_1970_AND_1601) * 1000000ULL;
}
