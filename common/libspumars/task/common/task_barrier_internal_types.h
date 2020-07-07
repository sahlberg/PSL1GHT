#ifndef __MARS_TASK_BARRIER_INTERNAL_TYPES_H__
#define __MARS_TASK_BARRIER_INTERNAL_TYPES_H__

#include <stdint.h>

#define MARS_TASK_BARRIER_SIZE				128
#define MARS_TASK_BARRIER_ALIGN				128
#define MARS_TASK_BARRIER_ALIGN_MASK		0x7f

struct mars_task_barrier {
	uint32_t lock;
	uint32_t total;
	uint32_t notified_count;
	uint32_t waited_count;
	uint16_t notify_wait_count;
	uint16_t notify_wait_id[MARS_TASK_BARRIER_WAIT_MAX];
	uint16_t wait_count;
	uint16_t wait_id[MARS_TASK_BARRIER_WAIT_MAX];
	uint64_t mars_context_ea;
} __attribute__((aligned(MARS_TASK_BARRIER_ALIGN)));

#endif
