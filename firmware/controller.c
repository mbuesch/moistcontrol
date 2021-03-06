/*
 * Moistcontrol - Controller state machine
 *
 * Copyright (c) 2013-2015 Michael Buesch <m@bues.ch>
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
#include "notify_led.h"
#include "onoffswitch.h"

#include <string.h>

#include <avr/eeprom.h>


/* Controller interval time, in seconds.
 * This is the time the controller waits between measurements.
 */
#define CTRL_INTERVAL_SEC		60
/* First wait time, in seconds.
 * This is the time the controller waits before doing the first
 * measurement after a controller reset.
 */
#define FIRST_CTRL_INTERVAL_SEC		10

/* The time a valve is held "opened" when watering.
 * In milliseconds.
 */
#define VALVE_OPEN_MS			3000
/* The time a value is held "closed" when watering before doing
 * the next measurement. In milliseconds.
 */
#define VALVE_CLOSE_MS			30000


/* The watering-watchdog timeout, in seconds.
 * If the watchdog times out, an error is assumed and watering
 * of the affected pot is stopped.
 */
#define WATCHDOG_TIMEOUT_SEC		600
/* The watering-watchdog retrigger threshold.
 * If the measured value raised by this threshold, the watchdog
 * will be retriggered.
 * This value is a percentage of the regulator range. The range is
 * max_threshold minus min_threshold.
 */
#define WATCHDOG_THRESHOLD_PERCENT	15


/* Flowerpot controller context data structure. */
struct flowerpot {
	/* The ID-number of this pot. */
	uint8_t nr;
	/* The world-visible state of the controller state machine
	 * for this pot.
	 */
	struct flowerpot_state state;
	/* The remanent state.
	 * Remanent means it is also stored in EEPROM.
	 */
	struct flowerpot_remanent_state rem_state;
	/* Timestamp for the next measurement. */
	jiffies_t next_measurement;

	/* Timer variable for the VALVE_OPEN_MS and
	 * VALVE_CLOSE_MS times.
	 */
	jiffies_t valve_timer;
	/* Enable-state of manual-mode for this pot's valve.
	 * Manual mode is enabled, if this bit is 1.
	 */
	bool valve_manual_en;
	/* Manual-mode state for this pot's valve.
	 * The valve is force opened, if this bit is 1
	 * and manual mode is enabled.
	 * The valve is force closed, if this bit is 0
	 * and manual mode is enabled.
	 */
	bool valve_manual_state;
	/* Automatic-mode state for this pot's valve.
	 * The valve is opened, if this bit is 1
	 * and manual mode is disabled.
	 * The valve is closed, if this bit is 0
	 * and manual mode is disabled.
	 */
	bool valve_auto_state;

	/* The timeout time of the watering watchdog.
	 * This is set to the current time plus the relative timeout
	 * value, when watering starts.
	 */
	jiffies_t watering_watchdog_timeout;
	/* The watering watchdog retrigger threshold.
	 * If the current sensor value is equal or bigger than
	 * this, the watchdog timeout is retriggered.
	 */
	uint8_t watering_watchdog_threshold;
};

/* Controller context data structure. */
struct controller {
	/* The active controller configuration.
	 * This is a RAM-copy of the EEPROM contents.
	 */
	struct controller_config config;

	/* The instances of the flowerpot contexts. */
	struct flowerpot pots[MAX_NR_FLOWERPOTS];
	/* Counter variable for the flowerpot cycle. */
	uint8_t current_pot;

	/* EEPROM-update flag.
	 * If this bit is set, an EEPROM update is pending.
	 * The EEPROM update will be performed as soon as
	 * the timer has expired.
	 */
	bool eeprom_update_required;
	/* The EEPROM-update timer. */
	jiffies_t eeprom_update_time;

	/* Controller activity is frozen? */
	bool frozen;
	/* Timeout for the controller freeze. */
	jiffies_t freeze_timeout;
};

/* Instance of the controller context. */
static struct controller cont;


/* The EEPROM memory for storage of configuration values. */
static struct controller_config EEMEM eeprom_cont_config = {
	.pots[0 ... (MAX_NR_FLOWERPOTS - 1)] = {
		.flags			= 0,
		.min_threshold		= 85,
		.max_threshold		= 170,
		.active_range = {
			.from		= 0,
			.to		= (time_of_day_t)(long)-1,
		},
		.dow_on_mask		= 0x7F,
	},
	.global = {
		.flags			= CONTR_FLG_ENABLE,
		.sensor_lowest_value	= 0,
		.sensor_highest_value	= SENSOR_MAX,
	},
};

/* The EEPROM memory for storage of the remanent per-pot states. */
static struct flowerpot_remanent_state EEMEM eeprom_pot_rem_state[MAX_NR_FLOWERPOTS] = {
	[0 ... (MAX_NR_FLOWERPOTS - 1)] = {
		.flags		= 0,
	},
};


/* Get a pointer to the configuration structure for a pot.
 * pot: A pointer to the flowerpot.
 */
static inline struct flowerpot_config * pot_config(const struct flowerpot *pot)
{
	return &cont.config.pots[pot->nr];
}

/* Write the remanent state of a pot to the EEPROM.
 * pot: A pointer to the flowerpot.
 */
static void pot_remanent_state_commit_eeprom(struct flowerpot *pot)
{
	eeprom_update_block_wdtsafe(&pot->rem_state,
				    &eeprom_pot_rem_state[pot->nr],
				    sizeof(pot->rem_state));
}

/* Emit a log message, if logging is enabled.
 * pot: A pointer to the flowerpot.
 * log_class: The logging class to use. May be LOG_INFO or LOG_ERROR.
 * log_code: The info or error code.
 * log_data: Additional user-data for this log message.
 */
static void pot_info(struct flowerpot *pot,
		     uint8_t log_class, uint8_t log_code, uint8_t log_data)
{
	if (!(pot_config(pot)->flags & POT_FLG_LOG)) {
		/* Logging is disabled. Do not emit the message. */
		return;
	}

	log_event(log_class, log_code, log_data);
}

/* Emit a log message, if verbose logging is enabled.
 * pot: A pointer to the flowerpot.
 * log_class: The logging class to use. May be LOG_INFO or LOG_ERROR.
 * log_code: The info or error code.
 * log_data: Additional user-data for this log message.
 */
static void pot_info_verbose(struct flowerpot *pot,
			     uint8_t log_class, uint8_t log_code, uint8_t log_data)
{
	if (pot_config(pot)->flags & POT_FLG_LOGVERBOSE)
		pot_info(pot, log_class, log_code, log_data);
}

/* Switch the controller state machine into another state.
 * pot: A pointer to the flowerpot.
 * new_state: The new state to switch to.
 */
static void pot_state_enter(struct flowerpot *pot,
			    enum flowerpot_state_id new_state)
{
	uint8_t data;

	/* Switch state, if new_state is different from the current state. */
	if (pot->state.state_id != new_state) {
		pot->state.state_id = new_state;

		/* Emit a verbose log message for this switch operation.
		 * The log data holds the pot number in the lower 4 bits
		 * and the new state number in the upper 4 bits.
		 */
		data = ((uint8_t)new_state << 4) | (pot->nr & 0xF);
		pot_info_verbose(pot, LOG_INFO, LOG_INFO_CONTSTATCHG, data);
	}
}

/* Scale the raw sensor ADC value into the fixed 0-255 moisture range.
 * res: Pointer to the sensor result (ADC value).
 * Returns the 8-bit scaled value.
 */
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

/* Get the output extender bit-number for a valve.
 * nr: The valve number to get the extender-bit-number for.
 * Returns the output extender bit value.
 */
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

/* Write the current valve state out to the valve hardware.
 * pot: A pointer to the flowerpot.
 */
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

/* Set the automatic-state of a valve to "closed"
 * and write the state to the hardware.
 * pot: A pointer to the flowerpot.
 */
static void valve_close(struct flowerpot *pot)
{
	pot->valve_auto_state = 0;
	valve_state_commit(pot);
}

/* Set the automatic-state of a valve to "opened"
 * and write the state to the hardware.
 * Also start the valve-open-timer and switch the state machine
 * into the "waiting-for-valve" state.
 * pot: A pointer to the flowerpot.
 */
static void valve_open(struct flowerpot *pot)
{
	pot->valve_auto_state = 1;
	valve_state_commit(pot);

	/* Set state machine to waiting-for-valve. */
	pot->valve_timer = jiffies_get() + msec_to_jiffies(VALVE_OPEN_MS);
	pot_state_enter(pot, POT_WAITING_FOR_VALVE);
}

/* Start a sensor measurement and switch the state machine
 * into the "measuring" state.
 * pot: A pointer to the flowerpot.
 */
static void pot_start_measurement(struct flowerpot *pot)
{
	sensor_start(pot->nr);
	pot_state_enter(pot, POT_MEASURING);
}

/* Switch the state machine into the "idle" state.
 * Also schedule the next measurement.
 * pot: A pointer to the flowerpot.
 */
static void pot_go_idle(struct flowerpot *pot)
{
	pot->next_measurement = jiffies_get() + sec_to_jiffies(CTRL_INTERVAL_SEC);
	pot_state_enter(pot, POT_IDLE);
}

/* Stop watering.
 * This will reset the "watering" state and close the valve.
 * Additionally it will reset the controller state machine to "idle".
 * pot: A pointer to the flowerpot.
 */
static void pot_stop_watering(struct flowerpot *pot)
{
	if (pot->state.is_watering) {
		/* Emit a "watering stopped" log message.
		 * The lower 4 bits of the log data is the pot number
		 * and the 7th bit is the "watering active" bit.
		 */
		pot_info(pot, LOG_INFO, LOG_INFO_WATERINGCHG,
			 pot->nr & 0x0F);

		/* Go out of watering state, close the valve and
		 * set the state machine to "idle"
		 */
		pot->state.is_watering = 0;
	}
	valve_close(pot);
	pot_go_idle(pot);
}

/* Reset the state machine on one pot.
 * pot: A pointer to the flowerpot.
 * clear_measured: Clear measured values?
 */
static void pot_reset(struct flowerpot *pot, bool clear_measured)
{
	if (pot->state.state_id == POT_MEASURING) {
		/* We are currently measuring on this
		 * pot. Cancel the sensor measurement.
		 */
		sensor_cancel();
	}

	/* Reset all state values. */
	pot->state.is_watering = 0;
	if (clear_measured) {
		pot->state.last_measured_raw_value = 0;
		pot->state.last_measured_value = 0;
	}
	pot->next_measurement = jiffies_get() + sec_to_jiffies(FIRST_CTRL_INTERVAL_SEC);
	pot_state_enter(pot, POT_IDLE);
	pot->valve_manual_en = 0;
	pot->valve_manual_state = 0;

	/* Make sure the valve is closed. */
	valve_close(pot);
}

/* Retrigger the watering watchdog.
 * pot: A pointer to the flowerpot.
 */
static void pot_watchdog_retrigger(struct flowerpot *pot)
{
	const struct flowerpot_config *config = pot_config(pot);
	jiffies_t now = jiffies_get();
	uint8_t range, threshold;

	/* Get the configured regulator range. */
	range = max(0, (int16_t)config->max_threshold - (int16_t)config->min_threshold);

	/* Calculate the watchdog retrigger threshold. */
	threshold = (uint16_t)range * WATCHDOG_THRESHOLD_PERCENT / 100;
	threshold = max(1, threshold);

	/* Assign the new watchdog parameters. */
	pot->watering_watchdog_threshold = pot->state.last_measured_value + threshold;
	pot->watering_watchdog_timeout = now + sec_to_jiffies(WATCHDOG_TIMEOUT_SEC);
}

/* Check whether the watering watchdog timed out.
 * If that's the case, take appropriate measures.
 * pot: A pointer to the flowerpot.
 */
static bool pot_check_watchdog(struct flowerpot *pot)
{
	jiffies_t now = jiffies_get();

	if (pot->state.last_measured_value >= pot->watering_watchdog_threshold) {
		/* The sensor value raised above the threshold.
		 * Retrigger the watchdog.
		 */
		pot_watchdog_retrigger(pot);
		return 0;
	}

	if (time_before(now, pot->watering_watchdog_timeout)) {
		/* The watchdog did not time out, yet. */
		return 0;
	}

	/* The watchdog timed out. This is an emergency situation.
	 * Log the event.
	 */
	notify_led_set(1);
	pot_info(pot, LOG_ERROR, LOG_ERR_WATERDOG, pot->nr & 0x0F);

	/* Now stop watering and shutdown the pot. */
	pot_stop_watering(pot);
	pot->rem_state.flags |= POT_REMFLG_WDTRIGGER;
	pot_remanent_state_commit_eeprom(pot);

	return 1;
}

/* Clear the watchdog flag on a pot.
 * pot: The pot to clear the watchdog on.
 */
static void pot_watchdog_clear(struct flowerpot *pot)
{
	if (pot->rem_state.flags & POT_REMFLG_WDTRIGGER) {
		pot->rem_state.flags &= ~POT_REMFLG_WDTRIGGER;
		pot_reset(pot, 1);
		pot_remanent_state_commit_eeprom(pot);
	}
}

/* Clear the watchdog flags on all pots.
 * This will re-enable watering, if it was stopped due to watchdog.
 */
static void controller_watchdogs_clear(void)
{
	struct flowerpot *pot;
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(cont.pots); i++) {
		pot = &cont.pots[i];
		pot_watchdog_clear(pot);
	}
}

/* Start watering.
 * This will set the "watering" state and open the valve.
 * pot: A pointer to the flowerpot.
 */
static void pot_start_watering(struct flowerpot *pot)
{
	/* Emit a "watering started" log message.
	 * The lower 4 bits of the log data is the pot number
	 * and the 7th bit is the "watering active" bit.
	 */
	pot_info(pot, LOG_INFO, LOG_INFO_WATERINGCHG,
		 (pot->nr & 0x0F) | 0x80);

	/* Start the watchdog that will stop watering,
	 * if it takes too long.
	 */
	pot_watchdog_retrigger(pot);

	/* Go into watering state and open the valve. */
	pot->state.is_watering = 1;
	valve_open(pot);
}

/* The pot controller state machine routine.
 * pot: A pointer to the flowerpot.
 */
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
			/* This pot is disabled. Don't do anything. */
			break;
		}
		if (pot->rem_state.flags & POT_REMFLG_WDTRIGGER) {
			/* The watchdog triggered. Don't do anything. */
			break;
		}

		/* Get the current RTC time. */
		rv3029_get_time(&rtc);
		/* Create a day-of-week mask for today. */
		dow_mask = BITMASK8(rtc.day_of_week);

		if (!(config->dow_on_mask & dow_mask)) {
			/* This pot is disabled on today's weekday. */
			break;
		}

		/* Check if we are in the active-time-range. */
		tod = rtc_get_time_of_day(&rtc);
		if (time_of_day_before(tod, config->active_range.from) ||
		    time_of_day_after(tod, config->active_range.to)) {
			/* Current time is not in the active range.
			 * Don't run.
			 */
			break;
		}

		/* Check, if the next measurement is pending. */
		if (time_before(now, pot->next_measurement)) {
			/* No. Don't run, yet. */
			break;
		}

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

		/* Start a measurement on this pot, now. */
		pot_start_measurement(pot);
		break;
	case POT_MEASURING:
		/* Poll the sensor state. */

		ok = sensor_poll(&result);
		if (!ok) {
			/* The measurement did not finish, yet. */
			break;
		}

		/* Check if verbose logging is requested for this pot
		 * and send the raw measurement result.
		 */
		if (config->flags & POT_FLG_LOGVERBOSE) {
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

		/* Sensor value sanity check.
		 * Too low and too big values are rejected. These values
		 * indicate a short circuit or cable break.
		 */
		if (result.value < 16 || result.value > (SENSOR_MAX - 16)) {
			pot_info(pot, LOG_ERROR, LOG_ERR_SENSOR, pot->nr);
			/* Force-stop watering and bail to idle state. */
			pot_stop_watering(pot);
			break;
		}

		if (pot->state.is_watering) {
			/* We are watering. Check if we reached the upper threshold.
			 * If so, stop watering.
			 * If not, go on watering.
			 */
			if (sensor_val >= config->max_threshold) {
				pot_stop_watering(pot);
			} else {
				/* Not, yet. Check the watchdog. */
				if (pot_check_watchdog(pot)) {
					/* Whoops, it triggered. Abort. */
					break;
				}
				valve_open(pot);
			}
		} else {
			/* We are not watering, yet. Check if we dropped below
			 * the lower threshold.
			 * If so, start watering.
			 * If not, don't do anything and go idle.
			 */
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

/* Completely reset all controllers.
 * This resets all state machines and the corresponding hardware.
 */
static void controller_reset(void)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(cont.pots); i++)
		pot_reset(&cont.pots[i], 1);
	cont.current_pot = 0;
}

/* Get the controller configuration.
 * Copies the current config into "dest".
 * dest: Pointer to the destination buffer.
 */
void controller_get_config(struct controller_config *dest)
{
	*dest = cont.config;
}

/* Set a new controller configuration.
 * Copies the "new_config" into the current config
 * and schedules an EEPROM update.
 * The affected controllers are reset, if the configuration changed.
 * new_config: Pointer to the new configuration.
 */
void controller_update_config(const struct controller_config *new_config)
{
	struct controller_config *active;
	uint8_t i;

	/* Get pointer to the currently active configuration. */
	active = &cont.config;

	/* Check which part of the config changed, if any. */
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
				pot_reset(&cont.pots[i],
					  !(new_config->pots[i].flags & POT_FLG_ENABLED));
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

/* Get the state information for a given pot.
 * pot_number: The number of the pot to get the state for.
 * state: A pointer to the buffer the state will be copied into.
 * rem_state: A pointer to the buffer the remanent state will be copied into.
 */
void controller_get_pot_state(uint8_t pot_number,
			      struct flowerpot_state *state,
			      struct flowerpot_remanent_state *rem_state)
{
	if (pot_number >= ARRAY_SIZE(cont.pots))
		return;

	if (state)
		*state = cont.pots[pot_number].state;
	if (rem_state)
		*rem_state = cont.pots[pot_number].rem_state;
}

/* Update the remanent state on a given pot.
 * pot_number: The number of the pot to set the state for.
 * rem_state: A pointer to the new remanent state.
 */
void controller_update_pot_rem_state(uint8_t pot_number,
				     const struct flowerpot_remanent_state *rem_state)
{
	struct flowerpot *pot;

	if (pot_number >= ARRAY_SIZE(cont.pots))
		return;
	pot = &cont.pots[pot_number];

	if (memcmp(rem_state, &pot->rem_state, sizeof(*rem_state)) == 0) {
		/* Nothing changed. */
		return;
	}

	pot->rem_state = *rem_state;
	pot_remanent_state_commit_eeprom(pot);

	pot_reset(pot, 0);
}

/* Set the "manual mode" control bits.
 * force_stop_watering_mask: A bitmask of pots to force-stop watering on.
 * valve_manual_mask: A bitmask of pots to enable manual valve control on.
 * valve_manual_state: A bitmask of manual-mode valve states.
 * force_start_measurement_mask: A bitmask of pots to force-start measurement on.
 */
void controller_manual_mode(uint8_t force_stop_watering_mask,
			    uint8_t valve_manual_mask,
			    uint8_t valve_manual_state,
			    uint8_t force_start_measurement_mask)
{
	struct flowerpot *pot;
	const struct flowerpot_config *config;
	uint8_t i, mask;

	for (i = 0, mask = 1; i < MAX_NR_FLOWERPOTS; i++, mask <<= 1) {
		pot = &cont.pots[i];
		config = pot_config(pot);

		if ((force_stop_watering_mask & mask) &&
		    pot->state.is_watering)
			pot_stop_watering(pot);

		pot->valve_manual_en = !!(valve_manual_mask & mask);
		pot->valve_manual_state = !!(valve_manual_state & mask);
		valve_state_commit(pot);

		if (force_start_measurement_mask & mask) {
			/* Force measurement state, if enabled and idle. */
			if (pot->state.state_id == POT_IDLE &&
			    (config->flags & POT_FLG_ENABLED) &&
			    !(pot->rem_state.flags & POT_REMFLG_WDTRIGGER))
				pot_state_enter(pot, POT_START_MEASUREMENT);
		}
	}
}

/* Freeze the controller activity.
 * freeze: If true, freeze. Otherwise unfreeze.
 */
void controller_freeze(bool freeze)
{
	cont.frozen = freeze;
	cont.freeze_timeout = jiffies_get() + sec_to_jiffies(5);
}

/* The main controller routine. */
void controller_work(void)
{
	jiffies_t now = jiffies_get();
	enum onoff_state hw_switch = onoffswitch_get_state();

	if (cont.eeprom_update_required &&
	    !time_before(now, cont.eeprom_update_time)) {
		cont.eeprom_update_required = 0;
		/* An EEPROM write was scheduled.
		 * Update the EEPROM contents.
		 * This only updates the bytes that changed. (reduces wearout)
		 */
		eeprom_update_block_wdtsafe(&cont.config, &eeprom_cont_config,
					    sizeof(cont.config));
	}

	if (cont.frozen) {
		if (time_before(now, cont.freeze_timeout)) {
			/* No freeze timeout. */
			return;
		} else {
			/* Timeout. Disable freeze. */
			cont.frozen = 0;
			log_error(LOG_ERR_FREEZE, 0);
		}
	}

	if (hw_switch == ONOFF_SWITCHED_OFF) {
		/* The hardware switch was just switched off. */

		/* Stop everything. */
		controller_reset();
		return;
	} else if (hw_switch == ONOFF_SWITCHED_ON) {
		/* The hardware switch was just switched on. */

		/* Clear all watchdogs and thus re-enable watering, if
		 * watering was stopped due to watchdog. */
		controller_watchdogs_clear();
	} else if (hw_switch == ONOFF_IS_OFF) {
		/* The hardware switch is turned off. */
		return;
	}

	if (!(cont.config.global.flags & CONTR_FLG_ENABLE)) {
		/* The global software switch is turned off. */
		return;
	}

	/* The controller is enabled globally.
	 * Run the pot state machines. */
	handle_pot(&cont.pots[cont.current_pot]);
	cont.current_pot++;
	if (cont.current_pot >= ARRAY_SIZE(cont.pots))
		cont.current_pot = 0;
}

/* Initialization of the controller data structures and hardware. */
void controller_init(void)
{
	struct flowerpot *pot;
	uint8_t i;

	/* Initialize the output extender hardware (shift register).
	 * All valves are connected through this extender.
	 */
	ioext_init(1);

	/* Read the configuration from EEPROM. */
	memset(&cont, 0, sizeof(cont));
	eeprom_read_block_wdtsafe(&cont.config, &eeprom_cont_config,
				  sizeof(cont.config));

	/* Initialize and reset all pot states. */
	for (i = 0; i < ARRAY_SIZE(cont.pots); i++) {
		pot = &cont.pots[i];

		pot->nr = i;
		pot_reset(pot, 1);

		/* Read the remanent pot state. */
		eeprom_read_block_wdtsafe(&pot->rem_state,
					  &eeprom_pot_rem_state[i],
					  sizeof(pot->rem_state));
	}
}
