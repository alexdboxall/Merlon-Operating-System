#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * Sets the system's current local time. 
 * 
 * @param time The local time to set to, in microseconds since 
 *             1 January 1601, 12:00:00AM.
 * @return Zero on success, or an errno value if the time could not be set.
 */
int OsSetLocalTime(uint64_t time);

/**
 * @return The current local time, in microseconds since 
 *         1 January 1601, 12:00:00AM, or zero on error.
 */
uint64_t OsGetLocalTime(void);

/**
 * Sets the system's current timezone.
 * 
 * @param name The TZ identifier of the timezone to set.
 * @return Zero on success, or EINVAL if the timezone isn't supported, or
 *         another errno code.
 */
int OsSetTimezone(const char* name);

/**
 * Returns information about the system's current timezone.
 *
 * @param name_out   If not NULL, and if the length of the timezone string is 
 *                   less than `max_length`, then current timezone's name is 
 *                   written here.
 * @param max_length If `name_out` is not NULL, the call will fail with
 *                   ENAMETOOLONG if the string to be written is larger than
 *                   this value.
 * @param offset_out If not NULL, the timezone's offset from UTC in microseconds
 *                   will be written here.
 * @return Zero on success; or ENAMETOOLONG if name_out is not NULL, but the
 *         timezone name is larger than `max_length`; or another errno code.
 */
int OsGetTimezone(char* name_out, int max_length, uint64_t* offset_out);


/**
 * @return The current UTC time, in microseconds since 
 *         1 January 1601, 12:00:00AM, or zero on error.
 */
uint64_t OsGetUtcTime(void);
