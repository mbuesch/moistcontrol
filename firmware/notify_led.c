/*
 * Notification LED
 *
 * Copyright (c) 2014 Michael Buesch <m@bues.ch>
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

#include "notify_led.h"
#include "main.h"

#include <avr/io.h>
#include <avr/eeprom.h>

#include <string.h>


#define NOTIFY_LED_DDR		DDRD
#define NOTIFY_LED_PORT		PORTD
#define NOTIFY_LED_BIT		4

#define PULSE_PAUSE_TIME	msec_to_jiffies(50)
#define LONG_PAUSE_TIME		msec_to_jiffies(3000)


struct notify_led {
	bool state;
	uint8_t count;
	jiffies_t timer;
};

static struct notify_led led;

static uint8_t EEMEM eeprom_notify_led_state = 0;


void notify_led_set(bool on)
{
	uint8_t sreg;

	sreg = irq_disable_save();

	if (led.state != on) {
		led.count = 0;
		led.state = on;
		led.timer = jiffies_get() + PULSE_PAUSE_TIME;

		if (on)
			NOTIFY_LED_PORT |= (1 << NOTIFY_LED_BIT);
		else
			NOTIFY_LED_PORT &= ~(1 << NOTIFY_LED_BIT);

		eeprom_update_byte(&eeprom_notify_led_state, on);
	}

	irq_restore(sreg);
}

bool notify_led_get(void)
{
	return led.state;
}

void notify_led_work(void)
{
	jiffies_t now = jiffies_get();
	uint8_t sreg;

	sreg = irq_disable_save();

	if (led.state && !time_before(now, led.timer)) {
		switch (led.count) {
		default:
			led.count = -1;
			/* fallthrough */
		case 0:
		case 1:
		case 2:
		case 3:
			led.count++;
			led.timer = now + PULSE_PAUSE_TIME;
			NOTIFY_LED_PORT ^= (1 << NOTIFY_LED_BIT);
			break;
		case 4:
			led.count++;
			led.timer = now + LONG_PAUSE_TIME;
			NOTIFY_LED_PORT &= ~(1 << NOTIFY_LED_BIT);
			break;
		}
	}

	irq_restore(sreg);
}

void notify_led_init(void)
{
	NOTIFY_LED_PORT &= ~(1 << NOTIFY_LED_BIT);
	NOTIFY_LED_DDR |= (1 << NOTIFY_LED_BIT);

	memset(&led, 0, sizeof(led));
	led.state = eeprom_read_byte(&eeprom_notify_led_state);
}
