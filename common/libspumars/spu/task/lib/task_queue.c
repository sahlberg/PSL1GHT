#include <stddef.h>

#include <mars/error.h>
#include <mars/task_types.h>
#include <mars/task_queue.h>

#include "task_internal_types.h"
#include "task_queue_internal_types.h"
#include "../module/task_module.h"

static struct mars_task_queue queue;

int mars_task_queue_count(uint64_t queue_ea, uint32_t *count)
{
	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (!count)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);

	/* return current items in queue */
	*count = queue.count;

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	return MARS_SUCCESS;
}

int mars_task_queue_clear(uint64_t queue_ea)
{
	int i;
	int signal_host;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);

	/* whether signal the waiting host */
	signal_host =
		(queue.count == queue.depth &&
		 queue.direction != MARS_TASK_QUEUE_MPU_TO_HOST &&
		 queue.direction != MARS_TASK_QUEUE_MPU_TO_MPU);

	/* signal all waiting tasks that queue is ready for push */
	for (i = 0; i < queue.push_wait_count; i++)
		mars_task_module_signal_send(queue.push_wait_id[i]);

	/* flush all ids from push wait list */
	queue.push_wait_count = 0;

	/* clear the queue count and push/pop ea */
	queue.count = 0;
	queue.push_ea = queue.buffer_ea;
	queue.pop_ea = queue.buffer_ea;

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	/* signal the waiting host */
	if (signal_host)
		mars_task_module_signal_host(
			queue_ea + offsetof(struct mars_task_queue, count));

	return MARS_SUCCESS;
}

static int push_update(void)
{
	int signal_host;

	/* whether signal the waiting host */
	signal_host =
		(queue.count == 0 &&
		 queue.direction != MARS_TASK_QUEUE_HOST_TO_MPU &&
		 queue.direction != MARS_TASK_QUEUE_MPU_TO_MPU);

	/* signal waiting task that queue is ready for pop */
	if (queue.pop_wait_count) {
		/* signal waiting task */
		mars_task_module_signal_send(
			queue.pop_wait_id[queue.pop_wait_head]);

		/* flush id from pop wait list */
		queue.pop_wait_count--;
		queue.pop_wait_head++;
		queue.pop_wait_head %= MARS_TASK_QUEUE_WAIT_MAX;
	}

	/* increment queue count */
	queue.count++;

	/* increment queue push ea */
	queue.push_ea += queue.size;

	/* wrap to front of queue if necessary */
	if (queue.push_ea == queue.buffer_ea + (queue.size * queue.depth))
		queue.push_ea = queue.buffer_ea;

	return signal_host;
}

static int push(uint64_t queue_ea, const void *data,
		int try, int begin, uint32_t tag)
{
	struct mars_task_context *task;
	int signal_host;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if ((uintptr_t)data & MARS_TASK_QUEUE_ENTRY_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (tag > MARS_TASK_MODULE_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea && !try)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);

	/* check for valid direction */
	if (queue.direction != MARS_TASK_QUEUE_MPU_TO_HOST &&
	    queue.direction != MARS_TASK_QUEUE_MPU_TO_MPU) {
		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);
		return MARS_ERROR_STATE;
	}

	/* queue is full so wait */
	while (queue.count == queue.depth) {
		uint8_t push_wait_tail;

		/* only try so return busy */
		if (try) {
			mars_mutex_unlock_put(queue_ea,
					      (struct mars_mutex *)&queue);
			return MARS_ERROR_BUSY;
		}

		/* check if push wait list full */
		if (queue.push_wait_count == MARS_TASK_QUEUE_WAIT_MAX) {
			mars_mutex_unlock_put(queue_ea,
					      (struct mars_mutex *)&queue);
			return MARS_ERROR_LIMIT;
		}

		/* add id to push wait list */
		push_wait_tail = (queue.push_wait_head + queue.push_wait_count)
				 % MARS_TASK_QUEUE_WAIT_MAX;
		queue.push_wait_id[push_wait_tail] = task->id.workload_id;
		queue.push_wait_count++;

		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);
	}

	/* begin data transfer into queue */
	mars_dma_put(data, queue.push_ea, queue.size, tag);

	/* only begin data transfer so return */
	if (begin)
		return MARS_SUCCESS;

	/* wait for dma completion */
	mars_dma_wait(tag);

	/* update queue data */
	signal_host = push_update();

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	/* signal the waiting host */
	if (signal_host)
		mars_task_module_signal_host(
			queue_ea + offsetof(struct mars_task_queue, count));

	return MARS_SUCCESS;
}

int mars_task_queue_push(uint64_t queue_ea, const void *data)
{
	return push(queue_ea, data, 0, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_push_begin(uint64_t queue_ea, const void *data,
			       uint32_t tag)
{
	return push(queue_ea, data, 0, 1, tag);
}

int mars_task_queue_push_end(uint64_t queue_ea, uint32_t tag)
{
	int signal_host;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (tag > MARS_TASK_MODULE_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	/* wait for dma completion */
	mars_dma_wait(tag);

	/* update queue data */
	signal_host = push_update();

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	/* signal the waiting host */
	if (signal_host)
		mars_task_module_signal_host(
			queue_ea + offsetof(struct mars_task_queue, count));

	return MARS_SUCCESS;
}

int mars_task_queue_try_push(uint64_t queue_ea, const void *data)
{
	return push(queue_ea, data, 1, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_try_push_begin(uint64_t queue_ea, const void *data,
				   uint32_t tag)
{
	return push(queue_ea, data, 1, 1, tag);
}

static int pop_update(void)
{
	int signal_host;

	/* whether signal the waiting host */
	signal_host =
		(queue.count == queue.depth &&
		 queue.direction != MARS_TASK_QUEUE_MPU_TO_HOST &&
		 queue.direction != MARS_TASK_QUEUE_MPU_TO_MPU);

	/* signal waiting task that queue is ready for push */
	if (queue.push_wait_count) {
		/* signal waiting task */
		mars_task_module_signal_send(
			queue.push_wait_id[queue.push_wait_head]);

		/* flush id from push wait list */
		queue.push_wait_count--;
		queue.push_wait_head++;
		queue.push_wait_head %= MARS_TASK_QUEUE_WAIT_MAX;
	}

	/* decrement queue count */
	queue.count--;

	/* increment queue pop ea */
	queue.pop_ea += queue.size;

	/* wrap to front of queue if necessary */
	if (queue.pop_ea == queue.buffer_ea + (queue.size * queue.depth))
		queue.pop_ea = queue.buffer_ea;

	return signal_host;
}

static int pop(uint64_t queue_ea, void *data,
	       int peek, int try, int begin, uint32_t tag)
{
	struct mars_task_context *task;
	int signal_host;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if ((uintptr_t)data & MARS_TASK_QUEUE_ENTRY_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (tag > MARS_TASK_MODULE_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea && !try)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);

	/* check for valid direction */
	if (queue.direction != MARS_TASK_QUEUE_HOST_TO_MPU &&
	    queue.direction != MARS_TASK_QUEUE_MPU_TO_MPU) {
		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);
		return MARS_ERROR_STATE;
	}

	/* queue is empty so wait */
	while (!queue.count) {
		uint8_t pop_wait_tail;

		/* only try so return busy */
		if (try) {
			mars_mutex_unlock_put(queue_ea,
					      (struct mars_mutex *)&queue);
			return MARS_ERROR_BUSY;
		}

		/* check if pop wait list full */
		if (queue.pop_wait_count == MARS_TASK_QUEUE_WAIT_MAX) {
			mars_mutex_unlock_put(queue_ea,
					      (struct mars_mutex *)&queue);
			return MARS_ERROR_LIMIT;
		}

		/* add id to pop wait list */
		pop_wait_tail = (queue.pop_wait_head + queue.pop_wait_count)
				% MARS_TASK_QUEUE_WAIT_MAX;
		queue.pop_wait_id[pop_wait_tail] = task->id.workload_id;
		queue.pop_wait_count++;

		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		mars_mutex_lock_get(queue_ea, (struct mars_mutex *)&queue);
	}

	/* begin data transfer from queue */
	mars_dma_get(data, queue.pop_ea, queue.size, tag);

	/* only begin data transfer so return */
	if (begin)
		return MARS_SUCCESS;

	/* wait for dma completion */
	mars_dma_wait(tag);

	/* update queue data only if this is not a peek operation */
	if (!peek)
		signal_host = pop_update();
	else
		signal_host = 0;

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	/* signal the waiting host */
	if (signal_host)
		mars_task_module_signal_host(
			queue_ea + offsetof(struct mars_task_queue, count));

	return MARS_SUCCESS;
}

int mars_task_queue_pop(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 0, 0, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_pop_begin(uint64_t queue_ea, void *data, uint32_t tag)
{
	return pop(queue_ea, data, 0, 0, 1, tag);
}

int mars_task_queue_pop_end(uint64_t queue_ea, uint32_t tag)
{
	int signal_host;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (tag > MARS_TASK_MODULE_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	/* wait for dma completion */
	mars_dma_wait(tag);

	/* update queue data */
	signal_host = pop_update();

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	/* signal the waiting host */
	if (signal_host)
		mars_task_module_signal_host(
			queue_ea + offsetof(struct mars_task_queue, count));

	return MARS_SUCCESS;
}

int mars_task_queue_try_pop(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 0, 1, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_try_pop_begin(uint64_t queue_ea, void *data, uint32_t tag)
{
	return pop(queue_ea, data, 0, 1, 1, tag);
}

int mars_task_queue_peek(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 1, 0, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_peek_begin(uint64_t queue_ea, void *data, uint32_t tag)
{
	return pop(queue_ea, data, 1, 0, 1, tag);
}

int mars_task_queue_peek_end(uint64_t queue_ea, uint32_t tag)
{
	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (tag > MARS_TASK_MODULE_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	/* wait for dma completion */
	mars_dma_wait(tag);

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)&queue);

	return MARS_SUCCESS;
}

int mars_task_queue_try_peek(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 1, 1, 0, MARS_TASK_MODULE_DMA_TAG);
}

int mars_task_queue_try_peek_begin(uint64_t queue_ea, void *data, uint32_t tag)
{
	return pop(queue_ea, data, 1, 1, 1, tag);
}
