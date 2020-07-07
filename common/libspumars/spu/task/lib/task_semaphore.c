#include <mars/error.h>
#include <mars/task_types.h>
#include <mars/task_semaphore.h>

#include "task_internal_types.h"
#include "task_semaphore_internal_types.h"
#include "../module/task_module.h"

static struct mars_task_semaphore semaphore;

int mars_task_semaphore_acquire(uint64_t semaphore_ea)
{
	struct mars_task_context *task;

	/* check function params */
	if (!semaphore_ea)
		return MARS_ERROR_NULL;
	if (semaphore_ea & MARS_TASK_SEMAPHORE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(semaphore_ea, (struct mars_mutex *)&semaphore);

	/* check if semaphore wait limit reached */
	if (semaphore.wait_count == MARS_TASK_SEMAPHORE_WAIT_MAX) {
		mars_mutex_unlock_put(semaphore_ea,
				      (struct mars_mutex *)&semaphore);
		return MARS_ERROR_LIMIT;
	}

	if (semaphore.count <= 0) {
		uint8_t wait_tail = (semaphore.wait_head + semaphore.wait_count)
				    % MARS_TASK_SEMAPHORE_WAIT_MAX;

		/* add id to wait list */
		semaphore.wait_id[wait_tail] = task->id.workload_id;
		semaphore.wait_count++;

		mars_mutex_unlock_put(semaphore_ea,
				      (struct mars_mutex *)&semaphore);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		return MARS_SUCCESS;
	}

	/* decrement semaphore count */
	semaphore.count--;

	mars_mutex_unlock_put(semaphore_ea, (struct mars_mutex *)&semaphore);

	return MARS_SUCCESS;
}

int mars_task_semaphore_release(uint64_t semaphore_ea)
{
	/* check function params */
	if (!semaphore_ea)
		return MARS_ERROR_NULL;
	if (semaphore_ea & MARS_TASK_SEMAPHORE_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	mars_mutex_lock_get(semaphore_ea, (struct mars_mutex *)&semaphore);

	/* increment semaphore count */
	semaphore.count++;

	/* signal those that are waiting */
	if (semaphore.count > 0 && semaphore.wait_count) {
		/* decrement semaphore count */
		semaphore.count--;

		/* signal waiting task */
		mars_task_module_signal_send(
			semaphore.wait_id[semaphore.wait_head]);

		/* flush id from wait list */
		semaphore.wait_count--;
		semaphore.wait_head++;
		semaphore.wait_head %= MARS_TASK_SEMAPHORE_WAIT_MAX;
	}

	mars_mutex_unlock_put(semaphore_ea, (struct mars_mutex *)&semaphore);

	return MARS_SUCCESS;
}
