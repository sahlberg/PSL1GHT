#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <mars/base.h>
#include <mars/error.h>
#include <mars/mutex.h>
#include <mars/workload_queue.h>

#include "callback_internal_types.h"
#include "kernel_internal_types.h"
#include "workload_internal_types.h"

#include "context_internal.h"

static inline uint64_t get_workload_ea(uint64_t queue_ea, int workload_id)
{
	uint64_t context_ea =
		mars_ea_get_uint64(queue_ea +
				   offsetof(struct mars_workload_queue_header,
				   context_ea));
	int context_index =
		workload_id - (workload_id / MARS_WORKLOAD_PER_BLOCK) - 1;

	return context_ea + context_index * MARS_WORKLOAD_CONTEXT_SIZE;
}

static void callback_process(struct mars_context *mars, uint16_t workload_id)
{
	uint64_t workload_callback_ea;
	struct mars_workload_callback *workload_callback;
	struct mars_callback_args out;

	/* busy loop until workload is put into wait state */
	while (!mars_workload_queue_query(mars, workload_id,
					  MARS_WORKLOAD_QUERY_IS_WAITING)) {
	}

	/* get ea of workload callback structure */
	workload_callback_ea =
		get_workload_ea(mars->workload_queue_ea, workload_id) +
		MARS_WORKLOAD_MODULE_SIZE;

	/* prepare work area for queue */
	workload_callback =
		mars_ea_work_area_get(workload_callback_ea,
				      MARS_WORKLOAD_CALLBACK_ALIGN,
				      MARS_WORKLOAD_CALLBACK_SIZE);

	/* get workload callback structure from ea */
	mars_ea_get(workload_callback_ea, workload_callback,
		    MARS_WORKLOAD_CALLBACK_SIZE);

	/* call the callback and store the return value */
	workload_callback->callback_ret =
		((mars_callback)
			mars_ea_to_ptr(workload_callback->callback_ea))
				(&workload_callback->callback_args, &out);

	/* copy output args back into callback structure */
	workload_callback->callback_args = out;

	/* update workload data on ea */
	mars_ea_put(workload_callback_ea, workload_callback,
		    MARS_WORKLOAD_CALLBACK_SIZE);
}

static int callback_queue_pop(uint64_t queue_ea, uint16_t *workload_id)
{
	struct mars_callback_queue *queue;

	/* prepare work area for queue */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_CALLBACK_QUEUE_ALIGN,
				      MARS_CALLBACK_QUEUE_SIZE);

	/* lock the queue */
	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);

	/* queue is empty (false signal?) */
	if (!queue->count) {
		mars_mutex_unlock(queue_ea);
		return MARS_ERROR_STATE;
	}

	/* get workload id from head of queue */
	*workload_id = queue->workload_id[queue->head];

	/* decrement count */
	queue->count--;

	/* increment head */
	queue->head++;

	/* wrap head to front of queue if necessary */
	if (queue->head == MARS_CALLBACK_QUEUE_MAX)
		queue->head = 0;

	/* if queue is empty reset the queue flag */
	if (!queue->count)
		queue->flag = MARS_CALLBACK_QUEUE_FLAG_NONE;

	/* unlock the queue */
	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

	return MARS_SUCCESS;
}

static int callback_queue_state(uint32_t flag, void *param)
{
	(void)param;

	/* exit flag is set so return */
	switch (flag) {
	case MARS_CALLBACK_QUEUE_FLAG_EXIT:
		return MARS_ERROR_STATE;
	case MARS_CALLBACK_QUEUE_FLAG_PUSH:
		return MARS_SUCCESS;
	default:
		return -1;
	}
}

static void callback_handler_thread(void *arg)
{
	struct mars_context *mars = (struct mars_context *)arg;
	uint64_t queue_ea = mars->callback_queue_ea;
	uint16_t workload_id;
	int ret;

	while (1) {
		/* wait until kernel requests callback processing */
		ret = mars_ea_cond_wait(
			queue_ea + offsetof(struct mars_callback_queue, flag),
			callback_queue_state, NULL);
		if (ret != MARS_SUCCESS)
			break;

		/* pop the workload id requesting callback */
		ret = callback_queue_pop(queue_ea, &workload_id);
		if (ret != MARS_SUCCESS)
			continue;

		/* process the callback requested by workload */
		callback_process(mars, workload_id);

		/* signal the workload for completion of callback */
		mars_workload_queue_signal_send(mars, workload_id);
	}

	sysThreadExit(0);
}

int mars_callback_queue_create(struct mars_context *mars, uint32_t ppu_prio)
{
	int ret;
	int i;
	uint64_t queue_ea;
	struct mars_callback_queue *queue;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (mars->callback_queue_ea)
		return MARS_ERROR_STATE;

	/* allocate queue instance */
	queue_ea = mars_ea_memalign(MARS_CALLBACK_QUEUE_ALIGN,
				    MARS_CALLBACK_QUEUE_SIZE);
	if (!queue_ea)
		return MARS_ERROR_MEMORY;

	/* prepare work area for queue */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_CALLBACK_QUEUE_ALIGN,
				      MARS_CALLBACK_QUEUE_SIZE);

	/* initialize queue structure */
	queue->flag = MARS_CALLBACK_QUEUE_FLAG_NONE;
	queue->count = 0;
	queue->head = 0;
	queue->tail = 0;

	for (i = 0; i < MARS_CALLBACK_QUEUE_MAX; i++)
		queue->workload_id[i] = MARS_WORKLOAD_ID_NONE;

	/* update queue on EA */
	mars_ea_put(queue_ea, queue, MARS_CALLBACK_QUEUE_SIZE);

	/* reset mutex portion of queue header */
	mars_mutex_reset(queue_ea);

	/* sync EA */
	mars_ea_sync();

	/* set the host callback queue instance in the mars context */
	mars->callback_queue_ea = queue_ea;

	/* create the host callback handler thread */
	ret = sysThreadCreate(&mars->callback_handler, callback_handler_thread, mars, ppu_prio, 4096, THREAD_JOINABLE, "mars_callback_handler");
	if (ret) {
		mars_ea_free(queue_ea);
		return MARS_ERROR_INTERNAL;
	}

	return MARS_SUCCESS;
}

int mars_callback_queue_destroy(struct mars_context *mars)
{
	int ret;
	uint64_t ret_val;
	uint64_t queue_ea;
	struct mars_callback_queue *queue;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->callback_queue_ea)
		return MARS_ERROR_PARAMS;

	queue_ea = mars->callback_queue_ea;

	/* prepare work area for queue */
	queue = mars_ea_work_area_get(mars->callback_queue_ea,
				      MARS_CALLBACK_QUEUE_ALIGN,
				      MARS_CALLBACK_QUEUE_SIZE);

	/* get queue from ea */
	mars_ea_get(queue_ea, queue, MARS_CALLBACK_QUEUE_SIZE);

	/* make sure queue is empty */
	if (queue->count)
		return MARS_ERROR_STATE;
	
	/* join the host callback handler thread */
	ret = sysThreadJoin(mars->callback_handler, &ret_val);
	if (ret)
		return MARS_ERROR_INTERNAL;
	
	/* free host callback queue instance */
	mars_ea_free(queue_ea);

	/* set the workload queue to NULL for error checking */
	mars->callback_queue_ea = 0;

	return MARS_SUCCESS;
}

int mars_callback_queue_exit(struct mars_context *mars)
{
	uint64_t queue_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!mars->callback_queue_ea)
		return MARS_ERROR_PARAMS;

	queue_ea = mars->callback_queue_ea;

	/* set callback queue exit flag */
	mars_ea_put_uint32(
		queue_ea + offsetof(struct mars_callback_queue, flag),
		MARS_CALLBACK_QUEUE_FLAG_EXIT);

	/* signal callback handler to wake up from wait */
	mars_ea_cond_signal(
		queue_ea + offsetof(struct mars_callback_queue, flag), 1);

	return MARS_SUCCESS;
}
