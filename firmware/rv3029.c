/*
 * RV-3029-C2 RTC driver
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

#include "rv3029.h"
#include "datetime.h"
#include "twi_master.h"

#include <string.h>


/* RV-3029 hardware registers */
enum rv3029_registers {
	/* Control page */
	RV3029_REG_ONOFFCTRL	= 0x00,
	RV3029_REG_IRQCTRL,
	RV3029_REG_IRQFLAGS,
	RV3029_REG_STATUS,
	RV3029_REG_RSTCTRL,

	/* Watch page */
	RV3029_REG_WSECONDS	= 0x08,
	RV3029_REG_WMINUTES,
	RV3029_REG_WHOURS,
	RV3029_REG_WDATE,
	RV3029_REG_WDAYS,
	RV3029_REG_WMONTHS,
	RV3029_REG_WYEARS,

	/* Alarm page */
	RV3029_REG_ASECONDS	= 0x10,
	RV3029_REG_AMINUTES,
	RV3029_REG_AHOURS,
	RV3029_REG_ADATE,
	RV3029_REG_ADAYS,
	RV3029_REG_AMONTHS,
	RV3029_REG_AYEARS,

	/* Timer page */
	RV3029_REG_TIMLOW	= 0x18,
	RV3029_REG_TIMHIGH,

	/* Temperature page */
	RV3029_REG_TEMP		= 0x20,

	/* EEPROM data page */
	RV3029_REG_EEDATA0	= 0x28,
	RV3029_REG_EEDATA1,

	/* EEPROM control page */
	RV3029_REG_EECTRL	= 0x30,
	RV3029_REG_XTALOFFSET,
	RV3029_REG_QCOEF,
	RV3029_REG_TURNOVER,

	/* RAM page */
	RV3029_REG_RAMDATA0	= 0x38,
	RV3029_REG_RAMDATA1,
	RV3029_REG_RAMDATA2,
	RV3029_REG_RAMDATA3,
	RV3029_REG_RAMDATA4,
	RV3029_REG_RAMDATA5,
	RV3029_REG_RAMDATA6,
	RV3029_REG_RAMDATA7,
};

/* RV3029_REG_ONOFFCTRL bits */
enum rv3029_bits_onoffctrl {
	RV3029_ONOFFCTRL_WAON,
	RV3029_ONOFFCTRL_TION,
	RV3029_ONOFFCTRL_TRON,
	RV3029_ONOFFCTRL_EEREFON,
	RV3029_ONOFFCTRL_SRON,
	RV3029_ONOFFCTRL_TD0,
	RV3029_ONOFFCTRL_TD1,
	RV3029_ONOFFCTRL_CLKINT,
};

/* RV3029_REG_IRQCTRL bits */
enum rv3029_bits_irqctrl {
	RV3029_IRQCTRL_AINTE,
	RV3029_IRQCTRL_TINTE,
	RV3029_IRQCTRL_V1INTE,
	RV3029_IRQCTRL_V2INTE,
	RV3029_IRQCTRL_SRINTE,
};

/* RV3029_REG_IRQFLAGS bits */
enum rv3029_bits_irqflags {
	RV3029_IRQFLAGS_AF,
	RV3029_IRQFLAGS_TF,
	RV3029_IRQFLAGS_V1F,
	RV3029_IRQFLAGS_V2F,
	RV3029_IRQFLAGS_SRF,
};

/* RV3029_REG_STATUS bits */
enum rv3029_bits_status {
	RV3029_STATUS_VLOW1	= 2,
	RV3029_STATUS_VLOW2,
	RV3029_STATUS_SR,
	RV3029_STATUS_EEBUSY	= 7,
};

/* RV3029_REG_RSTCTRL bits */
enum rv3029_bits_rstctrl {
	RV3029_RSTCTRL_ALLRES,
	RV3029_RSTCTRL_SYSRES	= 4,
};

/* RV3029_REG_WHOURS bits */
enum rv3029_bits_whours {
	RV3029_WHOURS_PM	= 5,
	RV3029_WHOURS_S1224,
};

/* RV3029_REG_ASECONDS bits */
enum rv3029_bits_aseconds {
	RV3029_ASECONDS_SECEQ	= 7,
};

/* RV3029_REG_AMINUTES bits */
enum rv3029_bits_aminutes {
	RV3029_AMINUTES_MINEQ	= 7,
};

/* RV3029_REG_AHOURS bits */
enum rv3029_bits_ahours {
	RV3029_AHOURS_PM	= 5,
	RV3029_AHOURS_HOUREQ	= 7,
};

/* RV3029_REG_ADATE bits */
enum rv3029_bits_adate {
	RV3029_ADATE_DATEEQ	= 7,
};

/* RV3029_REG_ADAYS bits */
enum rv3029_bits_adays {
	RV3029_ADAYS_DAYEQ	= 7,
};

/* RV3029_REG_AMONTHS bits */
enum rv3029_bits_amonths {
	RV3029_AMONTHS_MONTHEQ	= 7,
};

/* RV3029_REG_AYEARS bits */
enum rv3029_bits_ayears {
	RV3029_AYEARS_YEAREQ	= 7,
};

/* RV3029_REG_EECTRL bits */
enum rv3029_bits_eectrl {
	RV3029_EECTRL_THPER,
	RV3029_EECTRL_THEN,
	RV3029_EECTRL_FD0,
	RV3029_EECTRL_FD1,
	RV3029_EECTRL_R1K,
	RV3029_EECTRL_R5K,
	RV3029_EECTRL_R20K,
	RV3029_EECTRL_R80K,
};

/* RV-3029 device state. */
struct rv3029_device {
	/* I2C transfer context. */
	struct twi_transfer xfer;
	/* I2C transfer data buffer. */
	uint8_t xfer_buffer[8];

	/* Cached watch time. */
	struct rtc_time now;
};

/* Instance of the device state. */
static struct rv3029_device rv3029_dev;


/* I2C address of the RV-3029 device. */
#define RV3029_I2C_ADDRESS	0x56
/* I2C transfer timeout, in milliseconds. */
#define RV3029_I2C_TIMEOUT	50


/* Schedule an asynchronous multi-byte register write.
 * reg: The hardware register to start writing to.
 * buffer: The data buffer.
 * count: The number of bytes to write.
 * callback: The callback to call on completion or error.
 *           May be NULL.
 */
static void rv3029_write_async(uint8_t reg,
			       const void *buffer, uint8_t count,
			       twi_callback_t callback)
{
	struct rv3029_device *dev = &rv3029_dev;

	/* Wait for a possible previous transfer to finish. */
	twi_transfer_wait(&dev->xfer, RV3029_I2C_TIMEOUT);

	count = min(count, sizeof(dev->xfer_buffer) - 1);

	/* The first byte is the register offset. */
	dev->xfer_buffer[0] = reg;
	/* Append the payload data. */
	memcpy(dev->xfer_buffer + 1, buffer, count);

	dev->xfer.write_size = count + 1;
	dev->xfer.read_size = 0;
	dev->xfer.callback = callback;

	/* Schedule the I2C transfer. */
	twi_transfer(&dev->xfer);
}

/* Synchronous multi-byte register write.
 * reg: The hardware register to start writing to.
 * buffer: The data buffer.
 * count: The number of bytes in buffer.
 * Returns 1 on success, or 0 on I2C error.
 */
static bool rv3029_write(uint8_t reg,
			 const void *buffer, uint8_t count)
{
	struct rv3029_device *dev = &rv3029_dev;
	enum twi_status stat;

	/* Schedule an asynchronous write and wait afterwards. */
	rv3029_write_async(reg, buffer, count, NULL);
	stat = twi_transfer_wait(&dev->xfer, RV3029_I2C_TIMEOUT);
	if (stat != TWI_STAT_FINISHED)
		return 0;

	return 1;
}

/* Synchronously read y byte-wide register.
 * reg: The hardware register to write.
 * value: The value to write.
 * Returns 1 on success, or 0 on I2C error.
 */
static bool rv3029_write_byte(uint8_t reg, uint8_t value)
{
	return rv3029_write(reg, &value, sizeof(value));
}

/* Schedule an asynchronous multi-byte register read.
 * reg: The hardware register to start reading from.
 * count: The number of bytes to read.
 * callback: The callback to call on completion or error.
 *           May be NULL.
 */
static void rv3029_read_async(uint8_t reg, uint8_t count,
			      twi_callback_t callback)
{
	struct rv3029_device *dev = &rv3029_dev;

	/* Wait for a possible previous transfer to finish. */
	twi_transfer_wait(&dev->xfer, RV3029_I2C_TIMEOUT);

	count = min(count, sizeof(dev->xfer_buffer));

	/* Write the register offset (one byte). */
	dev->xfer_buffer[0] = reg;

	dev->xfer.write_size = 1;
	dev->xfer.read_size = count;
	dev->xfer.callback = callback;

	/* Schedule the I2C transfer. */
	twi_transfer(&dev->xfer);
}

/* Synchronous multi-byte register read.
 * reg: The hardware register to start reading from.
 * count: The number of bytes to read.
 * Returns a pointer to the receive buffer on success,
 * or NULL on error.
 */
static const uint8_t * rv3029_read(uint8_t reg, uint8_t count)
{
	struct rv3029_device *dev = &rv3029_dev;
	enum twi_status stat;

	/* Schedule an asynchronous read and wait afterwards. */
	rv3029_read_async(reg, count, NULL);
	stat = twi_transfer_wait(&dev->xfer, RV3029_I2C_TIMEOUT);
	if (stat != TWI_STAT_FINISHED)
		return NULL;

	return dev->xfer_buffer;
}

/* Synchronously read a byte-wide register.
 * reg: The hardware register to read. */
static uint8_t rv3029_read_byte(uint8_t reg)
{
	const uint8_t *buffer;
	uint8_t retval = 0;

	buffer = rv3029_read(reg, sizeof(retval));
	if (buffer)
		retval = buffer[0];

	return retval;
}

/* Check whether the RV-3029 EEPROM is busy. */
static bool rv3029_eeprom_busy(void)
{
	return !!(rv3029_read_byte(RV3029_REG_STATUS) &
		  (1 << RV3029_STATUS_EEBUSY));
}

/* Write to RV-3029 EEPROM.
 * reg: The EEPROM register.
 * value: The value to write.
 */
static void rv3029_eeprom_write(uint8_t reg, uint8_t value)
{
	uint8_t old_onoffctrl, old_value, status;

	/* Wait for voltage level Vcc > Vprog */
	status = rv3029_read_byte(RV3029_REG_STATUS);
	if (status & ((1 << RV3029_STATUS_VLOW1) |
		      (1 << RV3029_STATUS_VLOW2))) {
		while (1) {
			status = rv3029_read_byte(RV3029_REG_STATUS);
			if (!(status & ((1 << RV3029_STATUS_VLOW1) |
				        (1 << RV3029_STATUS_VLOW2)))) {
				_delay_ms(50);
				break;
			}
			status &= ~(1 << RV3029_STATUS_VLOW1);
			status &= ~(1 << RV3029_STATUS_VLOW2);
			rv3029_write_byte(RV3029_REG_STATUS, status);
		}
	}

	/* Clear EERefOn bit before accessing EEPROM */
	old_onoffctrl = rv3029_read_byte(RV3029_REG_ONOFFCTRL);
	rv3029_write_byte(RV3029_REG_ONOFFCTRL,
			  old_onoffctrl & ~(1 << RV3029_ONOFFCTRL_EEREFON));

	while (rv3029_eeprom_busy())
		; /* Busy-wait */

	/* Write the new value, if it changed. */
	old_value = rv3029_read_byte(reg);
	if (value != old_value)
		rv3029_write_byte(reg, value);

	while (rv3029_eeprom_busy())
		; /* Busy-wait */

	/* Restore EERefOn bit */
	rv3029_write_byte(RV3029_REG_ONOFFCTRL, old_onoffctrl);
}

/* Convert a given time from hardware-BCD-format to binary format. */
static void rv3029_time_bcd_to_bin(struct rtc_time *bin_time,
				   const struct rtc_time *bcd_time)
{
	/* Convert rtc_time from BCD to binary. */
	rtc_time_from_bcd(bin_time, bcd_time);

	/* In the hardware format the day count, month count and
	 * weekday count are 1-based. In the binary rtc_time format
	 * these are 0-based. So subtract 1 here to account for this.
	 */
	bin_time->day -= 1;
	bin_time->month -= 1;
	bin_time->day_of_week -= 1;
}

/* Convert a given time from binary format to hardware-BCD-format. */
static void rv3029_time_bin_to_bcd(struct rtc_time *bcd_time,
				   const struct rtc_time *bin_time)
{
	struct rtc_time tmp_bin_time = *bin_time;

	/* In the hardware format the day count, month count and
	 * weekday count are 1-based. In the binary rtc_time format
	 * these are 0-based. So add 1 here to account for this.
	 * Also limit year to 79, because this is the max value according
	 * to the datasheet.
	 */
	tmp_bin_time.day += 1;
	tmp_bin_time.month += 1;
	tmp_bin_time.day_of_week += 1;
	tmp_bin_time.year = clamp(tmp_bin_time.year, 0, 79);

	rtc_time_to_bcd(bcd_time, &tmp_bin_time);
}

/* Write "time" to the RTC's watch registers. */
void rv3029_write_time(const struct rtc_time *time)
{
	struct rv3029_device *dev = &rv3029_dev;
	struct rtc_time bcd_time;
	uint8_t buffer[7];
	uint8_t sreg;

	/* Convert to BCD hardware format. */
	rv3029_time_bin_to_bcd(&bcd_time, time);

	/* Prepare the I2C write buffer. */
	buffer[0] = bcd_time.second;
	buffer[1] = bcd_time.minute;
	buffer[2] = bcd_time.hour & ~(1 << RV3029_WHOURS_S1224);
	buffer[3] = bcd_time.day;
	buffer[4] = bcd_time.day_of_week;
	buffer[5] = bcd_time.month;
	buffer[6] = bcd_time.year;

	/* Schedule the write of the watch hardware registers. */
	rv3029_write(RV3029_REG_WSECONDS, buffer, sizeof(buffer));

	/* Update the cached time. */
	sreg = irq_disable_save();
	dev->now = *time;
	irq_restore(sreg);
}

/* Async-read callback */
static void read_time_callback(struct twi_transfer *xfer, enum twi_status status)
{
	struct rv3029_device *dev = &rv3029_dev;
	struct rtc_time bcd_time;
	uint8_t sreg;

	if (status != TWI_STAT_FINISHED) {
		/* I2C finished with an error. */
		return;
	}

	/* Read the transfer buffer, with interrupts disabled. */
	sreg = irq_disable_save();
	bcd_time.second = dev->xfer_buffer[0];
	bcd_time.minute = dev->xfer_buffer[1];
	bcd_time.hour = dev->xfer_buffer[2] & ~(1 << RV3029_WHOURS_S1224);
	bcd_time.day = dev->xfer_buffer[3];
	bcd_time.day_of_week = dev->xfer_buffer[4];
	bcd_time.month = dev->xfer_buffer[5];
	bcd_time.year = dev->xfer_buffer[6];
	irq_restore(sreg);

	/* Convert from BCD hardware format to binary and store in cache. */
	rv3029_time_bcd_to_bin(&dev->now, &bcd_time);
}

/* Update the cached time. */
void rv3029_read_time(void)
{
	/* Poke an asynchronous read of the watch registers.
	 * read_time_callback() will be called after I2C read finished.
	 */
	rv3029_read_async(RV3029_REG_WSECONDS, 7, read_time_callback);
}

/* Returns the currently cached time.
 * time: Pointer to the destination buffer.
 */
void rv3029_get_time(struct rtc_time *time)
{
	struct rv3029_device *dev = &rv3029_dev;
	uint8_t sreg;

	sreg = irq_disable_save();
	*time = dev->now;
	irq_restore(sreg);
}

/* Initialize the RTC */
void rv3029_init(void)
{
	struct rv3029_device *dev = &rv3029_dev;
	uint8_t tmp;

	/* Reset the device data structure. */
	memset(dev, 0, sizeof(*dev));

	/* Initialize I2C transfer data structure. */
	twi_transfer_init(&dev->xfer);
	dev->xfer.address = RV3029_I2C_ADDRESS;
	dev->xfer.buffer = dev->xfer_buffer;

	/* Reset the device */
	rv3029_write_byte(RV3029_REG_RSTCTRL, (1 << RV3029_RSTCTRL_SYSRES));
	_delay_ms(25);

	/* Clear status bits */
	rv3029_write_byte(RV3029_REG_STATUS, 0);

	/* Disable interrupts */
	rv3029_write_byte(RV3029_REG_IRQCTRL, 0);
	/* Clear interrupt flags */
	rv3029_write_byte(RV3029_REG_IRQFLAGS, 0);

	/* Enable thermometer, scan period 1 second,
	 * Timer 32786 Hz,
	 * 1k trickle charge resistor.
	 */
	tmp = (1 << RV3029_EECTRL_THEN) |
	      (1 << RV3029_EECTRL_R1K);
	rv3029_eeprom_write(RV3029_REG_EECTRL, tmp);

	/* Enable IRQ pin,
	 * Timer frequency 32 Hz,
	 * Enable self recovery.
	 * Enable EEPROM refresh.
	 * Enable timer auto reload.
	 * Disable timer.
	 * Enable watch.
	 */
	tmp = (1 << RV3029_ONOFFCTRL_SRON) |
	      (1 << RV3029_ONOFFCTRL_EEREFON) |
	      (1 << RV3029_ONOFFCTRL_TRON) |
	      (1 << RV3029_ONOFFCTRL_WAON);
	rv3029_write_byte(RV3029_REG_ONOFFCTRL, tmp);
}
