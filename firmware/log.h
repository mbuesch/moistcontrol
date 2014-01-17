#ifndef LOG_H_
#define LOG_H_

#include "util.h"
#include "datetime.h"


/* Log message types. */
enum log_type_flags {
	/* Types */
	LOG_ERROR,
	LOG_INFO,
	LOG_SENSOR_DATA,

	LOG_TYPE_MASK	= 0x7F,
	LOG_FLAGS_MASK	= 0x80,

	/* Overflow flag. */
	LOG_OVERFLOW	= 0x80,
};

enum log_error {
	LOG_ERR_SENSOR,			/* Sensor short circuit. */
	LOG_ERR_WATERDOG,		/* Watering-watchdog fired. */
	LOG_ERR_FREEZE,			/* Freeze timeout. */
};

enum log_info {
	LOG_INFO_DEBUG,			/* Generic debug message. */
	LOG_INFO_CONTSTATCHG,		/* Controller status change */
	LOG_INFO_WATERINGCHG,		/* The "watering" state changed. */
};

/* Construct a 'sensor_data' field. */
#define LOG_SENSOR_DATA(sensor_nr, value)	\
	(((sensor_nr) << 10) | ((value) & 0x3FF))

/* Log message item. */
struct log_item {
	/* Log message type and flags. */
	uint8_t type_flags;
	/* Timestamp of the event occurrence. */
	timestamp_t time;

	union {
		/* LOG_ERROR and LOG_INFO */
		struct {
			/* Error/info code. */
			uint8_t code;
			/* Error/info payload. */
			uint8_t data;
		} _packed;

		/* LOG_SENSOR_DATA */
		struct {
			/* Sensor number and value combined.
			 * See LOG_SENSOR_DATA() macro for
			 * creating this field. */
			uint16_t sensor_data;
		} _packed;
	} _packed;
} _packed;


void log_init(struct log_item *item, uint8_t type);
void log_append(const struct log_item *item);
bool log_pop(struct log_item *item);

static inline void log_debug(uint8_t data)
{
	struct log_item item;

	log_init(&item, LOG_INFO);
	item.code = LOG_INFO_DEBUG;
	item.data = data;
	log_append(&item);
}

#endif /* LOG_H_ */
