#ifndef __MARS_TASK_EVENT_FLAG_INTERNAL_TYPES_H__
#define __MARS_TASK_EVENT_FLAG_INTERNAL_TYPES_H__

#include <stdint.h>

#define MARS_TASK_EVENT_FLAG_SIZE			128
#define MARS_TASK_EVENT_FLAG_ALIGN			128
#define MARS_TASK_EVENT_FLAG_ALIGN_MASK		0x7f

struct mars_task_event_flag {
	uint32_t lock;
	uint32_t bits;
	uint8_t direction;
	uint8_t clear_mode;
	uint16_t wait_count;
	uint16_t wait_id[MARS_TASK_EVENT_FLAG_WAIT_MAX + 1];
	uint32_t wait_mask[MARS_TASK_EVENT_FLAG_WAIT_MAX];
	uint8_t wait_mask_mode[MARS_TASK_EVENT_FLAG_WAIT_MAX + 1];
	uint64_t mars_context_ea;
} __attribute__((aligned(MARS_TASK_EVENT_FLAG_ALIGN)));

#endif
