/*
 * TWI-master Medium Access Control
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

#include "twi_master.h"
#include "twi_master_sync.h"
#include "util.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#include <string.h>


#ifndef TWI_SCL_HZ
# warning "No TWI_SCL_HZ defined. Defaulting to 100 KHz."
# define TWI_SCL_HZ	100000ul
#endif

#define TWI_SYNC


enum twi_transfer_status_flags {
	TWI_XFER_READ		= 0x80,

	TWI_XFER_STAT_MASK	= 0x0F,
	TWI_XFER_STAT_SHIFT	= 0,
};

struct twi_context {
	struct twi_transfer *first_xfer;
	struct twi_transfer *last_xfer;
};

static struct twi_context twi;


static inline void TWCR_write(uint8_t additional_flags)
{
	mb();
	TWCR = (1 << TWEN) | (1 << TWINT) |  additional_flags;
}

static void send_start_condition(void)
{
	/* Send start condition. */
	TWCR_write((1 << TWIE) | (1 << TWSTA));
}

static void send_stop_condition(void)
{
	/* Send stop condition. */
	TWCR_write(1 << TWSTO);
}

static void transfer_set_status(struct twi_transfer *xfer,
				enum twi_status status)
{
	xfer->status = (xfer->status & ~TWI_XFER_STAT_MASK) |
		       (status << TWI_XFER_STAT_SHIFT);
}

static enum twi_status transfer_get_status(const struct twi_transfer *xfer)
{
	return (xfer->status & TWI_XFER_STAT_MASK) >> TWI_XFER_STAT_SHIFT;
}

static void stop_transfer(struct twi_transfer *xfer,
			  enum twi_status new_status)
{
	send_stop_condition();

	transfer_set_status(xfer, new_status);

	twi.first_xfer = xfer->next;
	if (twi.first_xfer)
		send_start_condition();
	else
		twi.last_xfer = NULL;

	if (xfer->callback)
		xfer->callback(xfer, new_status);
}

static void handle_tw_status(struct twi_transfer *xfer, uint8_t twstat)
{
	uint8_t *buffer = xfer->buffer;

	switch (twstat) {
	default:
		stop_transfer(xfer, TWI_STAT_BUSERROR);
		break;
	case TW_START:
	case TW_REP_START:
		if (xfer->status & TWI_XFER_READ)
			TWDR = (xfer->address << 1) | 1;
		else
			TWDR = (xfer->address << 1);
		TWCR_write(1 << TWIE);
		break;
	case TW_MT_DATA_ACK:
	case TW_MR_DATA_ACK:
	case TW_MR_DATA_NACK:
	case TW_MT_SLA_ACK:
		if (xfer->status & TWI_XFER_READ) {
			buffer[xfer->offset++] = TWDR;

			if (xfer->offset == xfer->read_size) {
				stop_transfer(xfer, TWI_STAT_FINISHED);
			} else {
				if (xfer->offset + 1 == xfer->read_size)
					TWCR_write(1 << TWIE);
				else
					TWCR_write((1 << TWIE) | (1 << TWEA));
			}
		} else {
			if (xfer->offset == xfer->write_size) {
				if (xfer->read_size) {
					xfer->status |= TWI_XFER_READ;
					xfer->offset = 0;
					send_start_condition();
				} else
					stop_transfer(xfer, TWI_STAT_FINISHED);
			} else {
				TWDR = buffer[xfer->offset++];
				TWCR_write(1 << TWIE);
			}
		}
		break;
	case TW_MR_SLA_ACK:
		if (xfer->offset + 1 == xfer->read_size)
			TWCR_write(1 << TWIE);
		else
			TWCR_write((1 << TWIE) | (1 << TWEA));
		break;
	}
}

static void twi_interrupt_handler(void)
{
	struct twi_transfer *xfer;
	uint8_t twstat;

	mb();
	twstat = TW_STATUS;
	xfer = twi.first_xfer;
	if (xfer)
		handle_tw_status(xfer, twstat);
	mb();
}

ISR(TWI_vect)
{
	twi_interrupt_handler();
}

void twi_init(void)
{
#ifdef TWI_SYNC
	i2c_init();
#else
	memset(&twi, 0, sizeof(twi));
	TWSR = 0;
	TWBR = ((F_CPU / TWI_SCL_HZ) - 16) / 2;
	TWAR = 0;
#endif
}

void twi_transfer(struct twi_transfer *xfer)
{
#ifdef TWI_SYNC
	twi_size_t i;
	uint8_t *buffer = xfer->buffer;

	if (xfer->write_size) {
		i2c_start(xfer->address << 1);
		for (i = 0; i < xfer->write_size; i++)
			i2c_write(buffer[i]);
		if (!xfer->read_size)
			i2c_stop();
	}
	if (xfer->read_size) {
		i2c_start((xfer->address << 1) | 1);
		for (i = 0; i < xfer->read_size; i++) {
			if (i + 1 == xfer->read_size)
				buffer[i] = i2c_readNak();
			else
				buffer[i] = i2c_readAck();
		}
		i2c_stop();
	}
	if (xfer->callback)
		xfer->callback(xfer, TWI_STAT_FINISHED);

#else /* TWI_SYNC */

	uint8_t sreg;

	xfer->offset = 0;
	xfer->next = NULL;
	xfer->status = 0;
	if (!xfer->write_size)
		xfer->status |= TWI_XFER_READ;
	transfer_set_status(xfer, TWI_STAT_INPROGRESS);

	sreg = irq_disable_save();

	if (twi.last_xfer)
		twi.last_xfer->next = xfer;
	twi.last_xfer = xfer;
	if (!twi.first_xfer) {
		twi.first_xfer = xfer;
		send_start_condition();
	}
	irq_restore(sreg);
#endif
}

enum twi_status twi_transfer_get_status(const struct twi_transfer *xfer)
{
	enum twi_status status;
	uint8_t sreg;

#ifdef TWI_SYNC
	return TWI_STAT_FINISHED;
#endif

	sreg = irq_disable_save();
	status = transfer_get_status(xfer);
	irq_restore(sreg);

	return status;
}

enum twi_status twi_transfer_wait(struct twi_transfer *xfer,
				  uint16_t timeout_ms)
{
	enum twi_status status;
	uint32_t timeout = (uint32_t)timeout_ms * 100;

	while (1) {
		status = twi_transfer_get_status(xfer);
		if (status != TWI_STAT_INPROGRESS)
			break;
		if (timeout-- == 0) {
			twi_transfer_cancel(xfer);
			status = TWI_STAT_TIMEOUT;
			break;
		}
		_delay_us(10);
		if (!irqs_enabled()) {
			if (TWCR & (1 << TWINT))
				twi_interrupt_handler();
		}
	}

	return status;
}

void twi_transfer_cancel(struct twi_transfer *xfer)
{
	uint8_t sreg;

#ifdef TWI_SYNC
	return;
#endif

	sreg = irq_disable_save();
	if (transfer_get_status(xfer) == TWI_STAT_INPROGRESS)
		stop_transfer(xfer, TWI_STAT_CANCELLED);
	irq_restore(sreg);
}
