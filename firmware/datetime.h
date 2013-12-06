#ifndef DATETIME_H_
#define DATETIME_H_

#include "util.h"

#include <stdint.h>


/* RTC representation of date and time. */
struct rtc_time {
	/* The second. 0-59 */
	uint8_t second;
	/* The minute. 0-59 */
	uint8_t minute;
	/* The hour. 0-23 */
	uint8_t hour;
	/* The day. 0-30 */
	uint8_t day;
	/* The month. 0-11 */
	uint8_t month;
	/* The year. 0-99 */
	uint8_t year;
	/* The day of the week. 0-6 */
	uint8_t day_of_week;
};

/* Type representing the time of the day, in 2-second resolution.
 * This is a count of double-seconds since midnight.
 */
typedef uint16_t time_of_day_t;

/* Representation of a time range in a day.
 * It features a start time and an end time.
 */
struct time_of_day_range {
	/* The start time of the range. */
	time_of_day_t from;
	/* The end time of the range. */
	time_of_day_t to;
};

/* Timestamp type with second granularity.
 * For easy convertability from struct rtc_time, this type
 * stores most of the rtc_time fields in a packed format
 * instead of using a count of seconds since some point in time (epoch).
 *
 * The bit-allocation of the 32-bit timestamp_t type is:
 *
 * Bit 0 - 5:	Seconds
 *		Value range: 0-59
 * Bit 6 - 11:	Minutes
 *		Value range: 0-59
 * Bit 12 - 16:	Hours
 *		Value range: 0-23
 * Bit 17 - 21:	Days
 *		Value range: 0-30
 * Bit 22 - 25:	Months
 *		Value range: 0-11
 * Bit 26 - 31:	Years
 *		Value range: 0-63 (= 2000-2063)
 */
typedef uint32_t timestamp_t;


void rtc_time_to_bcd(struct rtc_time *bcd_time,
		     const struct rtc_time *time);

void rtc_time_from_bcd(struct rtc_time *time,
		       const struct rtc_time *bcd_time);

time_of_day_t rtc_get_time_of_day(const struct rtc_time *time);

/* Return 1, if time_a is after time_b.
 * Return 0 otherwise.
 */
static inline bool time_of_day_after(time_of_day_t time_a,
				     time_of_day_t time_b)
{
	return time_a > time_b;
}

/* Return 1, if time_a is before time_b.
 * Return 0 otherwise.
 */
static inline bool time_of_day_before(time_of_day_t time_a,
				      time_of_day_t time_b)
{
	return time_a < time_b;
}

timestamp_t rtc_get_timestamp(const struct rtc_time *time);

#endif /* DATETIME_H_ */
