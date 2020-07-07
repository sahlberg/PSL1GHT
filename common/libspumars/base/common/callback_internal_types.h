#ifndef __MARS_CALLBACK_INTERNAL_TYPES_H__
#define __MARS_CALLBACK_INTERNAL_TYPES_H__

#define MARS_CALLBACK_QUEUE_SIZE				128		/* size of 128 bytes */
#define MARS_CALLBACK_QUEUE_ALIGN				128		/* align to 128 bytes */
#define MARS_CALLBACK_QUEUE_MAX					54		/* max depth of queue */

#define MARS_CALLBACK_QUEUE_FLAG_NONE			0x0		/* no flag set */
#define MARS_CALLBACK_QUEUE_FLAG_EXIT			0x1		/* exit flag */
#define MARS_CALLBACK_QUEUE_FLAG_PUSH   		0x2		/* push flag */

/* 128 byte callback queue structure */
struct mars_callback_queue {
	uint32_t lock;
	uint32_t flag;
	uint32_t count;
	uint32_t head;
	uint32_t tail;
	uint16_t workload_id[MARS_CALLBACK_QUEUE_MAX];
} __attribute__((aligned(MARS_CALLBACK_QUEUE_ALIGN)));

#endif
