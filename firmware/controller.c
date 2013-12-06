/*
 * Moistcontrol - Controller state machine
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

#include "controller.h"
#include "sensor.h"
#include "util.h"
#include "main.h"
#include "rv3029.h"
#include "ioext.h"
#include "log.h"

#include <string.h>

#include <avr/eeprom.h>


#define CTRL_INTERVAL_SEC	1 //60 TODO
#define VALVE_OPEN_MS		1000
#define VALVE_CLOSE_MS		15000


struct flowerpot {
	uint8_t nr;
	struct flowerpot_state state;
	jiffies_t next_measurement;

	jiffies_t valve_timer;
	bool valve_manual_en;
	bool valve_manual_state;
	bool valve_auto_state;
};

struct controller {
	struct controller_config config;
	struct flowerpot pots[MAX_NR_FLOWERPOTS];
	uint8_t current_pot;

	bool eeprom_update_required;
	jiffies_t eeprom_update_time;
};

static struct controller cont;


#define POT_DEFAULT_CONFIG						\
	{								\
		.flags			= 0,				\
		.min_threshold		= 85,				\
		.max_threshold		= 170,				\
		.active_range = {					\
			.from		= 0,				\
			.to		= (time_of_day_t)(long)-1,	\
		},							\
		.dow_on_mask		= 0x7F,				\
		.dow_ignoretime_mask	= 0,				\
	}

static struct controller_config EEMEM eeprom_cont_config = {
	.pots[0] = POT_DEFAULT_CONFIG,
	.pots[1] = POT_DEFAULT_CONFIG,
	.pots[2] = POT_DEFAULT_CONFIG,
	.pots[3] = POT_DEFAULT_CONFIG,
	.pots[4] = POT_DEFAULT_CONFIG,
	.pots[5] = POT_DEFAULT_CONFIG,
	.global = {
		.flags			= CONTR_FLG_ENABLE,
		.sensor_lowest_value	= 0x000,
		.sensor_highest_value	= 0x3FF,
	},
};


static inline struct flowerpot_config * pot_config(const struct flowerpot *pot)
{
	return &cont.config.pots[pot->nr];
}

static void pot_info(struct flowerpot *pot,
		     uint8_t log_class, uint8_t log_code, uint8_t log_data)
{
	struct log_item log;

	if (!(pot_config(pot)->flags & POT_FLG_LOG))
		return;

	log_init(&log, log_class);
	log.code = log_code;
	log.data = log_data;
	log_append(&log);
}

static void pot_state_enter(struct flowerpot *pot,
			    enum flowerpot_state_id new_state)
{
	uint8_t data;

	if (pot->state.state_id != new_state) {
		pot->state.state_id = new_state;
		data = ((uint8_t)new_state << 4) | (pot->nr & 0xF);
		pot_info(pot, LOG_INFO, LOG_INFO_CONTSTATCHG, data);
	}
}

static uint8_t scale_sensor_val(const struct sensor_result *res)
{
	uint16_t raw_value = res->value;
	uint16_t raw_range;
	uint16_t raw_lowest, raw_highest;
	uint8_t scaled_value;

	/* Clamp the raw sensor value between the lowest and highest
	 * possible values. This aides minimal range overshoots.
	 * Also subtract the lower boundary to adjust the range to zero.
	 */
	raw_lowest = cont.config.global.sensor_lowest_value;
	raw_highest = cont.config.global.sensor_highest_value;
	raw_value = clamp(raw_value, raw_lowest, raw_highest);
	raw_value -= raw_lowest;
	raw_range = raw_highest - raw_lowest;

	/* Scale the value with the formula:
	 *   scaled_value = scale_max / raw_range * raw_value
	 * where scale_max is UINT8_MAX (= 0xFF).
	 * Honor division by zero and precision loss on division.
	 * Round to the nearest integer.
	 */
	if (raw_range) {
		scaled_value = div_round((uint32_t)UINT8_MAX * raw_value,
					 (uint32_t)raw_range);
	} else
		scaled_value = 0;

	return scaled_value;
}

static uint8_t valvenr_to_bitnr(uint8_t nr)
{
	switch (nr) {
	case 0:
		return EXTOUT_VALVE0;
	case 1:
		return EXTOUT_VALVE1;
	case 2:
		return EXTOUT_VALVE2;
	case 3:
		return EXTOUT_VALVE3;
	case 4:
		return EXTOUT_VALVE4;
	case 5:
		return EXTOUT_VALVE5;
	}
	return 0;
}

static void valve_state_commit(struct flowerpot *pot)
{
	uint8_t bitnr = valvenr_to_bitnr(pot->nr);
	bool state;

	if (pot->valve_manual_en)
		state = pot->valve_manual_state;
	else
		state = pot->valve_auto_state;
	/* Invert the state. The hardware will invert it again. */
	state = !state;
	/* And write it to the hardware. */
	ioext_write_bit(bitnr, state);
	ioext_commit();
}

static void valve_close(struct flowerpot *pot)
{
	pot->valve_auto_state = 0;
	valve_state_commit(pot);
}

static void valve_open(struct flowerpot *pot)
{
	pot->valve_auto_state = 1;
	valve_state_commit(pot);

	pot->valve_timer = jiffies_get() + msec_to_jiffies(VALVE_OPEN_MS);
	pot_state_enter(pot, POT_WAITING_FOR_VALVE);
}

static void pot_start_measurement(struct flowerpot *pot)
{
	sensor_start(pot->nr);
	pot_state_enter(pot, POT_MEASURING);
}

static void pot_go_idle(struct flowerpot *pot)
{
	pot->next_measurement = jiffies_get() + sec_to_jiffies(CTRL_INTERVAL_SEC);
	pot_state_enter(pot, POT_IDLE);
}

static void pot_start_watering(struct flowerpot *pot)
{
	pot->state.is_watering = 1;
	valve_open(pot);
}

static void pot_stop_watering(struct flowerpot *pot)
{
	pot->state.is_watering = 0;
	valve_close(pot);
	pot_go_idle(pot);
}

static void pot_reset(struct flowerpot *pot)
{
	if (pot->state.state_id == POT_MEASURING) {
		/* We are currently measuring on this
		 * pot. Cancel the sensor measurement.
		 */
		sensor_cancel();
	}

	pot->state.is_watering = 0;
	pot->next_measurement = jiffies_get() + sec_to_jiffies(1);
	pot_state_enter(pot, POT_IDLE);
	pot->valve_manual_en = 0;
	pot->valve_manual_state = 0;
	valve_close(pot);
}

static void handle_pot(struct flowerpot *pot)
{
	struct sensor_result result;
	uint8_t sensor_val;
	jiffies_t now;
	bool ok;
	struct rtc_time rtc;
	time_of_day_t tod;
	uint8_t dow_mask;
	const struct flowerpot_config *config = pot_config(pot);

	now = jiffies_get();

	switch (pot->state.state_id) {
	case POT_IDLE:
		/* Idle: We are not doing anything, yet. */

		if (!(config->flags & POT_FLG_ENABLED)) {
			/* Pot is disabled. */
			break;
		}

		/* Get the current RTC time. */
		rv3029_get_time(&rtc);
		/* Create a day-of-week mask for today. */
		dow_mask = BITMASK8(rtc.day_of_week);

		if (!(config->dow_on_mask & dow_mask)) {
			/* Pot is disabled on today's weekday. */
			break;
		}

		/* Check active range, if requested. */
		if (!(config->dow_ignoretime_mask & dow_mask)) {
			/* Check if we in the active range. */
			tod = rtc_get_time_of_day(&rtc);
			if (time_of_day_before(tod, config->active_range.from) ||
			    time_of_day_after(tod, config->active_range.to)) {
				/* Current time is not in the active range.
				 * Don't run.
				 */
				break;
			}
		}

		/* Check, if the next measurement is pending. */
		if (time_before(now, pot->next_measurement))
			break;

		/* It's time to start a new measurement. */
		pot_state_enter(pot, POT_START_MEASUREMENT);
		break;
	case POT_START_MEASUREMENT:
		/* Start the measurement as soon as the sensors go idle. */

		if (!sensors_idle()) {
			/* Another sensor measurement is still running.
			 * Cannot start this measurement, yet.
			 */
			break;
		}

		/* Start the controller cycle. */
		pot_start_measurement(pot);
		break;
	case POT_MEASURING:
		/* Poll the sensor state. */

		ok = sensor_poll(&result);
		if (!ok) {
			/* The measurement did not finish, yet. */
			break;
		}

		/* Check if logging is requested for this pot. */
		if (config->flags & POT_FLG_LOG) {
			struct log_item log;

			/* Get the current RTC time. */
			rv3029_get_time(&rtc);

			/* Fill out the log data structure
			 * with the current timestamp and sensor values
			 * and queue it in the log subsystem.
			 */
			log_init(&log, LOG_SENSOR_DATA);
			log.sensor_data = LOG_SENSOR_DATA(result.nr,
							  result.value);
			log_append(&log);
		}

		/* Scale the raw sensor value to the 8-bit
		 * data type of the controller logics.
		 */
		sensor_val = scale_sensor_val(&result);
		pot->state.last_measured_raw_value = result.value;
		pot->state.last_measured_value = sensor_val;

		if (pot->state.is_watering) {
			if (sensor_val >= config->max_threshold)
				pot_stop_watering(pot);
			else
				valve_open(pot);
		} else {
			if (sensor_val < config->min_threshold)
				pot_start_watering(pot);
			else
				pot_go_idle(pot);
		}
		break;
	case POT_WAITING_FOR_VALVE:
		/* Wait for the valve open/close cycle to finish. */

		if (time_before(now, pot->valve_timer)) {
			/* Open or close time not expired, yet. */
			break;
		}

		if (pot->valve_auto_state) {
			/* Valve is currently opened.
			 * Close it and start the close timer.
			 */
			valve_close(pot);
			pot->valve_timer = now + msec_to_jiffies(VALVE_CLOSE_MS);
		} else {
			/* Close timer expired.
			 * Perform a new measurement, now. */
			pot_state_enter(pot, POT_START_MEASUREMENT);
		}
		break;
	}
}

static void controller_reset(void)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(cont.pots); i++)
		pot_reset(&cont.pots[i]);
	cont.current_pot = 0;
}

void controller_get_config(struct controller_config *dest)
{
	*dest = cont.config;
}

void controller_update_config(const struct controller_config *new_config)
{
	struct controller_config *active;
	uint8_t i;

	/* Get pointer to the currently active configuration. */
	active = &cont.config;

	if (memcmp(&new_config->global, &active->global,
		   sizeof(new_config->global)) == 0) {
		/* Global config did not change.
		 * Check if a pot changed.
		 */
		for (i = 0; i < MAX_NR_FLOWERPOTS; i++) {
			if (memcmp(&new_config->pots[i], &active->pots[i],
				   sizeof(new_config->pots[i])) != 0) {
				/* This pot changed.
				 * Reset the pot's state machine.
				 */
				pot_reset(&cont.pots[i]);
			}
		}
	} else {
		/* Global config differs.
		 * Reset the complete controller state machine (all pots).
		 */
		controller_reset();
	}

	/* Copy the new configuration to the active config in RAM. */
	cont.config = *new_config;

	/* Schedule an EEPROM update. */
	cont.eeprom_update_time = jiffies_get() + msec_to_jiffies(3000);
	cont.eeprom_update_required = 1;
}

void controller_get_pot_state(uint8_t pot_number,
			      struct flowerpot_state *state)
{
	if (pot_number < ARRAY_SIZE(cont.pots))
		*state = cont.pots[pot_number].state;
}

void controller_manual_mode(uint8_t force_stop_watering_mask,
			    uint8_t valve_manual_mask,
			    uint8_t valve_manual_state)
{
	struct flowerpot *pot;
	uint8_t i, mask;

	for (i = 0, mask = 1; i < MAX_NR_FLOWERPOTS; i++, mask <<= 1) {
		pot = &cont.pots[i];

		if ((force_stop_watering_mask & mask) &&
		    pot->state.is_watering)
			pot_stop_watering(pot);
		pot->valve_manual_en = !!(valve_manual_mask & mask);
		pot->valve_manual_state = !!(valve_manual_state & mask);
		valve_state_commit(pot);
	}
}

void controller_work(void)
{
	if (cont.eeprom_update_required &&
	    time_after(jiffies_get(), cont.eeprom_update_time)) {
		cont.eeprom_update_required = 0;
		/* An EEPROM write was scheduled.
		 * Update the EEPROM contents.
		 * This only updates the bytes that changed. (reduces wearout)
		 */
		eeprom_update_block(&cont.config, &eeprom_cont_config,
				    sizeof(cont.config));
	}

	if (cont.config.global.flags & CONTR_FLG_ENABLE) {
		/* The controller is enabled globally.
		 * Run the pot state machines. */

		handle_pot(&cont.pots[cont.current_pot]);
		cont.current_pot++;
		if (cont.current_pot >= ARRAY_SIZE(cont.pots))
			cont.current_pot = 0;
	}
}

void controller_init(void)
{
	struct flowerpot *pot;
	uint8_t i;

	ioext_init();

	memset(&cont, 0, sizeof(cont));
	eeprom_read_block(&cont.config, &eeprom_cont_config,
			  sizeof(cont.config));
	for (i = 0; i < ARRAY_SIZE(cont.pots); i++) {
		pot = &cont.pots[i];

		pot->nr = i;
		pot_reset(pot);
	}
}
