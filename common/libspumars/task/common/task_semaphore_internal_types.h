#ifndef __MARS_TASK_SEMAPHORE_INTERNAL_TYPES_H__
#define __MARS_TASK_SEMAPHORE_INTERNAL_TYPES_H__

#include <stdint.h>

#define MARS_TASK_SEMAPHORE_SIZE			128
#define MARS_TASK_SEMAPHORE_ALIGN			128
#define MARS_TASK_SEMAPHORE_ALIGN_MASK		0x7f

struct mars_task_semaphore {
	uint32_t lock;
	int32_t count;
	uint16_t wait_count;
	uint16_t wait_id[MARS_TASK_SEMAPHORE_WAIT_MAX];
	uint8_t wait_head;
	uint8_t pad;
	uint64_t mars_context_ea;
} __attribute__((aligned(MARS_TASK_SEMAPHORE_ALIGN)));

#endif
