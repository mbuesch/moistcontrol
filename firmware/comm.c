/*
 *  Host communication
 *
 *  Copyright (C) 2013 Michael Buesch <m@bues.ch>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "comm.h"

#include <string.h>

#include <util/crc16.h>
#include <avr/io.h>
#include <avr/cpufunc.h>


struct rx_context {
	struct comm_message queue[COMM_RX_QUEUE_SIZE];
	uint8_t in_ptr;
	uint8_t out_ptr;
	uint8_t count;
	uint8_t byte_ptr;
	uint16_t timeout;
};

struct tx_context {
	struct comm_message queue[COMM_TX_QUEUE_SIZE];
	uint8_t in_ptr;
	uint8_t out_ptr;
	uint8_t count;
	uint8_t byte_ptr;
	uint8_t seq_count;
};

static struct rx_context rx;
static struct tx_context tx;


static void comm_reset(void)
{
	memset(&rx, 0, sizeof(rx));
	memset(&tx, 0, sizeof(tx));
}

static inline uint16_t to_little_endian_16(uint16_t v)
{
	union {
		uint16_t le;
		uint8_t b[2];
	} u = {
		.b = { v & 0xFF, v >> 8, },
	};

	return u.le;
}

static comm_crc_t message_calc_crc(const struct comm_message *msg)
{
	const uint8_t *data = (const uint8_t *)msg;
	uint16_t crc = 0xFFFF, len;
	comm_crc_t ret;

	len = sizeof(*msg) - COMM_FCS_LEN;
	do {
		crc = _crc16_update(crc, *data++);
	} while (--len);
	crc ^= 0xFFFF;

	ret = to_little_endian_16(crc);

	return ret;
}

static void tx_try_put_next_byte(void)
{
	const struct comm_message *msg;
	const uint8_t *buf;
	uint8_t data;

	if (tx.count == 0)
		return;
	if (!(UCSRA & (1 << UDRE)))
		return;

	msg = &tx.queue[tx.out_ptr];
	buf = (const uint8_t *)msg;

	data = buf[tx.byte_ptr];
	tx.byte_ptr++;
	if (tx.byte_ptr >= sizeof(struct comm_message)) {
		tx.byte_ptr = 0;
		tx.out_ptr = (tx.out_ptr + 1) & COMM_TX_QUEUE_MASK;
		tx.count--;
		if (tx.count == 0)
			UCSRB &= ~(1 << UDRIE);
	}
	UDR = data;
}

ISR(USART_UDRE_vect)
{
	tx_try_put_next_byte();
}

void comm_drain_tx_queue(void)
{
	uint8_t sreg;

	sreg = irq_disable_save();
	while (tx.count)
		tx_try_put_next_byte();
	irq_restore(sreg);
}

/* Called with IRQs disabled. */
static void handle_tx_queue_overflow(struct comm_message *msg,
				     bool may_enable_irqs)
{
	/* TX queue is full. Notify the overflow condition
	 * to the serial control, once we get the message out. */
	comm_msg_set_err(msg, COMM_ERR_Q);
	msg->fcs = message_calc_crc(msg);

	/* Manually push TX to get things going. */
	do {
		tx_try_put_next_byte();
		if (may_enable_irqs) {
			/* IRQs were enabled before we were called.
			 * Be nice to other interrupts. */
			irq_enable();
			_NOP();
			irq_disable();
		}
	} while (tx.count >= COMM_TX_QUEUE_SIZE);
}

static uint8_t uart_rx(uint8_t *data_buf)
{
	uint8_t status, data;

	status = UCSRA;
	if (!(status & (1 << RXC)))
		return 0;
	data = UDR;
	if (data_buf)
		*data_buf = data;
	if (status & ((1 << FE) | (1 << PE) | (1 << DOR)))
		return 2;

	return 1;
}

void comm_message_send(struct comm_message *msg, uint8_t dest_addr)
{
	uint8_t sreg;

	comm_msg_set_da(msg, dest_addr);

	sreg = irq_disable_save();

	msg->seq = tx.seq_count++;
	msg->fcs = message_calc_crc(msg);

	if (tx.count >= COMM_TX_QUEUE_SIZE)
		handle_tx_queue_overflow(msg, __irqs_enabled(sreg));

	memcpy(&tx.queue[tx.in_ptr], msg, sizeof(*msg));
	tx.in_ptr = (tx.in_ptr + 1) & COMM_TX_QUEUE_MASK;
	tx.count++;

	UCSRB |= (1 << UDRIE);
	tx_try_put_next_byte();

	irq_restore(sreg);
}

static void handle_rx(const struct comm_message *msg)
{
	COMM_MSG(reply);
	comm_crc_t crc;
	bool ok;

	if (comm_msg_da(msg) != COMM_LOCAL_ADDRESS) {
		/* The message was not for us. */
		return;
	}

	crc = message_calc_crc(msg);
	if (crc != msg->fcs) {
		/* CRC mismatch. */
		comm_msg_set_err(&reply, COMM_ERR_FCS);
		goto ack;
	}

	if (msg->fc & COMM_FC_RESET) {
		comm_reset();
		comm_msg_set_err(&reply, COMM_ERR_OK);
		goto ack;
	}

	ok = comm_handle_rx_message(msg, comm_payload(&reply));
	if (!ok) {
		comm_msg_set_err(&reply, COMM_ERR_FAIL);
		goto ack;
	}

ack:
	if (msg->fc & COMM_FC_REQ_ACK) {
		reply.fc |= COMM_FC_ACK;
		comm_message_send(&reply, comm_msg_sa(msg));
	}
}

/* RX interrupt */
ISR(USART_RXC_vect)
{
	uint8_t res, data, *rxbuf;

	while (1) {
		res = uart_rx(&data);
		if (!res)
			return;

		if (rx.count >= COMM_RX_QUEUE_SIZE) {
			/* Queue overflow. */
			continue;//TODO
		}

		rxbuf = (uint8_t *)&rx.queue[rx.in_ptr];
		rxbuf[rx.byte_ptr] = data;
		rx.byte_ptr++;
		if (rx.byte_ptr >= sizeof(struct comm_message)) {
			rx.byte_ptr = 0;
			rx.in_ptr = (rx.in_ptr + 1) & COMM_RX_QUEUE_MASK;
			rx.timeout = 0;
			mb();
			rx.count++;
		}
	}
}

void comm_centisecond_tick(void)
{
	uint8_t sreg;

	sreg = irq_disable_save();

	if (rx.byte_ptr > 0)
		rx.timeout++;
	if (rx.timeout > 50 /* 0.5 seconds */) {
		/* Timeout! Reset the RX buffer. */
		rx.byte_ptr = 0;
		rx.timeout = 0;
	}

	irq_restore(sreg);
}

void comm_work(void)
{
	uint8_t sreg;

	mb();
	if (rx.count) {
		handle_rx(&rx.queue[rx.out_ptr]);
		rx.out_ptr = (rx.out_ptr + 1) & COMM_RX_QUEUE_MASK;

		sreg = irq_disable_save();
		rx.count--;
		irq_restore(sreg);
	}
}

#define USE_2X		(((uint64_t)F_CPU % (8ull * COMM_BAUDRATE)) < \
			 ((uint64_t)F_CPU % (16ull * COMM_BAUDRATE)))
#define UBRRVAL		((uint64_t)F_CPU / ((USE_2X ? 8ull : 16ull) * COMM_BAUDRATE))

static void uart_init(void)
{
	/* Set baud rate */
	UBRRL = UBRRVAL & 0xFF;
	UBRRH = (UBRRVAL >> 8) & 0xFF & ~(1 << URSEL);
	UCSRA = (!!(USE_2X) << U2X);
	/* 8 data bits, 1 stop bit, No parity */
	UCSRC = (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);
	/* Enable transceiver and RX IRQs */
	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);
	/* Drain the RX buffer */
	while (uart_rx(NULL))
		mb();
}

void comm_init(void)
{
	comm_reset();
	uart_init();
}
