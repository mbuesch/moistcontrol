#ifndef PCF8574_H_
#define PCF8574_H_

#include <stdint.h>

#include "twi_master.h"
#include "util.h"


/* PCF-8574 chip context. */
struct pcf8574_chip {
	/* I2C transfer context. */
	struct twi_transfer xfer;
};

void pcf8574_init(struct pcf8574_chip *chip,
		  uint8_t address, bool chipversion_A,
		  bool initial_state);

void pcf8574_write(struct pcf8574_chip *chip,
		   uint8_t write_value);
void pcf8574_wait(struct pcf8574_chip *chip);

uint8_t pcf8574_read(struct pcf8574_chip *chip);

uint8_t pcf8574_write_read(struct pcf8574_chip *chip,
			   uint8_t write_value);

#endif /* PCF8574_H_ */
