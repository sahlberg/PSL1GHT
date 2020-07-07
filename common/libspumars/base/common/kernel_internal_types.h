#ifndef __KERNEL_INTERNAL_TYPES_H__
#define __KERNEL_INTERNAL_TYPES_H__

#include <stdint.h>

#include "mars/mutex_types.h"

#include "callback_internal_types.h"
#include "workload_internal_types.h"

#define MARS_KERNEL_ID_NONE							0xffff

#define MARS_KERNEL_TICKS_FLAG_SYNC_BEGIN			0x1
#define MARS_KERNEL_TICKS_FLAG_SYNC_END				0x2

#define MARS_KERNEL_DMA_TAG							31
#define MARS_KERNEL_SPU_EVENT_PORT					63

#define MARS_KERNEL_PARAMS_SIZE						128
#define MARS_KERNEL_PARAMS_ALIGN					128

/* mars kernel syscalls */
struct mars_kernel_syscalls {
	uint32_t                       (*get_ticks)(void);
	uint64_t                       (*get_mars_context_ea)(void);
	uint16_t                       (*get_kernel_id)(void);
	uint16_t                       (*get_workload_id)(void);
	struct mars_workload_context * (*get_workload)(void);
	struct mars_workload_context * (*get_workload_by_id)(uint16_t id);

	void (*workload_exit)(uint8_t state);
	int  (*workload_query)(uint16_t id, int query);
	int  (*workload_wait_set)(uint16_t id);
	int  (*workload_wait_reset)(void);
	int  (*workload_signal_set)(uint16_t id);
	int  (*workload_signal_reset)(void);
	int  (*workload_schedule_begin)(uint16_t id, uint8_t priority,
				struct mars_workload_context **workload);
	int  (*workload_schedule_end)(uint16_t id, int cancel);
	int  (*workload_unschedule_begin)(uint16_t id,
				struct mars_workload_context **workload);
	int  (*workload_unschedule_end)(uint16_t id);
	
	void  (*host_signal_send)(uint64_t watch_point_ea);
	int  (*host_callback_set)(uint64_t callback_ea,
				  const struct mars_callback_args *in);
	int  (*host_callback_reset)(struct mars_callback_args *out);

	int  (*mutex_lock_get)(uint64_t mutex_ea, struct mars_mutex *mutex);
	int  (*mutex_unlock_put)(uint64_t mutex_ea, struct mars_mutex *mutex);

	int  (*dma_get)(void *ls, uint64_t ea, uint32_t size, uint32_t tag);
	int  (*dma_put)(const void *ls, uint64_t ea, uint32_t size,
			uint32_t tag);
	int  (*dma_wait)(uint32_t tag);
};

/* mars kernel ticks */
struct mars_kernel_ticks {
	uint32_t flag;
	uint32_t offset;
};

/* mars kernel parameters */
struct mars_kernel_params {
	struct mars_kernel_ticks kernel_ticks;
	uint64_t mars_context_ea;
	uint64_t workload_queue_ea;
	uint64_t callback_queue_ea;
	uint16_t kernel_id;
	
	uint8_t pad[MARS_KERNEL_PARAMS_SIZE -
				(sizeof(struct mars_kernel_ticks) +
				 sizeof(uint64_t)*3 +
				 sizeof(uint16_t))
			   ];
} __attribute__((aligned(MARS_KERNEL_PARAMS_ALIGN)));

/* mars kernel buffer */
union mars_kernel_buffer {
	struct mars_callback_queue callback_queue;
	struct mars_workload_queue_header workload_queue_header;
	struct mars_workload_queue_block workload_queue_block;
};

/* mars kernel mutex */
int mutex_lock_get(uint64_t mutex_ea, struct mars_mutex *mutex);
int mutex_unlock_put(uint64_t mutex_ea, struct mars_mutex *mutex);

/* mars kernel dma */
int dma_get(void *ls, uint64_t ea, uint32_t size, uint32_t tag);
int dma_put(const void *ls, uint64_t ea, uint32_t size, uint32_t tag);
int dma_wait(uint32_t tag);

#endif
