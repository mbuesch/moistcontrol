/*
 * Moistcontrol
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

#include "main.h"
#include "comm.h"
#include "util.h"
#include "sensor.h"
#include "controller.h"
#include "log.h"
#include "twi_master.h"
#include "pcf8574.h"
#include "rv3029.h"
#include "notify_led.h"

#include <avr/io.h>
#include <avr/wdt.h>


/* RTC time fetch interval, in milliseconds. */
#define RTC_FETCH_INTERVAL_MS		1000


/* Message IDs of control messages transferred to and from
 * the host over serial wire. */
enum user_message_id {
	MSG_LOG,			/* Log message */
	MSG_LOG_FETCH,			/* Log message request */
	MSG_RTC,			/* RTC time */
	MSG_RTC_FETCH,			/* RTC time request */
	MSG_CONTR_CONF,			/* Global configuration */
	MSG_CONTR_CONF_FETCH,		/* Global configuration request */
	MSG_CONTR_POT_CONF,		/* Pot configuration */
	MSG_CONTR_POT_CONF_FETCH,	/* Pot configuration request */
	MSG_CONTR_POT_STATE,		/* Pot state */
	MSG_CONTR_POT_STATE_FETCH,	/* Pot state request */
	MSG_CONTR_POT_REM_STATE,	/* Pot remanent state */
	MSG_CONTR_POT_REM_STATE_FETCH,	/* Pot remanent state request */
	MSG_MAN_MODE,			/* Manual mode settings */
	MSG_MAN_MODE_FETCH,		/* Manual mode settings request */
};

enum man_mode_flags {
	MANFLG_FREEZE_CHANGE	= 1 << 0, /* Freeze change request */
	MANFLG_FREEZE_ENABLE	= 1 << 1, /* Freeze on/off */
	MANFLG_NOTIFY_CHANGE	= 1 << 2, /* LED-status change request */
	MANFLG_NOTIFY_ENABLE	= 1 << 3, /* LED-status on/off */
};

/* Payload of host communication messages. */
struct msg_payload {
	/* The ID number. (enum user_message_id) */
	uint8_t id;

	union {
		/* Log message. */
		struct {
			struct log_item item;
		} _packed log;

		/* RTC time. */
		struct {
			struct rtc_time time;
		} _packed rtc;

		/* Global controller configuration. */
		struct {
			struct controller_global_config conf;
		} _packed contr_conf;

		/* Controller flower pot configuration. */
		struct {
			uint8_t pot_number;
			struct flowerpot_config conf;
		} _packed contr_pot_conf;

		/* Controller flower pot state. */
		struct {
			uint8_t pot_number;
			struct flowerpot_state state;
		} _packed contr_pot_state;

		/* Controller flower pot remanent state. */
		struct {
			uint8_t pot_number;
			struct flowerpot_remanent_state rem_state;
		} _packed contr_pot_rem_state;

		/* Manual mode settings. */
		struct {
			uint8_t force_stop_watering_mask;
			uint8_t valve_manual_mask;
			uint8_t valve_manual_state;
			uint8_t flags;
		} _packed manual_mode;
	} _packed;
} _packed;


/* The current timekeeping count. */
static jiffies_t jiffies_count;
/* Serial communication timer. */
static jiffies_t comm_timer;
/* Timestamp for the next RTC time fetch. */
static jiffies_t next_rtc_fetch;


/* Host message handler.
 * Handle all received control messages sent by the host.
 */
bool comm_handle_rx_message(const struct comm_message *msg,
			    void *reply_payload)
{
	const struct msg_payload *pl = comm_payload(const struct msg_payload *, msg);
	struct msg_payload *reply = reply_payload;
	bool ok;

	if (msg->fc & COMM_FC_ACK) {
		/* This is just an acknowledge. Ignore. */
		return 1;
	}

	switch (pl->id) {
	case MSG_LOG_FETCH: {
		/* Fetch of the first log item. */

		/* Fill the reply message. */
		reply->id = MSG_LOG;
		/* Get the first item from the log stack. */
		ok = log_pop(&reply->log.item);
		if (!ok) {
			/* No log available.
			 * Signal an error to the host. */
			return 0;
		}
		break;
	}
	case MSG_RTC: {
		/* RTC time adjustment. */

		/* Write the new time to the RTC hardware. */
		rv3029_write_time(&pl->rtc.time);
		break;
	}
	case MSG_RTC_FETCH: {
		/* RTC time fetch. */

		/* Fill the reply message. */
		reply->id = MSG_RTC;
		rv3029_get_time(&reply->rtc.time);
		break;
	}
	case MSG_CONTR_CONF: {
		/* Set controller config. */

		struct controller_config conf;

		/* Get the current configuration. */
		controller_get_config(&conf);

		/* Write the new configuration. */
		conf.global = pl->contr_conf.conf;
		controller_update_config(&conf);
		break;
	}
	case MSG_CONTR_CONF_FETCH: {
		/* Fetch controller config. */

		struct controller_config conf;

		/* Get the current configuration. */
		controller_get_config(&conf);

		/* Fill the reply message. */
		reply->id = MSG_CONTR_CONF;
		reply->contr_conf.conf = conf.global;
		break;
	}
	case MSG_CONTR_POT_CONF: {
		/* Set flower pot config. */

		uint8_t pot_number = pl->contr_pot_conf.pot_number;
		struct controller_config conf;

		if (pot_number >= MAX_NR_FLOWERPOTS) {
			/* Invalid pot number. */
			return 0;
		}

		/* Get the current configuration. */
		controller_get_config(&conf);

		/* Write the new configuration. */
		conf.pots[pot_number] = pl->contr_pot_conf.conf;
		controller_update_config(&conf);
		break;
	}
	case MSG_CONTR_POT_CONF_FETCH: {
		/* Fetch flower pot config. */

		uint8_t pot_number = pl->contr_pot_conf.pot_number;
		struct controller_config conf;

		if (pot_number >= MAX_NR_FLOWERPOTS) {
			/* Invalid pot number. */
			return 0;
		}

		/* Get the current configuration. */
		controller_get_config(&conf);

		/* Fill the reply message. */
		reply->id = MSG_CONTR_POT_CONF;
		reply->contr_pot_conf.pot_number = pot_number;
		reply->contr_pot_conf.conf = conf.pots[pot_number];
		break;
	}
	case MSG_CONTR_POT_STATE_FETCH: {
		/* Fetch flower pot state. */

		uint8_t pot_number = pl->contr_pot_state.pot_number;

		if (pot_number >= MAX_NR_FLOWERPOTS) {
			/* Invalid pot number. */
			return 0;
		}

		/* Fill the reply message. */
		reply->id = MSG_CONTR_POT_STATE;
		reply->contr_pot_state.pot_number = pot_number;
		controller_get_pot_state(pot_number,
					 &reply->contr_pot_state.state,
					 NULL);
		break;
	}
	case MSG_CONTR_POT_REM_STATE: {
		/* Set the flower pot remanent state. */

		uint8_t pot_number = pl->contr_pot_rem_state.pot_number;

		if (pot_number >= MAX_NR_FLOWERPOTS) {
			/* Invalid pot number. */
			return 0;
		}

		/* Write the new rememanent state. */
		controller_update_pot_rem_state(pot_number,
						&pl->contr_pot_rem_state.rem_state);
		break;
	}
	case MSG_CONTR_POT_REM_STATE_FETCH: {
		/* Fetch flower pot remanent state. */

		uint8_t pot_number = pl->contr_pot_rem_state.pot_number;

		if (pot_number >= MAX_NR_FLOWERPOTS) {
			/* Invalid pot number. */
			return 0;
		}

		/* Fill the reply message. */
		reply->id = MSG_CONTR_POT_REM_STATE;
		reply->contr_pot_rem_state.pot_number = pot_number;
		controller_get_pot_state(pot_number,
					 NULL,
					 &reply->contr_pot_rem_state.rem_state);
		break;
	}
	case MSG_MAN_MODE: {
		/* Set controller manual mode state. */

		controller_manual_mode(pl->manual_mode.force_stop_watering_mask,
				       pl->manual_mode.valve_manual_mask,
				       pl->manual_mode.valve_manual_state);

		if (pl->manual_mode.flags & MANFLG_FREEZE_CHANGE)
			controller_freeze(!!(pl->manual_mode.flags & MANFLG_FREEZE_ENABLE));

		if (pl->manual_mode.flags & MANFLG_NOTIFY_CHANGE)
			notify_led_set(!!(pl->manual_mode.flags & MANFLG_NOTIFY_ENABLE));

		break;
	}
	default:
		/* Unsupported message. Return failure. */
		return 0;
	}

	return 1;
}

/* 200 Hz system timer. */
ISR(TIMER1_COMPA_vect)
{
	mb();
	/* Increment the system time counter. */
	jiffies_count++;
	mb();
}

/* Get the current system time counter. */
jiffies_t jiffies_get(void)
{
	uint8_t sreg;
	jiffies_t j;

	/* Fetch system time counter with interrupts disabled. */
	sreg = irq_disable_save();
	j = jiffies_count;
	irq_restore(sreg);

	return j;
}

/* Initialize the system timer. */
static void systimer_init(void)
{
	/* Initialize timer-1 to 200 Hz interrupt frequency. */

	/* Set OC value for 16 Mhz CPU clock. */
	build_assert(F_CPU == 16000000ul);
	OCR1A = 1250;
	TCNT1 = 0;
	TCCR1A = 0;
	/* CTC mode, prescaler 64 */
	TCCR1B = (1 << WGM12) | (1 << CS10) | (1 << CS11);
	/* Enable OC interrupt. */
	TIMSK |= (1 << OCIE1A);
}

/* Handle realtime clock work. */
static void handle_rtc(jiffies_t now)
{
	/* Only, if the RTC fetch timer expired. */
	if (time_before(now, next_rtc_fetch))
		return;
	/* Re-trigger the RTC fetch timer. */
	next_rtc_fetch = now + msec_to_jiffies(RTC_FETCH_INTERVAL_MS);

	/* Read the current time from RTC. */
	rv3029_read_time();
}

/* Program entry point. */
int main(void) _mainfunc;
int main(void)
{
	jiffies_t now;

	irq_disable();

	wdt_enable(WDTO_120MS);

	/* Initialize the system. */
	notify_led_init();
	twi_init();
	systimer_init();
	rv3029_init();
	sensor_init();
	controller_init();
	comm_init();

	/* Sanity checks. */
	build_assert(sizeof(struct msg_payload) <= COMM_PAYLOAD_LEN);

	/* Enable interrupts and enter the mainloop. */
	irq_enable();
	while (1) {
		/* Poke the watchdog. */
		wdt_reset();

		/* Get the current timestamp. */
		now = jiffies_get();

		/* Handle serial host communication. */
		comm_work();
		if (!time_before(now, comm_timer)) {
			comm_timer = now + msec_to_jiffies(10);
			comm_centisecond_tick();
		}

		/* Handle realtime clock work. */
		handle_rtc(now);

		/* Run the controller state machine. */
		controller_work();

		/* Handle notification LED state. */
		notify_led_work();
	}
}
