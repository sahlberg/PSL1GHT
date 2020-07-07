#include <string.h>

#include <mars/base.h>
#include <mars/error.h>
#include <mars/mutex.h>
#include <mars/workload_queue.h>

#include "elf.h"
#include "context_internal.h"
#include "kernel_internal_types.h"
#include "workload_internal_types.h"

static inline uint64_t get_workload_ea(uint64_t queue_ea, uint16_t workload_id)
{
	uint64_t context_ea =
		mars_ea_get_uint64(queue_ea +
				   offsetof(struct mars_workload_queue_header,
				   context_ea));
	int context_index =
		workload_id - (workload_id / MARS_WORKLOAD_PER_BLOCK) - 1;

	return context_ea + context_index * MARS_WORKLOAD_CONTEXT_SIZE;
}

static inline uint64_t get_block_ea(uint64_t queue_ea, int block)
{
	return queue_ea +
	       offsetof(struct mars_workload_queue, block) +
	       MARS_WORKLOAD_QUEUE_BLOCK_SIZE * block;
}

static inline uint64_t get_block_bits_ea(uint64_t block_ea, int index)
{
	return block_ea +
	       offsetof(struct mars_workload_queue_block, bits) +
	       sizeof(uint64_t) * index;
}

static int change_bits(struct mars_context *mars,
		       uint16_t id,
		       uint64_t *workload_ea,
		       int (*check_bits)(uint64_t bits, uint64_t param),
		       uint64_t check_bits_param,
		       uint64_t (*set_bits)(uint64_t bits, uint64_t param),
		       uint64_t set_bits_param,
		       void (*callback)(struct mars_context *mars, uint16_t id))
{
	int block;
	int index;
	uint64_t queue_ea;
	uint64_t block_ea;
	uint64_t bits_ea;
	uint64_t bits;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->workload_queue_ea)
		return MARS_ERROR_PARAMS;
	if (id > MARS_WORKLOAD_ID_MAX || !(id % MARS_WORKLOAD_PER_BLOCK))
		return MARS_ERROR_PARAMS;

	queue_ea = mars->workload_queue_ea;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	/* prepare work area for queue block */
	block_ea = get_block_ea(queue_ea, block);

	mars_mutex_lock(block_ea);

	/* get bits from workload queue block */
	bits_ea = get_block_bits_ea(block_ea, index);
	bits = mars_ea_get_uint64(bits_ea);

	/* check for valid state */
	if (!(*check_bits)(bits, check_bits_param)) {
		mars_mutex_unlock(block_ea);
		return MARS_ERROR_STATE;
	}

	/* reset workload queue bits and set state to new state */
	bits = (*set_bits)(bits, set_bits_param);

	/* store new bits into queue block */
	mars_ea_put_uint64(bits_ea, bits);

	/* if callback requested call it */
	if (callback)
		(*callback)(mars, id);

	mars_mutex_unlock(block_ea);

	/* if requested set workload context pointer to return */
	if (workload_ea)
		*workload_ea = get_workload_ea(queue_ea, id);

	return MARS_SUCCESS;
}

static int check_state_bits(uint64_t bits, uint64_t state)
{
	return (MARS_BITS_GET(&bits, WORKLOAD_STATE) == state);
}

static int check_state_bits_not(uint64_t bits, uint64_t state)
{
	return (MARS_BITS_GET(&bits, WORKLOAD_STATE) != state);
}

static uint64_t set_state_bits(uint64_t bits, uint64_t state)
{
	MARS_BITS_SET(&bits, WORKLOAD_STATE, state);

	return bits;
}

static int change_state(
		struct mars_context *mars,
		uint16_t id,
		uint64_t *workload_ea,
		unsigned int old_state,
		unsigned int new_state,
		void (*callback)(struct mars_context *mars, uint16_t id))
{
	return change_bits(mars, id, workload_ea,
			   check_state_bits, old_state,
			   set_state_bits, new_state,
			   callback);
}

static void init_header(uint64_t queue_ea)
{
	int block;
	uint16_t bits = 0;
	struct mars_workload_queue *queue;

	/* prepare work area for queue header */
	queue = mars_ea_work_area_get(queue_ea,
				MARS_WORKLOAD_QUEUE_HEADER_ALIGN,
				MARS_WORKLOAD_QUEUE_HEADER_SIZE);

	/* initialize workload queue header */
	queue->header.flag = MARS_WORKLOAD_QUEUE_FLAG_NONE;
	queue->header.queue_ea = queue_ea;
	queue->header.context_ea =
		queue_ea + offsetof(struct mars_workload_queue, context);

	/* create initial bit pattern of workload queue header */
	MARS_BITS_SET(&bits, BLOCK_PRIORITY, MARS_WORKLOAD_BLOCK_PRIORITY_MIN);
	MARS_BITS_SET(&bits, BLOCK_COUNTER, MARS_WORKLOAD_BLOCK_COUNTER_MAX);
	MARS_BITS_SET(&bits, BLOCK_READY, MARS_WORKLOAD_BLOCK_READY_OFF);
	MARS_BITS_SET(&bits, BLOCK_WAITING, MARS_WORKLOAD_BLOCK_WAITING_OFF);

	for (block = 0; block < MARS_WORKLOAD_NUM_BLOCKS; block++)
		queue->header.bits[block] = bits;

	/* update queue header on EA */
	mars_ea_put(queue_ea, queue, MARS_WORKLOAD_QUEUE_HEADER_SIZE);

	/* reset mutex portion of queue header */
	mars_mutex_reset(queue_ea);
}

static void init_block(uint64_t block_ea, uint64_t initial_bits)
{
	int index;
	struct mars_workload_queue_block *block =
		mars_ea_work_area_get(block_ea,
				      MARS_WORKLOAD_QUEUE_BLOCK_ALIGN,
				      MARS_WORKLOAD_QUEUE_BLOCK_SIZE);

	for (index = 1; index < MARS_WORKLOAD_PER_BLOCK; index++)
		block->bits[index] = initial_bits;

	/* update queue block on EA */
	mars_ea_put(block_ea, block, MARS_WORKLOAD_QUEUE_BLOCK_SIZE);

	/* reset mutex portion of queue block */
	mars_mutex_reset(block_ea);
}

static void init_blocks(uint64_t queue_ea)
{
	int block;
	uint64_t bits = 0;

	/* create initial bit pattern of workload queue blocks */
	MARS_BITS_SET(&bits, WORKLOAD_STATE, MARS_WORKLOAD_STATE_NONE);

	/* other bits are set by mars_workload_queue_schedule_begin properly */

	/* initialize workload queue blocks */
	for (block = 0; block < MARS_WORKLOAD_NUM_BLOCKS; block++) {
		uint64_t block_ea = get_block_ea(queue_ea, block);

		init_block(block_ea, bits);
	}
}

int mars_workload_queue_create(struct mars_context *mars)
{
	uint64_t queue_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (mars->workload_queue_ea)
		return MARS_ERROR_STATE;

	/* allocate workload instance */
	queue_ea = mars_ea_memalign(MARS_WORKLOAD_QUEUE_ALIGN,
				    MARS_WORKLOAD_QUEUE_SIZE);
	if (!queue_ea)
		return MARS_ERROR_MEMORY;

	/* initialize workload queue header */
	init_header(queue_ea);

	/* initialize workload queue blocks */
	init_blocks(queue_ea);

	/* sync EA */
	mars_ea_sync();

	/* set the workload queue instance in the mars context */
	mars->workload_queue_ea = queue_ea;

	return MARS_SUCCESS;
}

static int is_block_empty(uint64_t block_ea)
{
	int index;
	struct mars_workload_queue_block *block =
		mars_ea_work_area_get(block_ea,
				      MARS_WORKLOAD_QUEUE_BLOCK_ALIGN,
				      MARS_WORKLOAD_QUEUE_BLOCK_SIZE);

	/* get the workload queue block from shared memory */
	mars_ea_get(block_ea, block, sizeof(struct mars_workload_queue_block));

	/* check status */
	for (index = 1; index < MARS_WORKLOAD_PER_BLOCK; index++) {
		if (MARS_BITS_GET(&block->bits[index], WORKLOAD_STATE) !=
		    MARS_WORKLOAD_STATE_NONE)
			return MARS_ERROR_STATE;
	}

	return MARS_SUCCESS;
}

int mars_workload_queue_destroy(struct mars_context *mars)
{
	int block;
	uint64_t queue_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->workload_queue_ea)
		return MARS_ERROR_PARAMS;

	queue_ea = mars->workload_queue_ea;

	/* check for any workloads left in workload queue */
	for (block = 0; block < MARS_WORKLOAD_NUM_BLOCKS; block++) {
		uint64_t block_ea = get_block_ea(queue_ea, block);
		int ret = is_block_empty(block_ea);
		if (ret != MARS_SUCCESS)
			return ret;
	}

	/* free workload queue instance */
	mars_ea_free(queue_ea);

	/* set the workload queue to NULL for error checking */
	mars->workload_queue_ea = 0;

	return MARS_SUCCESS;
}

int mars_workload_queue_exit(struct mars_context *mars)
{
	uint64_t queue_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->workload_queue_ea)
		return MARS_ERROR_PARAMS;

	queue_ea = mars->workload_queue_ea;

	mars_mutex_lock(queue_ea);

	mars_ea_put_uint32(queue_ea +
			   offsetof(struct mars_workload_queue_header, flag),
			   MARS_WORKLOAD_QUEUE_FLAG_EXIT);

	mars_mutex_unlock(queue_ea);

	return MARS_SUCCESS;
}

int mars_workload_queue_query(struct mars_context *mars,
			      uint16_t id,
			      int query)
{
	int block;
	int index;
	uint64_t queue_ea;
	uint64_t block_ea;
	uint64_t bits_ea;
	uint64_t bits;

	/* check function params */
	if (!mars)
		return 0;
	if (!mars->workload_queue_ea)
		return 0;
	if (id > MARS_WORKLOAD_ID_MAX || !(id % MARS_WORKLOAD_PER_BLOCK))
		return 0;

	queue_ea = mars->workload_queue_ea;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	/* prepare work area for queue block */
	block_ea = get_block_ea(queue_ea, block);

	mars_mutex_lock(block_ea);

	/* get bits from workload queue block */
	bits_ea = get_block_bits_ea(block_ea, index);
	bits = mars_ea_get_uint64(bits_ea);

	mars_mutex_unlock(block_ea);

	switch (query) {
	case MARS_WORKLOAD_QUERY_IS_INITIALIZED:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) !=
			MARS_WORKLOAD_STATE_NONE);
	case MARS_WORKLOAD_QUERY_IS_READY:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) ==
			MARS_WORKLOAD_STATE_READY);
	case MARS_WORKLOAD_QUERY_IS_WAITING:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) ==
			MARS_WORKLOAD_STATE_WAITING);
	case MARS_WORKLOAD_QUERY_IS_RUNNING:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) ==
			MARS_WORKLOAD_STATE_RUNNING);
	case MARS_WORKLOAD_QUERY_IS_FINISHED:
		return (MARS_BITS_GET(&bits, WORKLOAD_STATE) ==
			MARS_WORKLOAD_STATE_FINISHED);
	case MARS_WORKLOAD_QUERY_IS_SIGNAL_SET:
		return (MARS_BITS_GET(&bits, WORKLOAD_SIGNAL) ==
			MARS_WORKLOAD_SIGNAL_ON);
	}

	return 0;
}

static int alloc_block(uint64_t block_ea)
{
	int ret = -1;
	int index;
	struct mars_workload_queue_block *block =
		mars_ea_work_area_get(block_ea,
				      MARS_WORKLOAD_QUEUE_BLOCK_ALIGN,
				      MARS_WORKLOAD_QUEUE_BLOCK_SIZE);

	mars_mutex_lock(block_ea);

	/* get the workload queue block from shared memory */
	mars_ea_get(block_ea, block, MARS_WORKLOAD_QUEUE_BLOCK_SIZE);

	/* check status */
	for (index = 1; index < MARS_WORKLOAD_PER_BLOCK; index++) {
		uint64_t bits = block->bits[index];
		if (MARS_BITS_GET(&bits, WORKLOAD_STATE) ==
		    MARS_WORKLOAD_STATE_NONE) {
			MARS_BITS_SET(&bits, WORKLOAD_STATE,
				      MARS_WORKLOAD_STATE_ADDING);
			MARS_BITS_SET(&bits, WORKLOAD_KERNEL_ID,
				      MARS_KERNEL_ID_NONE);
			mars_ea_put_uint64(get_block_bits_ea(block_ea, index),
					   bits);
			ret = index;
			break;
		}
	}

	mars_mutex_unlock(block_ea);

	return ret;
}

static int add_workload_module(uint64_t workload_ea,
			       const void *workload_module_elf,
			       const char *workload_module_name)
{
	int ret, i;
	int text_found = 0;
	int data_found = 0;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	struct mars_workload_module *workload_module;

	/* get work area for workload module */
	workload_module = mars_ea_work_area_get(workload_ea,
						MARS_WORKLOAD_MODULE_ALIGN,
						MARS_WORKLOAD_MODULE_SIZE);

	memset(workload_module, 0, MARS_WORKLOAD_MODULE_SIZE);

	/* process elf header information */
	ehdr = (Elf32_Ehdr *)workload_module_elf;
	phdr = (Elf32_Phdr *)((void *)ehdr + ehdr->e_phoff);

	/* elf is not executable */
	if (ehdr->e_type != ET_EXEC)
		return MARS_ERROR_FORMAT;

	for (i = 0; i < ehdr->e_phnum; i++) {
		/* readonly text segment */
		if (phdr->p_type == PT_LOAD &&
		    phdr->p_flags == PF_R + PF_X &&
		    phdr->p_align == 0x80) {
			/* make sure base addr is what we expect */
			if (text_found ||
			    phdr->p_vaddr != MARS_WORKLOAD_MODULE_BASE_ADDR ||
			    phdr->p_memsz != phdr->p_filesz) {
				ret = MARS_ERROR_FORMAT;
				goto error;
			}

			/* initialize the workload module text info */
			workload_module->text_ea = mars_ea_map((void *)ehdr +
							       phdr->p_offset,
							       phdr->p_filesz);
			if (!workload_module->text_ea) {
				ret = MARS_ERROR_MEMORY;
				goto error;
			}

			workload_module->text_vaddr = phdr->p_vaddr;
			workload_module->text_size = phdr->p_filesz;

			/* make sure we only find 1 text segment */
			text_found = 1;
		/* read-write data segment */
		} else if (phdr->p_type == PT_LOAD &&
			   phdr->p_flags == PF_R + PF_W &&
			   phdr->p_align == 0x80) {
			if (data_found) {
				ret = MARS_ERROR_FORMAT;
				goto error;
			}

			workload_module->data_ea = mars_ea_map((void *)ehdr +
							       phdr->p_offset,
							       phdr->p_filesz);
			if (!workload_module->data_ea) {
				ret = MARS_ERROR_MEMORY;
				goto error;
			}

			workload_module->data_vaddr = phdr->p_vaddr;
			workload_module->data_size = phdr->p_filesz;
			workload_module->bss_size = phdr->p_memsz -
						    phdr->p_filesz;

			/* make sure we only find 1 data segment */
			data_found = 1;
		}

		/* increment program header */
		phdr = (void *)phdr + ehdr->e_phentsize;
	}

	/* make sure text and data segment is found */
	if (!text_found || !data_found) {
		ret = MARS_ERROR_FORMAT;
		goto error;
	}

	/* set the entry point of execution */
	workload_module->entry = ehdr->e_entry;
	if (workload_module_name)
		strcpy((char *)workload_module->name, workload_module_name);
	else
		memset((char *)workload_module->name, 0,
		       MARS_WORKLOAD_MODULE_NAME_LEN_MAX + 1);

	/* update workload module on EA */
	mars_ea_put(workload_ea, workload_module, MARS_WORKLOAD_MODULE_SIZE);
	mars_ea_sync();

	return MARS_SUCCESS;

error:
	if (text_found)
		mars_ea_unmap(workload_module->text_ea,
			      workload_module->text_size);
	if (data_found)
		mars_ea_unmap(workload_module->data_ea,
			      workload_module->data_size);

	return ret;
}

static void remove_workload_module(struct mars_context *mars, uint16_t id)
{
	struct mars_workload_module *workload_module;

	/* prepare work area for workload module */
	workload_module = mars_ea_work_area_get(
				get_workload_ea(mars->workload_queue_ea, id),
				MARS_WORKLOAD_MODULE_ALIGN,
				MARS_WORKLOAD_MODULE_SIZE);

	/* get workload module from ea */
	mars_ea_get(mars->workload_queue_ea, workload_module,
		    MARS_WORKLOAD_MODULE_SIZE);

	mars_ea_unmap(workload_module->text_ea, workload_module->text_size);
	mars_ea_unmap(workload_module->data_ea, workload_module->data_size);
}

int mars_workload_queue_add_begin(struct mars_context *mars,
				  uint16_t *id,
				  uint64_t *workload_ea,
				  const void *workload_module_elf,
				  const char *workload_module_name)
{
	int ret;
	int block;
	int index = 0;
	uint64_t queue_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->workload_queue_ea)
		return MARS_ERROR_PARAMS;
	if (!id)
		return MARS_ERROR_NULL;
	if (!workload_module_elf)
		return MARS_ERROR_NULL;
	if (workload_module_name &&
	    strlen(workload_module_name) > MARS_WORKLOAD_MODULE_NAME_LEN_MAX)
		return MARS_ERROR_PARAMS;

	queue_ea = mars->workload_queue_ea;

	/* find free workload queue entry */
	for (block = 0; block < MARS_WORKLOAD_NUM_BLOCKS; block++) {
		uint64_t block_ea = get_block_ea(queue_ea, block);

		index = alloc_block(block_ea);
		if (index >= 0)
			break;
	}

	/* no more free workload queue entry */
	if (block >= MARS_WORKLOAD_NUM_BLOCKS)
		return MARS_ERROR_LIMIT;

	/* calculate and return workload id */
	*id = block * MARS_WORKLOAD_PER_BLOCK + index;

	/* add the workload module in workload context */
	ret = add_workload_module(get_workload_ea(queue_ea, *id),
				  workload_module_elf, workload_module_name);
	if (ret != MARS_SUCCESS) {
		change_state(mars, *id, NULL,
			     MARS_WORKLOAD_STATE_ADDING,
			     MARS_WORKLOAD_STATE_NONE,
			     NULL);
		return ret;
	}

	/* if requested set workload context pointer to return */
	if (workload_ea)
		*workload_ea = get_workload_ea(queue_ea, *id);

	return MARS_SUCCESS;
}

int mars_workload_queue_add_end(struct mars_context *mars,
				uint16_t id,
				int cancel)
{
	return change_state(mars, id, NULL,
			    MARS_WORKLOAD_STATE_ADDING,
			    cancel ? MARS_WORKLOAD_STATE_NONE :
				     MARS_WORKLOAD_STATE_FINISHED,
			    cancel ? remove_workload_module : NULL);
}

int mars_workload_queue_remove_begin(struct mars_context *mars,
				     uint16_t id,
				     uint64_t *workload_ea)
{
	return change_state(mars, id, workload_ea,
			    MARS_WORKLOAD_STATE_FINISHED,
			    MARS_WORKLOAD_STATE_REMOVING,
			    NULL);
}

int mars_workload_queue_remove_end(struct mars_context *mars,
				   uint16_t id,
				   int cancel)
{
	return change_state(mars, id, NULL,
			    MARS_WORKLOAD_STATE_REMOVING,
			    cancel ? MARS_WORKLOAD_STATE_FINISHED :
				     MARS_WORKLOAD_STATE_NONE,
			    cancel ? NULL : remove_workload_module);
}

static uint64_t set_schedule_bits(uint64_t bits, uint64_t priority)
{
	/* set the info bits inside queue block for this workload */
	MARS_BITS_SET(&bits, WORKLOAD_STATE, MARS_WORKLOAD_STATE_SCHEDULING);
	MARS_BITS_SET(&bits, WORKLOAD_PRIORITY, priority);
	MARS_BITS_SET(&bits, WORKLOAD_COUNTER, MARS_WORKLOAD_COUNTER_MIN);
	MARS_BITS_SET(&bits, WORKLOAD_SIGNAL, MARS_WORKLOAD_SIGNAL_OFF);
	MARS_BITS_SET(&bits, WORKLOAD_WAIT_ID, MARS_WORKLOAD_ID_NONE);

	return bits;
}

int mars_workload_queue_schedule_begin(struct mars_context *mars,
				       uint16_t id, uint8_t priority,
				       uint64_t *workload_ea)
{
	return change_bits(mars, id, workload_ea,
			   check_state_bits, MARS_WORKLOAD_STATE_FINISHED,
			   set_schedule_bits, priority,
			   NULL);
}

static void update_header_bits(struct mars_context *mars, uint16_t id)
{
	int block = id / MARS_WORKLOAD_PER_BLOCK;;
	int index;
	uint64_t queue_ea;
	uint64_t block_ea;
	uint64_t header_bits_ea;
	uint16_t header_bits;
	uint64_t header_access_ea;
	uint32_t header_access;
	uint8_t block_ready = MARS_WORKLOAD_BLOCK_READY_OFF;
	uint8_t block_waiting = MARS_WORKLOAD_BLOCK_WAITING_OFF;
	uint8_t block_priority = MARS_WORKLOAD_BLOCK_PRIORITY_MIN;

	queue_ea = mars->workload_queue_ea;

	block_ea = get_block_ea(queue_ea, block);

	/* search through currently locked queue block workload bits */
	for (index = 1; index < MARS_WORKLOAD_PER_BLOCK; index++) {
		uint64_t bits_ea = get_block_bits_ea(block_ea, index);
		uint64_t bits = mars_ea_get_uint64(bits_ea);
		uint8_t state = MARS_BITS_GET(&bits, WORKLOAD_STATE);

		/* workload state is ready so check priority */
		if (state == MARS_WORKLOAD_STATE_READY) {
			uint8_t priority = MARS_BITS_GET(&bits,
							 WORKLOAD_PRIORITY);

			/* set block priority if higher then current */
			if (priority > block_priority)
				block_priority = priority;

			/* set block ready bit in header bits for block */
			block_ready = MARS_WORKLOAD_BLOCK_READY_ON;
		} else if (state == MARS_WORKLOAD_STATE_WAITING) {
			/* set block waiting bit in header bits for block */
			block_waiting = MARS_WORKLOAD_BLOCK_WAITING_ON;
		}
	}

	/* lock the queue header */
	mars_mutex_lock(queue_ea);

	/* set the info bits inside queue header for this queue block */
	header_bits_ea = queue_ea +
			 offsetof(struct mars_workload_queue_header, bits) +
			 sizeof(uint16_t) * block;
	header_bits = mars_ea_get_uint16(header_bits_ea);

	MARS_BITS_SET(&header_bits, BLOCK_READY, block_ready);
	MARS_BITS_SET(&header_bits, BLOCK_WAITING, block_waiting);
	MARS_BITS_SET(&header_bits, BLOCK_PRIORITY, block_priority);

	mars_ea_put_uint16(header_bits_ea, header_bits);

	/* increment the header access value */
	header_access_ea = queue_ea +
			   offsetof(struct mars_workload_queue_header, access);
	header_access = mars_ea_get_uint32(header_access_ea);

	header_access++;

	mars_ea_put_uint32(header_access_ea, header_access);

	/* unlock the queue header */
	mars_mutex_unlock(queue_ea);
}

int mars_workload_queue_schedule_end(struct mars_context *mars,
				     uint16_t id,
				     int cancel)
{
	return change_state(mars, id, NULL,
			    MARS_WORKLOAD_STATE_SCHEDULING,
			    cancel ? MARS_WORKLOAD_STATE_FINISHED:
				     MARS_WORKLOAD_STATE_READY,
			    cancel ? NULL : update_header_bits);
}

static int unscheduling_state_bits(uint64_t bits, uint64_t param)
{
	(void)param;

	/* check for valid state */
	switch (MARS_BITS_GET(&bits, WORKLOAD_STATE)) {
	case MARS_WORKLOAD_STATE_READY:
	case MARS_WORKLOAD_STATE_RUNNING:
	case MARS_WORKLOAD_STATE_WAITING:
		return 1;
	default:
		return 0;
	}
}

int mars_workload_queue_unschedule_begin(struct mars_context *mars,
					 uint16_t id,
					 uint64_t *workload_ea)
{
	return change_bits(mars, id, workload_ea,
			  unscheduling_state_bits, 0,
			  set_state_bits, MARS_WORKLOAD_STATE_UNSCHEDULING,
			  NULL);
}

int mars_workload_queue_unschedule_end(struct mars_context *mars,
				       uint16_t id)
{
	int ret;
	int block;
	int index;
	uint64_t queue_ea;
	uint64_t block_ea;
	uint64_t bits_ea;

	ret = change_state(mars, id, NULL,
			   MARS_WORKLOAD_STATE_UNSCHEDULING,
			   MARS_WORKLOAD_STATE_FINISHED,
			   update_header_bits);
	if (ret != MARS_SUCCESS)
		return ret;

	queue_ea = mars->workload_queue_ea;

	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	block_ea = get_block_ea(queue_ea, block);
	bits_ea = get_block_bits_ea(block_ea, index);

	/* signal any threads possibly waiting for workload to finish */
	mars_ea_cond_signal(bits_ea, 1);

	return MARS_SUCCESS;
}

static int is_workload_finished(uint32_t upper, void *param)
{
	(void)param;

	/* this function assumes 'WORKLOAD_STATE' is stored in upper 32 bits */
	uint64_t bits = (uint64_t)upper << 32;

	switch (MARS_BITS_GET(&bits, WORKLOAD_STATE)) {
	case MARS_WORKLOAD_STATE_FINISHED:
		return MARS_SUCCESS;
	case MARS_WORKLOAD_STATE_NONE:
		return MARS_ERROR_STATE;
	default:
		return -1;
	}
}

static int workload_queue_wait(struct mars_context *mars,
			       uint16_t id,
			       int try,
			       uint64_t *workload_ea)
{
	int ret;
	int block;
	int index;
	uint64_t queue_ea;
	uint64_t block_ea;
	uint64_t bits_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->workload_queue_ea)
		return MARS_ERROR_PARAMS;
	if (id > MARS_WORKLOAD_ID_MAX || !(id % MARS_WORKLOAD_PER_BLOCK))
		return MARS_ERROR_PARAMS;

	queue_ea = mars->workload_queue_ea;

	/* calculate block/index from id */
	block = id / MARS_WORKLOAD_PER_BLOCK;
	index = id % MARS_WORKLOAD_PER_BLOCK;

	/* prepare work area for queue block */
	block_ea = get_block_ea(queue_ea, block);
	bits_ea = get_block_bits_ea(block_ea, index);

	if (try) {
		ret = is_workload_finished(mars_ea_get_uint32(bits_ea), NULL);
		if (ret < 0)
			ret = MARS_ERROR_BUSY;
	} else {
		ret = mars_ea_cond_wait(bits_ea, is_workload_finished, NULL);
	}

	if (ret != MARS_SUCCESS)
		return ret;

	/* if requested set workload context pointer to return */
	if (workload_ea)
		*workload_ea = get_workload_ea(queue_ea, id);

	return MARS_SUCCESS;
}

int mars_workload_queue_wait(struct mars_context *mars,
			     uint16_t id,
			     uint64_t *workload_ea)
{
	return workload_queue_wait(mars, id, 0, workload_ea);
}

int mars_workload_queue_try_wait(struct mars_context *mars,
				 uint16_t id,
				 uint64_t *workload_ea)
{
	return workload_queue_wait(mars, id, 1, workload_ea);
}

static uint64_t set_signal_bits(uint64_t bits, uint64_t signal)
{
	MARS_BITS_SET(&bits, WORKLOAD_SIGNAL, signal);

	return bits;
}

int mars_workload_queue_signal_send(struct mars_context *mars,
				    uint16_t id)
{
	return change_bits(mars, id, NULL,
			   check_state_bits_not, MARS_WORKLOAD_STATE_NONE,
			   set_signal_bits, MARS_WORKLOAD_SIGNAL_ON,
			   update_header_bits);
}
