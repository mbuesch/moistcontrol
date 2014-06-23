/*
 * Logbuffer
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

#include "log.h"
#include "rv3029.h"

#include <string.h>


/* Size of the log ringbuffer, in number of elements. */
#define LOG_BUFFER_SIZE		32


/* Log buffer */
static struct log_item logbuf[LOG_BUFFER_SIZE];
/* Current number of elements in the log buffer. */
static uint8_t logbuf_nr_elems;
/* Write pointer into the log buffer. */
static uint8_t logbuf_write_ptr;
/* Read pointer into the log buffer. */
static uint8_t logbuf_read_ptr;
/* Overflow notification flag. */
static bool logbuf_overflow;


/* Initialize a log item.
 * This also writes the timestamp.
 * item: Pointer to the log item.
 * type: The type of the log item.
 */
void log_init(struct log_item *item, uint8_t type)
{
	struct rtc_time rtc;

	rv3029_get_time(&rtc);

	memset(item, 0, sizeof(*item));
	item->type_flags = type & LOG_TYPE_MASK;
	item->time = rtc_get_timestamp(&rtc);
}

/* Write/read pointer increment helper.
 * Honors the pointer wrapping.
 * ptr: Pointer to the write or read pointer variable.
 */
static void ptr_inc(uint8_t *ptr)
{
	/* Increment the pointer. */
	*ptr += 1;
	/* If it points beyond the log buffer, wrap to zero. */
	if (*ptr >= LOG_BUFFER_SIZE)
		*ptr = 0;
}

/* Append an item to the log buffer.
 * item: Pointer to the log item to add.
 */
void log_append(const struct log_item *item)
{
	uint8_t sreg;

	sreg = irq_disable_save();

	if (logbuf_nr_elems >= LOG_BUFFER_SIZE) {
		/* Overflow. Drop the oldest element */
		log_pop(NULL);
		logbuf_overflow = 1;
	}

	/* Copy the log item to the log item at
	 * the write pointer. */
	logbuf[logbuf_write_ptr] = *item;
	/* Increment the write pointer and increment the
	 * number of elements. */
	ptr_inc(&logbuf_write_ptr);
	logbuf_nr_elems++;

	irq_restore(sreg);
}

/* Pop the oldest item from the log buffer.
 * item: The destination buffer.
 * Returns 1, if the buffer was non-empty and an item was fetched.
 * Returns 0 otherwise.
 */
bool log_pop(struct log_item *item)
{
	uint8_t sreg;

	sreg = irq_disable_save();

	if (!logbuf_nr_elems) {
		/* Log buffer is empty. */
		irq_restore(sreg);
		return 0;
	}

	if (item) {
		/* Copy the item at read-pointer into
		 * the destination buffer. */
		*item = logbuf[logbuf_read_ptr];
		/* Add the overflow flag, if the log buffer had
		 * an overflow earlier. */
		if (logbuf_overflow)
			item->type_flags |= LOG_OVERFLOW;
		logbuf_overflow = 0;
	}

	/* Increment the read pointer and decrement the
	 * number of elements. */
	ptr_inc(&logbuf_read_ptr);
	logbuf_nr_elems--;

	irq_restore(sreg);

	return 1;
}
