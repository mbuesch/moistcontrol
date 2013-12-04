#ifndef TWI_MASTER_MAC_H_
#define TWI_MASTER_MAC_H_

#include <stdint.h>
#include <string.h>

#include "util.h"


struct twi_transfer;

enum twi_status {
	TWI_STAT_IDLE = 0,
	TWI_STAT_INPROGRESS,
	TWI_STAT_FINISHED,
	TWI_STAT_BUSERROR,
	TWI_STAT_TIMEOUT,
	TWI_STAT_CANCELLED,
};

#if defined(TWI_SIZE_16BIT) && (TWI_SIZE_16BIT != 0)
typedef uint16_t twi_size_t;
#else
typedef uint8_t twi_size_t;
#endif

typedef void (*twi_callback_t)(struct twi_transfer *, enum twi_status);

struct twi_transfer {
	void *buffer;
	twi_size_t write_size;
	twi_size_t read_size;

	uint8_t address;

	twi_callback_t callback;

	/* Internal fields follow. */
	uint8_t status;			/* enum twi_transfer_status_flags */
	twi_size_t offset;		/* The current byte offset. */
	struct twi_transfer *next;	/* Linked list of transfer objects. */
};

void twi_init(void);

static inline void twi_transfer_init(struct twi_transfer *xfer)
{
	memset(xfer, 0, sizeof(*xfer));
}

void twi_transfer(struct twi_transfer *xfer);
enum twi_status twi_transfer_get_status(const struct twi_transfer *xfer);
enum twi_status twi_transfer_wait(struct twi_transfer *xfer,
				  uint8_t timeout_ms);
void twi_transfer_cancel(struct twi_transfer *xfer);

#endif /* TWI_MASTER_MAC_H_ */
