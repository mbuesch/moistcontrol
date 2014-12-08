#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include "util.h"
#include "datetime.h"
#include "onoffswitch.h"


/* The maximum possible number of flower-pots. */
#define MAX_NR_FLOWERPOTS	6


/* Flower-pot configuration flags. */
enum flowerpot_config_flag {
	/* Pot is enabled.
	 * If this bit is not set, the regulator is disabled.
	 */
	POT_FLG_ENABLED		= 0x01,
	/* Logging is enabled.
	 * If this bit is not set, logs will not be written.
	 */
	POT_FLG_LOG		= 0x02,
	/* Verbose logging is enabled.
	 * If this bit is not set, no verbose logs will be written.
	 */
	POT_FLG_LOGVERBOSE	= 0x04,
};

/* Configuration of one flower-pot. */
struct flowerpot_config {
	/* Boolean flags. See 'enum flowerpot_config_flags'. */
	uint8_t flags;
	/* The lower threshold value for the regulator. */
	uint8_t min_threshold;
	/* The upper threshold value for the regulator. */
	uint8_t max_threshold;
	/* The time of day range where the regulator is active.
	 * The regulator will be disabled outside of this time. */
	struct time_of_day_range active_range;
	/* Day-of-week ON mask. Bit 0 -> monday, Bit 1 -> tuesday, etc...
	 * If a bit it set, the regulator will be enabled on
	 * that weekday.
	 */
	uint8_t dow_on_mask;
};

enum controller_global_flags {
	/* Global controller-enable bit.
	 * If this bit is not set, the controller is disabled globally.
	 */
	CONTR_FLG_ENABLE	= 0x01,
};

/* Global controller configuration. */
struct controller_global_config {
	/* Global configuration flags.
	 * See 'enum controller_global_flags'
	 */
	uint8_t flags;
	/* Global lowest possible value of the raw sensor values. */
	uint16_t sensor_lowest_value;
	/* Global highest possible value of the raw sensor values. */
	uint16_t sensor_highest_value;
};

/* Controller configuration. */
struct controller_config {
	/* Per-pot configuration. */
	struct flowerpot_config pots[MAX_NR_FLOWERPOTS];
	/* Global config options. */
	struct controller_global_config global;
};

/* Flowerpot state-machine state-ID numbers. */
enum flowerpot_state_id {
	/* POT_IDLE: The controller is waiting for the next
	 *	measurement to happen.
	 */
	POT_IDLE = 0,
	/* POT_START_MEASUREMENT: The controller is going to start
	 *	a new measurement, as soon as the sensors
	 *	are available.
	 */
	POT_START_MEASUREMENT,
	/* POT_MEASURING: The controller is performing a
	 *	sensor measurement.
	 */
	POT_MEASURING,
	/* POT_WAITING_FOR_VALVE: The controller is waiting
	 *	for the last valve-action to finish.
	 */
	POT_WAITING_FOR_VALVE,
};

/* The flowerpot state-machine state. */
struct flowerpot_state {
	/* The current state-ID number. */
	enum flowerpot_state_id state_id;
	/* Are we currently watering? */
	bool is_watering;
	/* A copy of the last raw ADC sensor value. */
	uint16_t last_measured_raw_value;
	/* A copy of the last scaled sensor value. */
	uint8_t last_measured_value;
};

enum flowerpot_remanent_flags {
	/* The watering watchdog on this pot triggered.
	 * The regulator will be disabled.
	 */
	POT_REMFLG_WDTRIGGER	= 0x01,
};

/* The flowerpot state-machine remanent state. */
struct flowerpot_remanent_state {
	/* Remanent state flags bitfield. */
	uint8_t flags;
};

void controller_get_config(struct controller_config *dest);
void controller_update_config(const struct controller_config *src);

void controller_get_pot_state(uint8_t pot_number,
			      struct flowerpot_state *state,
			      struct flowerpot_remanent_state *rem_state);
void controller_update_pot_rem_state(uint8_t pot_number,
				     const struct flowerpot_remanent_state *rem_state);

void controller_manual_mode(uint8_t force_stop_watering_mask,
			    uint8_t valve_manual_mask,
			    uint8_t valve_manual_state);

void controller_freeze(bool freeze);

void controller_work(enum onoff_state hw_switch);
void controller_init(void);

#endif /* CONTROLLER_H_ */
