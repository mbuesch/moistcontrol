#ifndef SENSOR_H_
#define SENSOR_H_

#include "util.h"

#include <stdint.h>


/* Measurement result. */
struct sensor_result {
	/* The number of the sensor this result belongs to. */
	uint8_t nr;
	/* The raw ADC value of the measurement. */
	uint16_t value;
};

/* The largest sensor ADC value. */
#define SENSOR_MAX	0x3FF

void sensor_start(uint8_t nr);
void sensor_cancel(void);
bool sensor_poll(struct sensor_result *res);

bool sensors_idle(void);

void sensor_init(void);

#endif /* SENSOR_H_ */
