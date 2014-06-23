#ifndef IO_EXTENDER_H_
#define IO_EXTENDER_H_

#include "pcf8574.h"
#include "util.h"


enum ioext_bits {
	EXTOUT_VALVE0,
	EXTOUT_VALVE1,
	EXTOUT_VALVE2,
	EXTOUT_VALVE3,
	EXTOUT_VALVE4,
	EXTOUT_VALVE5,
};

#define EXTOUT_NR_CHIPS		1

struct ioext_context {
	struct pcf8574_chip chips[EXTOUT_NR_CHIPS];
	uint8_t old_states[EXTOUT_NR_CHIPS];
	uint8_t states[EXTOUT_NR_CHIPS];
};

extern struct ioext_context ioext_ctx;


static inline bool ioext_bit_is_set(uint8_t bit_number)
{
	uint8_t chip = bit_number / 8;
	uint8_t bit = bit_number % 8;

	return (ioext_ctx.states[chip] >> bit) & 1;
}

static inline bool ioext_bit_is_clear(uint8_t bit_number)
{
	return !ioext_bit_is_set(bit_number);
}

static inline void ioext_set_bit(uint8_t bit_number)
{
	uint8_t chip = bit_number / 8;
	uint8_t bit = bit_number % 8;

	ioext_ctx.states[chip] |= BITMASK8(bit);
}

static inline void ioext_clear_bit(uint8_t bit_number)
{
	uint8_t chip = bit_number / 8;
	uint8_t bit = bit_number % 8;

	ioext_ctx.states[chip] &= ~BITMASK8(bit);
}

static inline void ioext_write_bit(uint8_t bit_number, bool set)
{
	if (set)
		ioext_set_bit(bit_number);
	else
		ioext_clear_bit(bit_number);
}

void ioext_commit(void);

void ioext_init(bool all_ones);

#endif /* IO_EXTENDER_H_ */
