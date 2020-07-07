#ifndef __MARS_TASK_QUEUE_INTERNAL_TYPES_H__
#define __MARS_TASK_QUEUE_INTERNAL_TYPES_H__

#include <stdint.h>

#define MARS_TASK_QUEUE_SIZE				128
#define MARS_TASK_QUEUE_ALIGN				128
#define MARS_TASK_QUEUE_ALIGN_MASK			0x7f
#define MARS_TASK_QUEUE_ENTRY_SIZE_MASK		0xf
#define MARS_TASK_QUEUE_ENTRY_ALIGN			16
#define MARS_TASK_QUEUE_ENTRY_ALIGN_MASK	0xf
#define MARS_TASK_QUEUE_BUFFER_ALIGN		16
#define MARS_TASK_QUEUE_BUFFER_ALIGN_MASK	0xf

struct mars_task_queue {
	uint32_t lock;
	uint32_t size;
	uint32_t depth;
	uint32_t count;
	uint64_t buffer_ea;
	uint64_t push_ea;
	uint64_t pop_ea;
	uint8_t pad;
	uint8_t direction;
	uint8_t push_wait_head;
	uint8_t pop_wait_head;
	uint16_t push_wait_count;
	uint16_t pop_wait_count;
	uint16_t push_wait_id[MARS_TASK_QUEUE_WAIT_MAX];
	uint16_t pop_wait_id[MARS_TASK_QUEUE_WAIT_MAX];
	uint64_t mars_context_ea;
} __attribute__((aligned(MARS_TASK_QUEUE_ALIGN)));

#endif
