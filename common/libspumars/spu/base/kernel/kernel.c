#include <stddef.h>
#include <stdint.h>
#include <spu_mfcio.h>

#include <sys/spu_thread.h>
#include <sys/spu_event.h>

#include <dma/spu_dma.h>

#include <mars/error.h>

#include "callback_internal_types.h"
#include "kernel_internal_types.h"
#include "workload_internal_types.h"

#define MARS_KERNEL_STATUS_EXIT								0x1
#define MARS_KERNEL_STATUS_IDLE								0x2
#define MARS_KERNEL_STATUS_BUSY								0x4
#define MARS_KERNEL_STATUS_RETRY							0x8

#define MARS_KERNEL_UPDATE_HEADER_BITS_ACCESS				0x0
#define MARS_KERNEL_UPDATE_HEADER_BITS_STATE				0x1
#define MARS_KERNEL_UPDATE_HEADER_BITS_COUNTER				0x2
#define MARS_KERNEL_UPDATE_HEADER_BITS_COUNTER_RESET		0x4

/* kernel */
union mars_kernel_buffer kernel_buffer;
static struct mars_kernel_params kernel_params;
static uint64_t kernel_params_ea;

/* workload queue */
static struct mars_workload_queue_header queue_header;
static struct mars_workload_queue_block queue_block;

/* workload */
static struct mars_workload_context workload_buf;
static struct mars_workload_context workload;
static uint16_t workload_id;
static uint64_t workload_ea;
static uint8_t workload_state;
static uint8_t workload_is_cached;

/* workload module */
static struct mars_workload_module cached_workload_module;
static struct mars_workload_module *workload_module;
static uint8_t workload_module_is_cached;

/* workload module entry */
typedef void (*module_entry)(const struct mars_kernel_syscalls *kernel_syscalls);

/* defined in switch.S */
extern void workload_run(void);
extern void workload_exit(uint8_t state);

/* called by switch.S */
void __workload_run(void);
void __workload_exit(uint8_t state);

static void kernel_memcpy(void *dst, const void *src, int size)
{
	__vector const int *vptr_src = (__vector const int *)src;
	__vector int *vptr_dst = (__vector int *)dst;
	__vector int *vptr_end = (__vector int *)(dst + size);

	while (__builtin_expect(vptr_dst < vptr_end, 1))
		*vptr_dst++ = *vptr_src++;
}

static int kernel_memcmp(const void *s1, const void *s2, int size)
{
	unsigned int offset1, offset2;
	__vector int n_v;
	__vector unsigned char shuffle1, shuffle2;
	__vector const unsigned char *vptr_1 = (__vector const unsigned char*)s1;
	__vector const unsigned char *vptr_2 = (__vector const unsigned char*)s2;
	__vector unsigned int neq_v, tmp1_v, shift_n_v, mask_v, gt_v, lt_v;
	__vector unsigned char data1A, data1B, data1, data2A, data2B, data2;

	n_v = spu_promote(size, 0);
	data1 = data2 = spu_splats((unsigned char)0);
	
	offset1 = (unsigned int)(vptr_1)&15;
	offset2 = (unsigned int)(vptr_2)&15;
	
	shuffle1 = (__vector unsigned char)spu_add((__vector unsigned int)spu_splats((unsigned char)offset1), 
					   						   ((__vector unsigned int) {0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F}));
	shuffle2 = (__vector unsigned char)spu_add((__vector unsigned int)spu_splats((unsigned char)offset2), 
					   						   ((__vector unsigned int) {0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F}));
	
	data1A = *vptr_1++;
	data2A = *vptr_2++;			   					   
	do {
		data1B = *vptr_1++;
		data2B = *vptr_2++;
		
		data1 = spu_shuffle(data1A, data1B, shuffle1);
		data2 = spu_shuffle(data2A, data2B, shuffle2);
		
		data1A = data1B;
		data2A = data2B;
		
		neq_v = spu_gather(spu_xor(spu_cmpeq(data1, data2), -1));
		n_v = spu_add(n_v, -16);
		tmp1_v = spu_cmpeq(neq_v, 0);
		tmp1_v = spu_and(tmp1_v, spu_cmpgt(n_v, 0));
	} while(spu_extract(tmp1_v, 0));
	
	mask_v = spu_splats((unsigned int)0xFFFF);
	shift_n_v = spu_andc((__vector unsigned int)spu_sub(0, n_v), spu_cmpgt(n_v, -1));
	mask_v = spu_and(spu_sl(mask_v, spu_extract(shift_n_v, 0)), mask_v);
	
	gt_v = spu_gather(spu_cmpgt(data1, data2));
	lt_v = spu_gather(spu_cmpgt(data2, data1));
	gt_v = spu_sub(-1, spu_sl(spu_cmpgt(gt_v, lt_v), 1));
	
	mask_v = spu_cmpeq(spu_and(neq_v, mask_v), 0);
	return (spu_extract(spu_andc(gt_v, mask_v), 0));
}

static uint32_t get_ticks()
{
	return kernel_params.kernel_ticks.offset - spu_read_decrementer();
}

static uint64_t get_mars_context_ea(void)
{
	return kernel_params.mars_context_ea;
}

static uint16_t get_kernel_id(void)
{
	return kernel_params.kernel_id;
}

static uint16_t get_workload_id(void)
{
	return workload_id;
}

static struct mars_workload_context *get_workload(void)
{
	return &workload;
}


static uint64_t get_workload_ea(uint16_t id)
{
	int context_index = id - (id / MARS_WORKLOAD_PER_BLOCK) - 1;

	return queue_header.context_ea +
			context_index * MARS_WORKLOAD_CONTEXT_SIZE;
				
}

static struct mars_workload_context *get_workload_by_id(uint16_t id)
{
	static struct mars_workload_context ret_workload;

	/* id is caller workload's id so return current workload */
	if (id == workload_id)
		return &workload;

	/* get the workload context from workload queue */
	dma_get(&ret_workload, get_workload_ea(id), MARS_WORKLOAD_CONTEXT_SIZE, MARS_KERNEL_DMA_TAG);
	dma_wait(MARS_KERNEL_DMA_TAG);

	return &ret_workload;
}

static uint64_t get_block_ea(int block)
{
	return queue_header.queue_ea +
	       offsetof(struct mars_workload_queue, block) +
	       MARS_WORKLOAD_QUEUE_BLOCK_SIZE * block;
}

static uint64_t get_block_bits(uint16_t id)
{
	int block;
	int index;
	uint64_t block_ea;
	uint64_t block_bits;

	/* check function params */
	if (id > MARS_WORKLOAD_ID_MAX || !(id % MARS_WORKLOAD_PER_BLOCK))
		return 0;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;
	
	/* calculate block ea */
	block_ea = get_block_ea(block);

	/* lock the queue block */
	mutex_lock_get(block_ea, (struct mars_mutex *)&queue_block);

	block_bits = queue_block.bits[index];

	/* unlock the queue block */
	mutex_unlock_put(block_ea, (struct mars_mutex *)&queue_block);

	return block_bits;
}

static void update_header_bits(int mode, int block)
{
	int index;
	uint16_t *block_bits = &queue_header.bits[block];
	uint8_t block_ready = MARS_WORKLOAD_BLOCK_READY_OFF;
	uint8_t block_waiting = MARS_WORKLOAD_BLOCK_WAITING_OFF;
	uint8_t block_priority = MARS_WORKLOAD_BLOCK_PRIORITY_MIN;
	uint8_t block_counter = MARS_WORKLOAD_BLOCK_COUNTER_MIN;

	if (mode & MARS_KERNEL_UPDATE_HEADER_BITS_STATE) {
		/* search through currently locked queue block workload bits */
		for (index = 1; index < MARS_WORKLOAD_PER_BLOCK; index++) {
			uint64_t *bits = &queue_block.bits[index];
			uint8_t state = MARS_BITS_GET(bits, WORKLOAD_STATE);

			/* workload state is ready so check priority */
			if (state & MARS_WORKLOAD_STATE_READY) {
				uint8_t priority =
					MARS_BITS_GET(bits, WORKLOAD_PRIORITY);

				/* set block priority if higher then current */
				if (priority > block_priority)
					block_priority = priority;

				/* set block ready bit in header bits */
				block_ready = MARS_WORKLOAD_BLOCK_READY_ON;
			} else if (state & MARS_WORKLOAD_STATE_WAITING) {
				/* set block waiting bit in header bits */
				block_waiting = MARS_WORKLOAD_BLOCK_WAITING_ON;
			}
		}
	}

	/* lock the queue header */
	mutex_lock_get(kernel_params.workload_queue_ea,
		       (struct mars_mutex *)&queue_header);

	/* update header bits state */
	if (mode & MARS_KERNEL_UPDATE_HEADER_BITS_STATE) {
		/* set the info bits inside queue header for this queue block */
		MARS_BITS_SET(block_bits, BLOCK_READY, block_ready);
		MARS_BITS_SET(block_bits, BLOCK_WAITING, block_waiting);
		MARS_BITS_SET(block_bits, BLOCK_PRIORITY, block_priority);
	/* update header bits counter */
	} else if (mode) {
		/* reset is not specified so increment current block counter */
		if (mode & MARS_KERNEL_UPDATE_HEADER_BITS_COUNTER) {
			block_counter =
				MARS_BITS_GET(block_bits, BLOCK_COUNTER);
			if (block_counter < MARS_WORKLOAD_BLOCK_COUNTER_MAX)
				block_counter++;
		}

		/* set the block counter bits */
		MARS_BITS_SET(block_bits, BLOCK_COUNTER, block_counter);
	}

	/* increment the header access value */
	queue_header.access++;

	/* unlock the queue header */
	mutex_unlock_put(kernel_params.workload_queue_ea,
			 (struct mars_mutex *)&queue_header);
}

static int change_bits(uint16_t id,
		       int (*check_bits)(uint64_t bits, uint64_t param),
		       uint64_t check_bits_param,
		       uint64_t (*set_bits)(uint64_t bits, uint64_t param),
		       uint64_t set_bits_param,
		       void (*callback)(uint16_t id))
{
	int block;
	int index;
	uint64_t block_ea;
	uint64_t bits;

	/* check function params */
	if (id > MARS_WORKLOAD_ID_MAX || !(id % MARS_WORKLOAD_PER_BLOCK))
		return MARS_ERROR_PARAMS;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	/* calculate block ea */
	block_ea = get_block_ea(block);

	/* lock the queue block */
	mutex_lock_get(block_ea, (struct mars_mutex *)&queue_block);

	/* check for valid state */
	if (check_bits &&
	    !(*check_bits)(queue_block.bits[index], check_bits_param)) {
		mutex_unlock_put(block_ea, (struct mars_mutex *)&queue_block);
		return MARS_ERROR_STATE;
	}

	/* reset workload queue bits and set state to new state */
	bits = (*set_bits)(queue_block.bits[index], set_bits_param);

	/* store new bits into queue block */
	queue_block.bits[index] = bits;

	/* if callback requested call it */
	if (callback)
		(*callback)(id);

	/* unlock the queue block */
	mutex_unlock_put(block_ea, (struct mars_mutex *)&queue_block);

	return MARS_SUCCESS;
}

static int check_state_bits(uint64_t bits, uint64_t state)
{
	return (MARS_BITS_GET(&bits, WORKLOAD_STATE) == state);
}

static int check_state_bits_not(uint64_t bits, uint64_t state)
{
	return !check_state_bits(bits, state);
}

static uint64_t set_state_bits(uint64_t bits, uint64_t state)
{
	MARS_BITS_SET(&bits, WORKLOAD_STATE, state);

	return bits;
}

static int change_state(uint16_t id,
			unsigned int old_state,
			unsigned int new_state,
			void (*callback)(uint16_t id))
{
	return change_bits(id,
			   check_state_bits, old_state,
			   set_state_bits, new_state,
		           callback);
}

static int workload_query(uint16_t id, int query)
{
	uint64_t bits = get_block_bits(id);

	switch (query) {
	case MARS_WORKLOAD_QUERY_IS_MODULE_CACHED:
		return workload_module_is_cached;
	case MARS_WORKLOAD_QUERY_IS_CONTEXT_CACHED:
		return (id == workload_id && workload_is_cached);
	case MARS_WORKLOAD_QUERY_IS_INITIALIZED:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE));
	case MARS_WORKLOAD_QUERY_IS_READY:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) &
			MARS_WORKLOAD_STATE_READY);
	case MARS_WORKLOAD_QUERY_IS_WAITING:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) &
			MARS_WORKLOAD_STATE_WAITING);
	case MARS_WORKLOAD_QUERY_IS_RUNNING:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) &
			MARS_WORKLOAD_STATE_RUNNING);
	case MARS_WORKLOAD_QUERY_IS_FINISHED:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) &
			MARS_WORKLOAD_STATE_FINISHED);
	case MARS_WORKLOAD_QUERY_IS_SIGNAL_SET:
		return (MARS_BITS_GET(&bits, WORKLOAD_SIGNAL) &
			MARS_WORKLOAD_SIGNAL_ON);
	}

	return 0;
}

static uint64_t set_wait_id_bits(uint64_t bits, uint64_t id)
{
	MARS_BITS_SET(&bits, WORKLOAD_WAIT_ID, id);

	return bits;
}

static int workload_wait_set(uint16_t id)
{
	return change_bits(workload_id,
			   check_state_bits_not, MARS_WORKLOAD_STATE_NONE,
			   set_wait_id_bits, id,
			   NULL);
}

static int workload_wait_reset(void)
{
	return change_bits(workload_id,
			   NULL, 0,
			   set_wait_id_bits, MARS_WORKLOAD_ID_NONE,
			   NULL);
}

static uint64_t set_signal_bits(uint64_t bits, uint64_t signal)
{
	MARS_BITS_SET(&bits, WORKLOAD_SIGNAL, signal);

	/* update queue header bits access in case kernel is idle */
	update_header_bits(MARS_KERNEL_UPDATE_HEADER_BITS_ACCESS, 0);

	return bits;
}

static int workload_signal_set(uint16_t id)
{
	return change_bits(id,
			   check_state_bits_not, MARS_WORKLOAD_STATE_NONE,
			   set_signal_bits, MARS_WORKLOAD_SIGNAL_ON,
			   NULL);
}

static int workload_signal_reset(void)
{
	return change_bits(workload_id,
			   NULL, 0,
			   set_signal_bits, MARS_WORKLOAD_SIGNAL_OFF,
			   NULL);
}

static void begin_callback(uint16_t id)
{
	/* get the workload context from workload queue */
	dma_get(&workload_buf, get_workload_ea(id),
		MARS_WORKLOAD_CONTEXT_SIZE, MARS_KERNEL_DMA_TAG);
	dma_wait(MARS_KERNEL_DMA_TAG);

	/* update queue header bits */
	update_header_bits(MARS_KERNEL_UPDATE_HEADER_BITS_STATE,
			   id / MARS_WORKLOAD_PER_BLOCK);
}

static void end_callback(uint16_t id)
{
	/* put the workload context into workload queue */
	dma_put((void *)&workload_buf, get_workload_ea(id),
		MARS_WORKLOAD_CONTEXT_SIZE, MARS_KERNEL_DMA_TAG);
	dma_wait(MARS_KERNEL_DMA_TAG);

	/* update queue header bits */
	update_header_bits(MARS_KERNEL_UPDATE_HEADER_BITS_STATE,
			   id / MARS_WORKLOAD_PER_BLOCK);
}

static uint64_t set_schedule_bits(uint64_t bits, uint64_t priority)
{
	/* set the info bits inside queue block for this workload */
	MARS_BITS_SET(&bits, WORKLOAD_STATE, MARS_WORKLOAD_STATE_SCHEDULING);
	MARS_BITS_SET(&bits, WORKLOAD_PRIORITY, priority);
	MARS_BITS_SET(&bits, WORKLOAD_COUNTER, MARS_WORKLOAD_COUNTER_MAX);
	MARS_BITS_SET(&bits, WORKLOAD_SIGNAL, MARS_WORKLOAD_SIGNAL_OFF);
	MARS_BITS_SET(&bits, WORKLOAD_WAIT_ID, MARS_WORKLOAD_ID_NONE);

	return bits;
}

static int workload_schedule_begin(uint16_t id, uint8_t priority,
				   struct mars_workload_context **workload)
{
	int ret;

	/* change bits necessary to begin scheduling */
	ret = change_bits(id,
			  check_state_bits, MARS_WORKLOAD_STATE_FINISHED,
			  set_schedule_bits, priority,
			  begin_callback);
	if (ret != MARS_SUCCESS)
		return ret;

	/* if requested set workload context pointer to return */
	if (workload)
		*workload = &workload_buf;

	return MARS_SUCCESS;
}

static int workload_schedule_end(uint16_t id, int cancel)
{
	return change_state(id,
			    MARS_WORKLOAD_STATE_SCHEDULING,
			    cancel ? MARS_WORKLOAD_STATE_FINISHED :
				     MARS_WORKLOAD_STATE_READY,
			    end_callback);
}

static int unscheduling_state_bits(uint64_t bits, uint64_t param)
{
	(void)param;

	uint8_t state = MARS_BITS_GET(&bits, WORKLOAD_STATE);

	return (state & (MARS_WORKLOAD_STATE_READY |
			 MARS_WORKLOAD_STATE_RUNNING |
			 MARS_WORKLOAD_STATE_WAITING));
}

static int workload_unschedule_begin(uint16_t id,
				struct mars_workload_context **workload)
{
	int ret;

	/* change bits necessary to begin unscheduling */
	ret = change_bits(id,
			  unscheduling_state_bits, 0,
			  set_state_bits, MARS_WORKLOAD_STATE_UNSCHEDULING,
			  begin_callback);
	if (ret != MARS_SUCCESS)
		return ret;

	/* if requested set workload context pointer to return */
	if (workload)
		*workload = &workload_buf;

	return MARS_SUCCESS;
}

static void notify_host_bits(uint64_t block_ea, int index);

static int workload_unschedule_end(uint16_t id)
{
	int ret;
	int block;
	int index;
	uint64_t block_ea;

	ret = change_state(id,
			   MARS_WORKLOAD_STATE_UNSCHEDULING,
			   MARS_WORKLOAD_STATE_FINISHED,
			   end_callback);
	if (ret != MARS_SUCCESS)
		return ret;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	/* calculate block ea */
	block_ea = get_block_ea(block);

	/* notify host */
	notify_host_bits(block_ea, index);

	return MARS_SUCCESS;
}

static void host_signal_send(uint64_t watch_point_ea)
{
	spu_thread_send_event(MARS_KERNEL_SPU_EVENT_PORT, (uint32_t)(watch_point_ea >> 32), (watch_point_ea & 0xffffffff));
}

static int host_callback_queue_push(void)
{
	uint64_t queue_ea = kernel_params.callback_queue_ea;
	struct mars_callback_queue *queue = &kernel_buffer.callback_queue;

	/* lock the queue */
	mutex_lock_get(queue_ea, (struct mars_mutex *)queue);

	/* check if queue is full */
	if (queue->count == MARS_CALLBACK_QUEUE_MAX) {
		mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);
		return MARS_ERROR_LIMIT;
	}

	/* set the push flag to signal host of new item */
	queue->flag = MARS_CALLBACK_QUEUE_FLAG_PUSH;

	/* put workload id at tail of queue */
	queue->workload_id[queue->tail] = workload_id;

	/* increment count */
	queue->count++;

	/* increment tail */
	queue->tail++;

	/* wrap tail to front of queue if necessary */
	if (queue->tail == MARS_CALLBACK_QUEUE_MAX)
		queue->tail = 0;

	/* unlock the queue */
	mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

	return MARS_SUCCESS;
}

static int host_callback_set(uint64_t callback_ea,
			     const struct mars_callback_args *in)
{
	int ret;
	struct mars_workload_callback *callback =
		(struct mars_workload_callback *)
			((void *)&workload + MARS_WORKLOAD_MODULE_SIZE);

	/* set the callback function pointer into workload callback area */
	callback->callback_ea = callback_ea;

	/* input args specified so copy into workload data area */
	if (in)
		kernel_memcpy(&callback->callback_args, in,
			      MARS_CALLBACK_ARGS_SIZE);

	/* push workload id to callback queue */
	ret = host_callback_queue_push();
	if (ret != MARS_SUCCESS)
		return ret;

	/* notify host to process callback */
	host_signal_send(kernel_params.callback_queue_ea +
			 offsetof(struct mars_callback_queue, flag));

	return MARS_SUCCESS;
}

static int host_callback_reset(struct mars_callback_args *out)
{
	struct mars_workload_callback *callback =
		(struct mars_workload_callback *)
			((void *)&workload + MARS_WORKLOAD_MODULE_SIZE);

	/* output requested so dma from workload data area */
	if (out)
		kernel_memcpy(out, &callback->callback_args,
			      MARS_CALLBACK_ARGS_SIZE);

	return callback->callback_ret;
}

static struct mars_kernel_syscalls kernel_syscalls =
{
	get_ticks,
	get_mars_context_ea,
	get_kernel_id,
	get_workload_id,
	get_workload,
	get_workload_by_id,
	workload_exit,
	workload_query,
	workload_wait_set,
	workload_wait_reset,
	workload_signal_set,
	workload_signal_reset,
	workload_schedule_begin,
	workload_schedule_end,
	workload_unschedule_begin,
	workload_unschedule_end,
	host_signal_send,
	host_callback_set,
	host_callback_reset,
	mutex_lock_get,
	mutex_unlock_put,
	dma_get,
	dma_put,
	dma_wait
};

void __workload_run()
{
	/* call module entry function */
	((module_entry)workload_module->entry)(&kernel_syscalls);
}

void __workload_exit(uint8_t state)
{
	workload_state = state;
}

static int search_block(int block, int ready)
{
	int i;
	int index = -1;
	uint8_t max_priority = 0;
	uint16_t max_counter = 0;
	uint64_t block_ea = get_block_ea(block);

	/* lock the queue block */
	mutex_lock_get(block_ea, (struct mars_mutex *)&queue_block);

	/* search through all workloads in block */
	for (i = 1; i < MARS_WORKLOAD_PER_BLOCK; i++) {
		uint64_t *bits   = &queue_block.bits[i];
		uint8_t state    = MARS_BITS_GET(bits, WORKLOAD_STATE);
		uint8_t priority = MARS_BITS_GET(bits, WORKLOAD_PRIORITY);
		uint8_t signal   = MARS_BITS_GET(bits, WORKLOAD_SIGNAL);
		uint16_t wait_id = MARS_BITS_GET(bits, WORKLOAD_WAIT_ID);
		uint16_t counter = MARS_BITS_GET(bits, WORKLOAD_COUNTER);

		/* found workload in ready state */
		if (ready && (state & MARS_WORKLOAD_STATE_READY)) {
			/* compare priority and counter with previous ones */
			if (index < 0 || priority > max_priority ||
			  (priority == max_priority && counter > max_counter)) {
				index = i;
				max_counter = counter;
				max_priority = priority;
			}

			/* increment wait counter without overflowing */
			if (counter < MARS_WORKLOAD_COUNTER_MAX)
				MARS_BITS_SET(bits, WORKLOAD_COUNTER,
					      counter + 1);
		/* found workload in waiting state */
		} else if (!ready && (state & MARS_WORKLOAD_STATE_WAITING)) {
			/* waiting for workload to finish so check status */
			if (wait_id != MARS_WORKLOAD_ID_NONE) {
				struct mars_workload_queue_block *wait_block;
				uint8_t wait_state;

				int bl = wait_id / MARS_WORKLOAD_PER_BLOCK;
				int id = wait_id % MARS_WORKLOAD_PER_BLOCK;

				/* check if workload id is in the same block */
				if (block != bl) {
					/* set pointer to check buffer block */
					wait_block =
					    &kernel_buffer.workload_queue_block;

					/* fetch the necessary block */
					dma_get(wait_block, get_block_ea(bl),
						MARS_WORKLOAD_QUEUE_BLOCK_SIZE,
						MARS_KERNEL_DMA_TAG);
					dma_wait(MARS_KERNEL_DMA_TAG);
				} else {
					/* set pointer to check current block */
					wait_block = &queue_block;
				}

				/* get state of workload its waiting for */
				wait_state =
					MARS_BITS_GET(&wait_block->bits[id],
						      WORKLOAD_STATE);

				/* check if workload is finished and reset */
				if (wait_state & MARS_WORKLOAD_STATE_FINISHED) {
					MARS_BITS_SET(bits, WORKLOAD_WAIT_ID,
						MARS_WORKLOAD_ID_NONE);
					MARS_BITS_SET(bits, WORKLOAD_STATE,
						MARS_WORKLOAD_STATE_READY);

					index = i;
				}
			/* waiting for signal so check signal bit and reset */
			} else if (signal & MARS_WORKLOAD_SIGNAL_ON) {
				MARS_BITS_SET(bits, WORKLOAD_SIGNAL,
					      MARS_WORKLOAD_SIGNAL_OFF);
				MARS_BITS_SET(bits, WORKLOAD_STATE,
					      MARS_WORKLOAD_STATE_READY);

				index = i;
			}
		}
	}

	/* index is set so reserve the runnable workload */
	if (index >= 0) {
		if (ready) {
			/* update the current state of the workload */
			MARS_BITS_SET(&queue_block.bits[index],
				      WORKLOAD_STATE,
				      MARS_WORKLOAD_STATE_RUNNING);

			/* reset the counter for reserved workload */
			MARS_BITS_SET(&queue_block.bits[index],
				      WORKLOAD_COUNTER,
				      MARS_WORKLOAD_COUNTER_MIN);

			/* check if kernel ran this workload most recently */
			workload_is_cached =
				(workload_id ==
				 MARS_WORKLOAD_PER_BLOCK * block + index) &&
				(kernel_params.kernel_id ==
				 MARS_BITS_GET(&queue_block.bits[index],
					       WORKLOAD_KERNEL_ID));

			/* set the kernel id of this kernel into block bits */
			MARS_BITS_SET(&queue_block.bits[index],
				      WORKLOAD_KERNEL_ID,
				      kernel_params.kernel_id);

			/* reset block counter */
			update_header_bits(
				MARS_KERNEL_UPDATE_HEADER_BITS_COUNTER_RESET,
				block);
		}

		/* update queue header bits */
		update_header_bits(MARS_KERNEL_UPDATE_HEADER_BITS_STATE, block);
	}

	/* unlock the queue block */
	mutex_unlock_put(block_ea, (struct mars_mutex *)&queue_block);

	/* returns -1 if no runnable workload found */
	return index;
}

static void notify_host_bits(uint64_t block_ea, int index)
{
	uint64_t bits_ea =
		block_ea +
		offsetof(struct mars_workload_queue_block, bits) +
		sizeof(uint64_t) * index;

	host_signal_send(bits_ea);
}

static int workload_reserve(void)
{	
	int i;
	int block = -1;
	int index;
	int retry = 0;
	uint8_t max_block_priority = 0;
	uint16_t max_block_counter = 0;

	/* get the workload queue header */
	dma_get(&queue_header, kernel_params.workload_queue_ea,
		MARS_WORKLOAD_QUEUE_HEADER_SIZE, MARS_KERNEL_DMA_TAG);
	dma_wait(MARS_KERNEL_DMA_TAG);

	/* return exit status if exit flag is set from host */
	if (queue_header.flag & MARS_WORKLOAD_QUEUE_FLAG_EXIT)
		return MARS_KERNEL_STATUS_EXIT;

	/* search workload queue header for highest priority ready block that
	 * has waited the longest in ready state */
	for (i = 0; i < MARS_WORKLOAD_NUM_BLOCKS; i++) {
		uint16_t *bits = &queue_header.bits[i];
		uint8_t block_ready    = MARS_BITS_GET(bits, BLOCK_READY);
		uint8_t block_waiting  = MARS_BITS_GET(bits, BLOCK_WAITING);
		uint8_t block_priority = MARS_BITS_GET(bits, BLOCK_PRIORITY);
		uint16_t block_counter = MARS_BITS_GET(bits, BLOCK_COUNTER);

		/* block is ready so check scheduling conditions */
		if (block_ready) {
			if (block < 0 || block_priority > max_block_priority ||
			    (block_priority == max_block_priority &&
			     block_counter > max_block_counter)) {
				block = i;
				max_block_priority = block_priority;
				max_block_counter = block_counter;
			}

			/* increment block counter */
			update_header_bits(
				MARS_KERNEL_UPDATE_HEADER_BITS_COUNTER, i);
		}

		/* block is waiting so check block */
		if (block_waiting)
			retry |= (search_block(i, 0) >= 0);
	}

	/* no runnable workload found */
	if (block < 0)
		return retry ?
			MARS_KERNEL_STATUS_RETRY : MARS_KERNEL_STATUS_IDLE;

	/* search block for workload index to run */
	index = search_block(block, 1);
	if (index < 0)
		return MARS_KERNEL_STATUS_RETRY;

	/* set global workload info based on workload_id */
	workload_id = MARS_WORKLOAD_PER_BLOCK * block + index;
	workload_ea = get_workload_ea(workload_id);
	workload_module = (struct mars_workload_module *)&workload;

	/* get the workload context code from workload queue */
	dma_get(&workload, workload_ea, 
		MARS_WORKLOAD_CONTEXT_SIZE, MARS_KERNEL_DMA_TAG);
	dma_wait(MARS_KERNEL_DMA_TAG);
	
	return MARS_KERNEL_STATUS_BUSY;
}

static void workload_release(void)
{
	int unscheduled;
	int block = workload_id / MARS_WORKLOAD_PER_BLOCK;
	int index = workload_id % MARS_WORKLOAD_PER_BLOCK;
	uint64_t block_ea = get_block_ea(block);
	uint8_t state;

	/* lock the queue block */
	mutex_lock_get(block_ea, (struct mars_mutex *)&queue_block);

	/* get current state */
	state = MARS_BITS_GET(&queue_block.bits[index], WORKLOAD_STATE);

	/* if state is unscheduling or finished, it (was/will be) aborted */
	unscheduled = state & (MARS_WORKLOAD_STATE_UNSCHEDULING |
			       MARS_WORKLOAD_STATE_FINISHED);

	if (!unscheduled) {
		/* put the workload context into workload queue */
		dma_put(&workload, workload_ea,
			MARS_WORKLOAD_CONTEXT_SIZE, MARS_KERNEL_DMA_TAG);
		dma_wait(MARS_KERNEL_DMA_TAG);

		/* update current workload state in workload queue block */
		MARS_BITS_SET(&queue_block.bits[index], WORKLOAD_STATE,
			      workload_state);

		/* update queue header bits */
		update_header_bits(MARS_KERNEL_UPDATE_HEADER_BITS_STATE, block);
	}

	/* unlock the queue block */
	mutex_unlock_put(block_ea, (struct mars_mutex *)&queue_block);

	/* workload state is finished so notify host */
	if (!unscheduled && (workload_state & MARS_WORKLOAD_STATE_FINISHED))
		notify_host_bits(block_ea, index);
}

static void workload_module_load(void)
{
	__vector unsigned char *bss_ptr, *bss_end;

	workload_module_is_cached =
		!(kernel_memcmp(&cached_workload_module, workload_module,
				MARS_WORKLOAD_MODULE_SIZE));

	/* only reload the readonly text segment if different from cached */
	if (!workload_module_is_cached) {
		/* store the current cached workload module ea */
		kernel_memcpy(&cached_workload_module, workload_module,
			      MARS_WORKLOAD_MODULE_SIZE);

		/* load the text into mpu storage from host storage */
		dma_get((void *)workload_module->text_vaddr,
			workload_module->text_ea,
			workload_module->text_size,
			MARS_KERNEL_DMA_TAG);
	}

	/* load the read-write data segment */
	dma_get((void *)workload_module->data_vaddr,
		workload_module->data_ea,
		workload_module->data_size,
		MARS_KERNEL_DMA_TAG);

	/* 0 the bss segment */
	bss_ptr = (__vector unsigned char *)(workload_module->data_vaddr +
					     workload_module->data_size);
	bss_end = (__vector unsigned char *)((void *)bss_ptr +
					     workload_module->bss_size);

	while (__builtin_expect(bss_ptr < bss_end, 1))
		*bss_ptr++ = spu_splats((unsigned char)0);

	dma_wait(MARS_KERNEL_DMA_TAG);

	/* sync before executing loaded code */
	spu_sync();
}

static void idle_wait(void)
{
	struct mars_workload_queue_header *cur_queue_header =
		&kernel_buffer.workload_queue_header;

	/* set event mask for the lost event */
	spu_write_event_mask(MFC_LLR_LOST_EVENT);

	/* get current atomic state of queue header */
	mfc_getllar(cur_queue_header, kernel_params.workload_queue_ea, 0, 0);
	mfc_read_atomic_status();

	/* check if queue header has been modified since we last fetched it */
	if (!kernel_memcmp(&queue_header, cur_queue_header,
			   MARS_WORKLOAD_QUEUE_HEADER_SIZE)) {
		/* wait until queue header is modified */
		spu_read_event_status();
		spu_write_event_ack(MFC_LLR_LOST_EVENT);
	}

	/* clear any remnant lost event */
	spu_write_event_ack(MFC_LLR_LOST_EVENT);
}

static void get_params(void)
{
	/* set event mask for the lost event */
	spu_write_event_mask(MFC_LLR_LOST_EVENT);

	/* set sync begin flag so host knows to begin sync process */
	do {
		mfc_getllar(&kernel_params, kernel_params_ea, 0, 0);
		mfc_read_atomic_status();

		/* set the flag */
		kernel_params.kernel_ticks.flag =
			MARS_KERNEL_TICKS_FLAG_SYNC_BEGIN;
		spu_dsync();

		mfc_putllc(&kernel_params, kernel_params_ea, 0, 0);
	} while (mfc_read_atomic_status() & MFC_PUTLLC_STATUS);

	/* get the tick offset from host and check if sync end flag is set */
	do {
		spu_write_event_ack(MFC_LLR_LOST_EVENT);

		mfc_getllar(&kernel_params, kernel_params_ea, 0, 0);
		mfc_read_atomic_status();

		/* host set the sync end flag so finish */
		if (kernel_params.kernel_ticks.flag &
		    MARS_KERNEL_TICKS_FLAG_SYNC_END)
			break;

		spu_read_event_status();
	} while (1);

	/* now, kernel parameters, including offset of tick counters,
	 * are stored in 'kernel_params'.
	 */

	/* start decrementer */
	spu_write_decrementer(0);

	/* clear any remnant lost event */
	spu_write_event_ack(MFC_LLR_LOST_EVENT);
}

int main(uint64_t params_ea)
{
	kernel_params_ea = params_ea;

	get_params();

	do {
		int status = workload_reserve();

		if (status & MARS_KERNEL_STATUS_BUSY) {
			workload_module_load();
			workload_run();
			workload_release();
		} else if (status & MARS_KERNEL_STATUS_IDLE) {
			idle_wait();
		} else if (status & MARS_KERNEL_STATUS_EXIT) {
			break;
		}
	} while (1);

	host_signal_send(MARS_HOST_SIGNAL_EXIT);

	return MARS_SUCCESS;
}


void exit(int rc)
{
	spu_thread_exit(rc);
	for(;;)
		;
}
