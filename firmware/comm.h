#ifndef COMM_H_
#define COMM_H_

#include "util.h"

#include <stdint.h>

#include <avr/pgmspace.h>


#ifndef COMM_PAYLOAD_LEN
# define COMM_PAYLOAD_LEN		8
#endif

#ifndef COMM_LOCAL_ADDRESS
# define COMM_LOCAL_ADDRESS		0
#endif

#ifndef COMM_BAUDRATE
# define COMM_BAUDRATE			9600
#endif

#ifndef COMM_TX_QUEUE_SIZE
# define COMM_TX_QUEUE_SIZE		4	/* Must be power of two */
#endif
#define COMM_TX_QUEUE_MASK	(COMM_TX_QUEUE_SIZE - 1)

#ifndef COMM_RX_QUEUE_SIZE
# define COMM_RX_QUEUE_SIZE		4	/* Must be power of two */
#endif
#define COMM_RX_QUEUE_MASK	(COMM_RX_QUEUE_SIZE - 1)


enum comm_frame_control {
	COMM_FC_RESET		= 0x01,
	COMM_FC_REQ_ACK		= 0x02,
	COMM_FC_ACK		= 0x04,

	COMM_FC_ERRCODE		= 0xC0,
	COMM_FC_ERRCODE_SHIFT	= 6,
};

enum comm_errcode {
	COMM_ERR_OK,		/* Ok. */
	COMM_ERR_FAIL,		/* Failure. */
	COMM_ERR_FCS,		/* Checksum error. */
	COMM_ERR_Q,		/* Queue overflow. */
};

typedef uint16_t comm_crc_t;			/* little endian checksum*/

#define COMM_HDR_LEN			4
#define COMM_FCS_LEN			sizeof(comm_crc_t)

struct comm_message {
	uint8_t fc;				/* Frame control. */
	uint8_t seq;				/* Sequence number. */
	uint8_t addr;				/* Source and destination address. */
	uint8_t reserved;
	uint8_t payload[COMM_PAYLOAD_LEN];	/* Payload. */
	comm_crc_t fcs;				/* Frame check sequence. */
} _packed;

#define COMM_MSG_INIT()	{				\
	.fc		= 0,				\
	.seq		= 0,				\
	.addr		= COMM_LOCAL_ADDRESS & 0x0F,	\
}

#define COMM_MSG(_name)				\
	struct comm_message _name = COMM_MSG_INIT()

static inline uint8_t comm_msg_err(const struct comm_message *msg)
{
	return (msg->fc & COMM_FC_ERRCODE) >> COMM_FC_ERRCODE_SHIFT;
}

static inline void comm_msg_set_err(struct comm_message *msg, uint8_t err)
{
	msg->fc = (msg->fc & ~COMM_FC_ERRCODE) | (err << COMM_FC_ERRCODE_SHIFT);
}

static inline uint8_t comm_msg_sa(const struct comm_message *msg)
{
	return msg->addr & 0x0F;
}

static inline void comm_msg_set_sa(struct comm_message *msg, uint8_t sa)
{
	msg->addr = (msg->addr & 0xF0) | (sa & 0x0F);
}

static inline uint8_t comm_msg_da(const struct comm_message *msg)
{
	return msg->addr >> 4;
}

static inline void comm_msg_set_da(struct comm_message *msg, uint8_t da)
{
	msg->addr = (msg->addr & 0x0F) | (da << 4);
}

#define comm_payload(payload_ptr_type, msg)	((payload_ptr_type)((msg)->payload))

void comm_init(void);

void comm_work(void);
void comm_centisecond_tick(void);

void comm_message_send(struct comm_message *msg, uint8_t dest_addr);
void comm_drain_tx_queue(void);

extern bool comm_handle_rx_message(const struct comm_message *msg,
				   void *reply_payload);

#endif /* COMM_H_ */
