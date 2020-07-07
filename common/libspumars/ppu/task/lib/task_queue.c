#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/mutex.h>
#include <mars/workload_queue.h>

#include "mars/task_queue.h"

#include "task_queue_internal_types.h"

static inline uint64_t queue_count_ea(uint64_t queue_ea)
{
	return queue_ea + offsetof(struct mars_task_queue, count);
}

int mars_task_queue_create(struct mars_context *mars,
			   uint64_t *queue_ea_ret,
			   uint32_t size,
			   uint32_t depth,
			   uint8_t direction)
{
	struct mars_task_queue *queue;
	uint64_t queue_ea;
	uint64_t buffer_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!queue_ea_ret)
		return MARS_ERROR_NULL;
	if (size & MARS_TASK_QUEUE_ENTRY_SIZE_MASK)
		return MARS_ERROR_PARAMS;
	if (size > MARS_TASK_QUEUE_ENTRY_SIZE_MAX)
		return MARS_ERROR_PARAMS;
	if (direction != MARS_TASK_QUEUE_HOST_TO_MPU &&
	    direction != MARS_TASK_QUEUE_MPU_TO_HOST &&
	    direction != MARS_TASK_QUEUE_MPU_TO_MPU)
		return MARS_ERROR_PARAMS;

	/* allocate queue instance */
	queue_ea = mars_ea_memalign(MARS_TASK_QUEUE_ALIGN,
				    MARS_TASK_QUEUE_SIZE);
	if (!queue_ea)
		return MARS_ERROR_MEMORY;

	/* prepare work area for initialization */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_TASK_QUEUE_ALIGN,
				      MARS_TASK_QUEUE_SIZE);

	/* allocate queue buffer instance */
	buffer_ea = mars_ea_memalign(MARS_TASK_QUEUE_BUFFER_ALIGN,
				     size * depth);
	if (!buffer_ea) {
		mars_ea_free(queue_ea);
		return MARS_ERROR_MEMORY;
	}

	/* initialize queue instance */
	queue->mars_context_ea = mars_ptr_to_ea(mars);
	queue->buffer_ea = buffer_ea;
	queue->push_ea = buffer_ea;
	queue->pop_ea = buffer_ea;
	queue->size = size;
	queue->depth = depth;
	queue->direction = direction;
	queue->count = 0;
	queue->push_wait_count = 0;
	queue->pop_wait_count = 0;
	queue->push_wait_head = 0;
	queue->pop_wait_head = 0;

	/* update queue on EA */
	mars_ea_put(queue_ea, queue, MARS_TASK_QUEUE_SIZE);
	mars_ea_sync();

	mars_mutex_reset(queue_ea);

	/* return queue instance pointer */
	*queue_ea_ret = queue_ea;

	return MARS_SUCCESS;
}

int mars_task_queue_destroy(uint64_t queue_ea)
{
	struct mars_task_queue *queue;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_TASK_QUEUE_ALIGN,
				      MARS_TASK_QUEUE_SIZE);

	mars_ea_get(queue_ea, queue, MARS_TASK_QUEUE_SIZE);

	/* make sure no tasks in wait list */
	if (queue->push_wait_count || queue->pop_wait_count)
		return MARS_ERROR_STATE;

	/* free shared memory */
	mars_ea_free(queue->buffer_ea);
	mars_ea_free(queue_ea);

	return MARS_SUCCESS;
}

int mars_task_queue_count(uint64_t queue_ea, uint32_t *count)
{
	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (!count)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* get queue from EA */
	mars_mutex_lock(queue_ea);

	/* return current items in queue */
	*count = mars_ea_get_uint32(queue_count_ea(queue_ea));

	mars_mutex_unlock(queue_ea);

	return MARS_SUCCESS;
}

int mars_task_queue_clear(uint64_t queue_ea)
{
	int i;
	struct mars_context *mars;
	struct mars_task_queue *queue;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* prepare work area */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_TASK_QUEUE_ALIGN,
				      MARS_TASK_QUEUE_SIZE);

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);

	/* get mars context pointer */
	mars = mars_ea_to_ptr(queue->mars_context_ea);

	/* signal all waiting tasks that queue is ready for push */
	for (i = 0; i < queue->push_wait_count; i++)
		mars_workload_queue_signal_send(mars, queue->push_wait_id[0]);

	/* flush all ids from push wait list */
	queue->push_wait_count = 0;

	/* clear the queue count and push/pop ea */
	queue->count = 0;
	queue->push_ea = queue->buffer_ea;
	queue->pop_ea = queue->buffer_ea;

	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

	return MARS_SUCCESS;
}

static void push_update(struct mars_task_queue *queue)
{
	struct mars_context *mars = mars_ea_to_ptr(queue->mars_context_ea);

	/* signal waiting task that queue is ready for pop */
	if (queue->pop_wait_count) {
		/* signal waiting task */
		mars_workload_queue_signal_send(mars,
			queue->pop_wait_id[queue->pop_wait_head]);

		/* flush id from pop wait list */
		queue->pop_wait_count--;
		queue->pop_wait_head++;
		queue->pop_wait_head %= MARS_TASK_QUEUE_WAIT_MAX;
	}

	/* increment queue count */
	queue->count++;

	/* increment queue push ea */
	queue->push_ea += queue->size;

	/* wrap to front of queue if necessary */
	if (queue->push_ea == queue->buffer_ea + (queue->size * queue->depth))
		queue->push_ea = queue->buffer_ea;
}

static int test_not_full(uint32_t count, void *param)
{
	const struct mars_task_queue *queue =
		(const struct mars_task_queue *)param;

	return (count != queue->depth) ? MARS_SUCCESS : -1;
}

static int push(uint64_t queue_ea, const void *data, int try)
{
	int ret;
	struct mars_task_queue *queue;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* prepare work area */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_TASK_QUEUE_ALIGN,
				      MARS_TASK_QUEUE_SIZE);

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);

	/* check queue status */
	ret = MARS_ERROR_STATE;
	if (queue->direction != MARS_TASK_QUEUE_HOST_TO_MPU)
		goto end;

	/* queue is full so wait */
	while (queue->count == queue->depth) {
		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

		/* only try so return busy */
		if (try)
			return MARS_ERROR_BUSY;

		ret = mars_ea_cond_wait(queue_count_ea(queue_ea),
					test_not_full, queue);
		if (ret)
			return ret;

		mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);
	}

	/* copy data into queue */
	mars_ea_put(queue->push_ea, data, queue->size);

	/* update queue data */
	push_update(queue);

	ret = MARS_SUCCESS;
end:
	mars_mutex_unlock_put(queue_ea, (struct mars_mutex*)queue);

	return ret;
}

int mars_task_queue_push(uint64_t queue_ea, const void *data)
{
	return push(queue_ea, data, 0);
}

int mars_task_queue_try_push(uint64_t queue_ea, const void *data)
{
	return push(queue_ea, data, 1);
}

static void pop_update(struct mars_task_queue *queue)
{
	struct mars_context *mars = mars_ea_to_ptr(queue->mars_context_ea);

	/* signal waiting task that queue is ready for push */
	if (queue->push_wait_count) {
		/* signal waiting task */
		mars_workload_queue_signal_send(mars,
			queue->push_wait_id[queue->push_wait_head]);

		/* flush id from push wait list */
		queue->push_wait_count--;
		queue->push_wait_head++;
		queue->push_wait_head %= MARS_TASK_QUEUE_WAIT_MAX;
	}

	/* decrement queue count */
	queue->count--;

	/* increment queue pop ea */
	queue->pop_ea += queue->size;

	/* wrap to front of queue if necessary */
	if (queue->pop_ea == queue->buffer_ea + (queue->size * queue->depth))
		queue->pop_ea = queue->buffer_ea;
}

static int test_not_empty(uint32_t count, void *param)
{
	(void)param;

	return count ? MARS_SUCCESS : -1;
}

static int pop(uint64_t queue_ea, void *data, int peek, int try)
{
	int ret;
	struct mars_task_queue *queue;

	/* check function params */
	if (!queue_ea)
		return MARS_ERROR_NULL;
	if (queue_ea & MARS_TASK_QUEUE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* prepare work area */
	queue = mars_ea_work_area_get(queue_ea,
				      MARS_TASK_QUEUE_ALIGN,
				      MARS_TASK_QUEUE_SIZE);

	mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);

	ret = MARS_ERROR_STATE;
	if (queue->direction != MARS_TASK_QUEUE_MPU_TO_HOST)
		goto end;

	/* queue is empty so wait */
	while (!queue->count) {
		mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

		/* only try so return busy */
		if (try)
			return MARS_ERROR_BUSY;

		ret = mars_ea_cond_wait(queue_count_ea(queue_ea),
					test_not_empty, queue);
		if (ret)
			return ret;

		mars_mutex_lock_get(queue_ea, (struct mars_mutex *)queue);
	}

	/* copy data from queue */
	mars_ea_get(queue->pop_ea, data, queue->size);

	/* update queue data only if this is not a peek operation */
	if (!peek)
		pop_update(queue);

	ret = MARS_SUCCESS;
end:
	mars_mutex_unlock_put(queue_ea, (struct mars_mutex *)queue);

	return ret;
}

int mars_task_queue_pop(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 0, 0);
}

int mars_task_queue_try_pop(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 0, 1);
}

int mars_task_queue_peek(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 1, 0);
}

int mars_task_queue_try_peek(uint64_t queue_ea, void *data)
{
	return pop(queue_ea, data, 1, 1);
}
