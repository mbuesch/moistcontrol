/*
 * On/off-switch
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

#include "onoffswitch.h"
#include "util.h"
#include "main.h"

#include <avr/io.h>


/* Port definitions for the hardware on/off-switch. */
#define ONOFFSWITCH_DDR			DDRD
#define ONOFFSWITCH_PORT		PORTD
#define ONOFFSWITCH_PIN			PIND
#define ONOFFSWITCH_BIT			PD3

/* Saved state. */
static bool onoffswitch_state;
/* Timestamp for the next check. */
static jiffies_t next_check;


/* Initialize the on/off-switch. */
void onoffswitch_init(void)
{
	ONOFFSWITCH_DDR &= ~(1 << ONOFFSWITCH_BIT);
	ONOFFSWITCH_PORT |= (1 << ONOFFSWITCH_BIT);
	_delay_ms(20); /* Wait for pull-up. */
}

/* Get the on/off-switch state. */
enum onoff_state onoffswitch_get_state(void)
{
	bool new_state, old_state;
	jiffies_t now = jiffies_get();

	/* Debounce time. */
	if (time_before(now, next_check)) {
		if (onoffswitch_state)
			return ONOFF_IS_ON;
		return ONOFF_IS_OFF;
	}
	next_check = now + msec_to_jiffies(100);

	/* Get the state. */
	new_state = !!(ONOFFSWITCH_PIN & (1 << ONOFFSWITCH_BIT));
	/* Switch logic is inverted. */
	new_state = !new_state;

	/* Detect state and edges. */
	old_state = onoffswitch_state;
	onoffswitch_state = new_state;
	if (new_state && !old_state)
		return ONOFF_SWITCHED_ON;
	if (!new_state && old_state)
		return ONOFF_SWITCHED_OFF;
	if (new_state)
		return ONOFF_IS_ON;
	return ONOFF_IS_OFF;
}
