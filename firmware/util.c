/*
 * Utility functions
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

#include "util.h"

#include <avr/wdt.h>


/* Bitnumber to bitmask lookup table. */
const uint8_t PROGMEM _bit_to_mask8[8] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

/* Reboot the SOC. */
void reboot(void)
{
	/* Disable IRQs and use the watchdog
	 * to trigger a watchdog reset. */
	irq_disable();
	wdt_enable(WDTO_15MS);
	while (1);
	unreachable();
}

/* A fatal error occurred. */
void panic(void)
{
	//TODO: Try to get an error message out.
	reboot();
}
