/*
 * Date/time handling
 *
 * Copyright (c) 2013 Michael Buesch <m@bues.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "datetime.h"


/* Convert a binary byte to a BCD value.
 * value: The binary value.
 * Returns the BCD value.
 */
static uint8_t byte_to_bcd(uint8_t value)
{
	uint8_t bcd;

	bcd = value % 10;
	value /= 10;
	bcd |= (value % 10) << 4;

	return bcd;
}

/* Convert a BCD value to a binary byte.
 * bcd: The BCD value.
 * Returns the binary value.
 */
static uint8_t bcd_to_byte(uint8_t bcd)
{
	uint8_t value;

	value = bcd & 0x0F;
	value += ((bcd >> 4) & 0x0F) * 10;

	return value;
}

/* Convert an rtc_time from binary to BCD.
 * bcd_time: The destination buffer.
 * time: The source buffer.
 */
void rtc_time_to_bcd(struct rtc_time *bcd_time,
		     const struct rtc_time *time)
{
	bcd_time->second = byte_to_bcd(time->second);
	bcd_time->minute = byte_to_bcd(time->minute);
	bcd_time->hour = byte_to_bcd(time->hour);
	bcd_time->day = byte_to_bcd(time->day);
	bcd_time->month = byte_to_bcd(time->month);
	bcd_time->year = byte_to_bcd(time->year);
	bcd_time->day_of_week = time->day_of_week;
}

/* Convert an rtc_time from BCD to binary.
 * time: The destination buffer.
 * bcd_time: The source buffer.
 */
void rtc_time_from_bcd(struct rtc_time *time,
		       const struct rtc_time *bcd_time)
{
	time->second = bcd_to_byte(bcd_time->second);
	time->minute = bcd_to_byte(bcd_time->minute);
	time->hour = bcd_to_byte(bcd_time->hour);
	time->day = bcd_to_byte(bcd_time->day);
	time->month = bcd_to_byte(bcd_time->month);
	time->year = bcd_to_byte(bcd_time->year);
	time->day_of_week = bcd_time->day_of_week;
}

/* Get the time-of-day from an rtc_time structure.
 * time: The RTC time.
 * Returns a time_of_day_t object.
 */
time_of_day_t rtc_get_time_of_day(const struct rtc_time *time)
{
	uint32_t seconds;

	/* Add the number of seconds. */
	seconds = time->second;
	seconds += (uint16_t)time->minute * 60;
	seconds += (uint32_t)time->hour * 60 * 60;

	/* Return the number of seconds, divided by two.
	 * Divide by two, because time_of_day_t is a double-second count.
	 */
	return seconds / 2;
}

/* Get the timestamp from an rtc_time structure.
 * time: The RTC time.
 * Returns a timestamp_t object.
 * See the docs of timestamp_t for documentation of the format.
 */
timestamp_t rtc_get_timestamp(const struct rtc_time *time)
{
	timestamp_t s = 0;

	s |= (timestamp_t)(clamp(time->second, 0, 59) & 0x3F) << 0;
	s |= (timestamp_t)(clamp(time->minute, 0, 59) & 0x3F) << 6;
	s |= (timestamp_t)(clamp(time->hour, 0, 23) & 0x1F) << 12;
	s |= (timestamp_t)(clamp(time->day, 0, 30) & 0x1F) << 17;
	s |= (timestamp_t)(clamp(time->month, 0, 11) & 0x0F) << 22;
	s |= (timestamp_t)(clamp(time->year, 0, 63) & 0x3F) << 26;

	return s;
}
