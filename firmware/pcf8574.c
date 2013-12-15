/*
 * PCF-8574 shift register
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

#include "pcf8574.h"
#include "twi_master.h"
#include "util.h"


/* I2C base-address of the PCF-8574 */
#define PCF8574_ADDR_BASE	0x20
/* I2C base-address of the PCF-8574-A */
#define PCF8574A_ADDR_BASE	0x38


/* Wait for the scheduled transfer to finish.
 * If there is no scheduled transfer, this function
 * returns immediately.
 */
void pcf8574_wait(struct pcf8574_chip *chip)
{
	twi_transfer_wait(&chip->xfer, 100);
}

/* Asynchronously write the output states of a PCF-8574 chip.
 * The actual write might not have completed after return
 * from this function.
 * chip: The PCF-8574 chip instance.
 * write_value: The 8-bit value to write.
 */
void pcf8574_write(struct pcf8574_chip *chip,
		   uint8_t write_value)
{
	/* Wait for previous transfer to finish, if any. */
	pcf8574_wait(chip);

	/* Prepare the I2C transfer context. */
	chip->xfer.buffer = &write_value;
	chip->xfer.write_size = sizeof(write_value);
	chip->xfer.read_size = 0;

	/* Trigger the I2C transfer. */
	twi_transfer(&chip->xfer);
}

/* Synchronously read the input states of a PCF-8574 chip.
 * chip: The PCF-8574 chip instance.
 * Returns the 8-bit chip-input value.
 */
uint8_t pcf8574_read(struct pcf8574_chip *chip)
{
	uint8_t read_value;

	/* Wait for previous transfer to finish, if any. */
	pcf8574_wait(chip);

	/* Prepare the I2C transfer context. */
	chip->xfer.buffer = &read_value;
	chip->xfer.write_size = 0;
	chip->xfer.read_size = sizeof(read_value);

	/* Trigger the I2C transfer and wait for it to finish. */
	twi_transfer(&chip->xfer);
	pcf8574_wait(chip);

	return read_value;
}

/* Synchronously write the output states and read the input states
 * of a PCF-8574 chip.
 * This function first writes and then reads.
 * chip: The PCF-8574 chip instance.
 * write_value: The 8-bit value to write.
 * Returns the 8-bit chip-input value.
 */
uint8_t pcf8574_write_read(struct pcf8574_chip *chip,
			   uint8_t write_value)
{
	uint8_t buffer = write_value;

	/* Wait for previous transfer to finish, if any. */
	pcf8574_wait(chip);

	/* Prepare the I2C transfer context. */
	chip->xfer.buffer = &buffer;
	chip->xfer.write_size = sizeof(buffer);
	chip->xfer.read_size = sizeof(buffer);

	/* Trigger the I2C transfer and wait for it to finish. */
	twi_transfer(&chip->xfer);
	pcf8574_wait(chip);

	return buffer;
}

/* Initialize a PCF-8574 context structure.
 * address: The I2C sub-address that is physically
 *          configured on the chip pins (Lower 3 bits).
 * chipversion_A: 1: The chip is a PCF-8574-A.
 *                0: The chip is a PCF-8574.
 * initial_state: 1: All bits are initialized to 1.
 *                0: All bits are initialized to 0.
 */
void pcf8574_init(struct pcf8574_chip *chip,
		  uint8_t address, bool chipversion_A,
		  bool initial_state)
{
	/* Initialize the I2C transfer structure. */
	twi_transfer_init(&chip->xfer);

	/* Calculate the I2C address, based on the
	 * Chip version and the sub-address. */
	if (chipversion_A)
		chip->xfer.address = PCF8574A_ADDR_BASE + (address & 7);
	else
		chip->xfer.address = PCF8574_ADDR_BASE + (address & 7);

	/* Initially write all output states. */
	pcf8574_write(chip, initial_state ? 0xFF : 0);
}
