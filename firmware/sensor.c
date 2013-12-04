/*
 * Moistcontrol - sensor
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

#include "sensor.h"
#include "util.h"
#include "main.h"

#include <string.h>


/* Warmup time:	The time to wait with sensor _enabled_
 *		before performing one measurement. */
#define WARMUP_TIME		msec_to_jiffies(50)

/* Wait time:	The time to wait with sensor _disabled_
 *		between measurements. */
#define WAIT_TIME		msec_to_jiffies(500)


/* Sensor state-machine values. */
enum sensor_status {
	STAT_IDLE,	/* Idle: No measurement requested. */
	STAT_WAIT,	/* Wait: Wait time between measurements. */
	STAT_WARMUP_P0,	/* Warmup: First warmup time before measurement. */
	STAT_WARMUP_P1,	/* Warmup: Second warmup time before measurement. */
	STAT_ADC_CONV,	/* ADC-conv: ADC-conversion is in progress. */
};

/* Context of a measurement. */
struct sensor_context {
	/* Current state-machine status. */
	enum sensor_status stat;
	/* Generic timer used for wait and warmup. */
	jiffies_t timer;
	/* The sensor number of the currently active
	 * measurement, if any. */
	uint8_t nr;

	/* Temporary buffer for the measured values. */
	uint16_t values[3];
	uint8_t value_count;
};

/* Instance of the measurement context. */
static struct sensor_context sensor;

/* Port mappings for the supply-A lines of the sensors.
 * The array indices are the sensor number.
 * The array values are the port/ddr bit number, DDR register
 * and PORT register, respectively.
 */
static const uint8_t PROGMEM sensor_a_bit[] = {
	[0] = 5,
	[1] = 6,
	[2] = 7,
	[3] = 2,
	[4] = 1,
	[5] = 0,
};
static volatile uint8_t * const PROGMEM sensor_a_ddr[] = {
	[0] = &DDRD,
	[1] = &DDRD,
	[2] = &DDRD,
	[3] = &DDRB,
	[4] = &DDRB,
	[5] = &DDRB,
};
static volatile uint8_t * const PROGMEM sensor_a_port[] = {
	[0] = &PORTD,
	[1] = &PORTD,
	[2] = &PORTD,
	[3] = &PORTB,
	[4] = &PORTB,
	[5] = &PORTB,
};

/* Sensor supply terminal "B" definitions */
#define SENSOR_SUPPLY_B_PORT		PORTC
#define SENSOR_SUPPLY_B_DDR		DDRC
#define SENSOR_SUPPLY_B_BIT		3


/* The number of available sensors. */
#define SENSOR_COUNT	ARRAY_SIZE(sensor_a_bit)


/* Read the current ADC value. */
static uint16_t sensor_adc_read_value(void)
{
	return ADCW;
}

/* Start an ADC conversion. */
static void sensor_adc_start(void)
{
	/* Initialize ADC to 125 kHz (on 16 MHz CPU).
	 * Select AVcc reference.
	 * Set multiplexer to ADC0.
	 */
	build_assert(F_CPU == 16000000ul);
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);
	ADCSRA |= (1 << ADSC);
}

/* Returns the boolean state of the ADC unit.
 * 0: conversion active.  1: conversion done.
 */
static bool sensor_adc_done(void)
{
	return !(ADCSRA & (1 << ADSC));
}

/* Get the port access values for a sensor "A" supply.
 * sensor_nr: The sensor number to get the values for.
 * bitmask: Pointer to the returned bitmask.
 * ddr: Pointer to the returned DDR register.
 * port: Pointer to the returned PORT register.
 */
static inline void get_sensor_a_supply(uint8_t sensor_nr,
				       uint8_t *bitmask,
				       volatile uint8_t **ddr,
				       volatile uint8_t **port)
{
	uint8_t bitnr;

	PANIC_ON(sensor_nr >= SENSOR_COUNT);

	/* Read the bit number, DDR and PORT pointers from the tables. */
	bitnr = pgm_read_byte(&sensor_a_bit[sensor_nr]);
	*bitmask = BITMASK8(bitnr);
	*ddr = (volatile uint8_t *)pgm_read_word(&sensor_a_ddr[sensor_nr]);
	*port = (volatile uint8_t *)pgm_read_word(&sensor_a_port[sensor_nr]);
}

/* Enable the power supply of a sensor.
 * nr: The sensor number.
 * polarity: The supply polarity.
 *           Polarity 0 means: Vcc on supply "B" and
 *                             GND on supply "A".
 *           Polarity 1 means: Vcc on supply "A" and
 *                             GND on supply "B".
 */
static void sensor_enable(uint8_t nr, bool polarity)
{
	uint8_t sreg, a_mask;
	volatile uint8_t *a_ddr, *a_port;

	/* Get the supply-A credentials. */
	get_sensor_a_supply(nr, &a_mask, &a_ddr, &a_port);

	/* Enable A- and B-supply */
	sreg = irq_disable_save();
	if (polarity) {
		_MMIO_BYTE(a_ddr) |= a_mask;
		_MMIO_BYTE(a_port) |= a_mask;
		SENSOR_SUPPLY_B_DDR |= (1 << SENSOR_SUPPLY_B_BIT);
		SENSOR_SUPPLY_B_PORT &= ~(1 << SENSOR_SUPPLY_B_BIT);
	} else {
		_MMIO_BYTE(a_ddr) |= a_mask;
		_MMIO_BYTE(a_port) &= ~a_mask;
		SENSOR_SUPPLY_B_DDR |= (1 << SENSOR_SUPPLY_B_BIT);
		SENSOR_SUPPLY_B_PORT |= (1 << SENSOR_SUPPLY_B_BIT);
	}
	irq_restore(sreg);
}

/* Disable the power supply of a sensor.
 * nr: The sensor number.
 */
static void sensor_disable(uint8_t nr)
{
	uint8_t sreg, a_mask;
	volatile uint8_t *a_ddr, *a_port;

	/* Get the supply-A credentials. */
	get_sensor_a_supply(nr, &a_mask, &a_ddr, &a_port);

	/* Disable A- and B-supply. */
	sreg = irq_disable_save();
	_MMIO_BYTE(a_ddr) &= ~a_mask;
	_MMIO_BYTE(a_port) &= ~a_mask;
	SENSOR_SUPPLY_B_DDR &= ~(1 << SENSOR_SUPPLY_B_BIT);
	SENSOR_SUPPLY_B_PORT &= ~(1 << SENSOR_SUPPLY_B_BIT);
	irq_restore(sreg);
}

/* Start the warmup-cycle of the current sensor. */
static void sensor_warmup_begin(void)
{
	/* Set the warmup-end time and set
	 * warmup-polarity-0 state. */
	sensor.timer = jiffies_get() + WARMUP_TIME;
	sensor.stat = STAT_WARMUP_P0;
	/* Enable the sensor with 0-polarity. */
	sensor_enable(sensor.nr, 0);
}

/* Start a measurement on a sensor.
 * nr: The sensor number.
 */
void sensor_start(uint8_t nr)
{
	if (sensor.stat != STAT_IDLE) {
		/* Current status is not idle, cannot start. */
		return;
	}
	if (nr >= SENSOR_COUNT) {
		/* Invalid sensor number. */
		return;
	}

	/* Reset the stored values to zero. */
	memset(sensor.values, 0, sizeof(sensor.values));
	sensor.value_count = 0;
	/* Store the sensor number. */
	sensor.nr = nr;
	/* Start the warmup sequence. */
	sensor_warmup_begin();
}

/* Cancel the currently running measurement. */
void sensor_cancel(void)
{
	if (sensor.stat == STAT_IDLE)
		return;

	/* Wait for possibly running ADC to finish. */
	while (!sensor_adc_done());
	/* Disable the supplies and reset the state machine. */
	sensor_disable(sensor.nr);
	sensor.stat = STAT_IDLE;
}

/* Returns 1, if no measurement is running.
 * Returns 0 otherwise.
 */
bool sensors_idle(void)
{
	return sensor.stat == STAT_IDLE;
}

/* Poll the current measurement state.
 * Returns 1, if the measurement is finished.
 * Returns 0, if the measurement is still in progress.
 * If the measurement is finished (1), it puts the
 * measurement result into "res".
 */
bool sensor_poll(struct sensor_result *res)
{
	jiffies_t now;
	uint16_t a, b, c, median;

	if (sensor.stat == STAT_IDLE) {
		/* No measurement running. Return early. */
		return 0;
	}

	/* Get the current time. */
	now = jiffies_get();

	/* Run the sensor statemachine. */
	switch (sensor.stat) {
	case STAT_IDLE:
		break;
	case STAT_WAIT:
		if (time_before(now, sensor.timer)) {
			/* Wait time not finished, yet. */
			break;
		}
		/* Wait finished. Switch to warmup. */
		sensor_warmup_begin();
		break;
	case STAT_WARMUP_P0:
		if (time_before(now, sensor.timer)) {
			/* Warmup with polarity 0 not finished, yet. */
			break;
		}
		/* Warmup with polarity 0 done.
		 * Start warmup phase with polarity 1. */
		sensor_enable(sensor.nr, 1);
		sensor.timer = now + WARMUP_TIME;
		sensor.stat = STAT_WARMUP_P1;
		break;
	case STAT_WARMUP_P1:
		if (time_before(now, sensor.timer)) {
			/* Warmup with polarity 1 not finished, yet. */
			break;
		}
		/* Warmup with polarity 1 done.
		 * Start ADC conversion. */
		sensor_adc_start();
		sensor.stat = STAT_ADC_CONV;
		break;
	case STAT_ADC_CONV:
		if (!sensor_adc_done()) {
			/* ADC conversion not finised, yet. */
			break;
		}
		/* ADC conversion done. Disable sensor. */
		sensor_disable(sensor.nr);

		/* Store the measured value. */
		sensor.values[sensor.value_count] = sensor_adc_read_value();
		sensor.value_count++;

		if (sensor.value_count >= 3) {
			/* All measurements done. */

			a = sensor.values[0];
			b = sensor.values[1];
			c = sensor.values[2];

			/* Get the median of all measurements.
			 * 'b' will be the result. */
			if (a > b)
				swap_values(a, b);
			if (b > c)
				swap_values(b, c);
			median = b;

			/* Store the result. */
			res->nr = sensor.nr;
			res->value = median;
			sensor.stat = STAT_IDLE;

			/* Whole measurement done. */
			return 1;
		} else {
			/* Schedule the next measurement. */

			sensor.timer = now + WAIT_TIME;
			sensor.stat = STAT_WAIT;
		}
		break;
	}

	/* Measurement still in progress. */
	return 0;
}

/* Initialize the sensor unit. */
void sensor_init(void)
{
	uint8_t nr;

	build_assert(ARRAY_SIZE(sensor_a_bit) == ARRAY_SIZE(sensor_a_ddr));
	build_assert(ARRAY_SIZE(sensor_a_bit) == ARRAY_SIZE(sensor_a_port));

	/* Reset the sensor context. */
	memset(&sensor, 0, sizeof(sensor));

	/* Disable all sensors. */
	for (nr = 0; nr < SENSOR_COUNT; nr++)
		sensor_disable(nr);

	/* Perform one ADC conversion, as per AtMega datasheet the
	 * very first conversion has less precision.
	 * Discard the result.
	 */
	sensor_adc_start();
	while (!sensor_adc_done());
}
