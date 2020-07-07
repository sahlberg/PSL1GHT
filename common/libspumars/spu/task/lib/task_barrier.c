#include <mars/error.h>

#include <mars/task_barrier.h>
#include <mars/task_types.h>

#include "task_barrier_internal_types.h"
#include "task_internal_types.h"
#include "../module/task_module.h"

static struct mars_task_barrier barrier;

static int notify(uint64_t barrier_ea, int try)
{
	int i;
	struct mars_task_context *task;

	/* check function params */
	if (!barrier_ea)
		return MARS_ERROR_NULL;
	if (barrier_ea & MARS_TASK_BARRIER_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea && !try)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(barrier_ea, (struct mars_mutex *)&barrier);

	/* previous barrier wait not complete so wait */
	if (barrier.notified_count == barrier.total) {
		/* only try so return busy */
		if (try) {
			mars_mutex_unlock_put(barrier_ea,
					      (struct mars_mutex *)&barrier);
			return MARS_ERROR_BUSY;
		}

		/* check if barrier notify wait limit reached */
		if (barrier.notify_wait_count == barrier.total) {
			mars_mutex_unlock_put(barrier_ea,
					      (struct mars_mutex *)&barrier);
			return MARS_ERROR_LIMIT;
		}

		/* add id to wait list */
		barrier.notify_wait_id[barrier.notify_wait_count] =
			task->id.workload_id;
		barrier.notify_wait_count++;

		mars_mutex_unlock_put(barrier_ea,
				      (struct mars_mutex *)&barrier);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		mars_mutex_lock_get(barrier_ea, (struct mars_mutex *)&barrier);
	}

	/* increment notified count */
	barrier.notified_count++;

	/* notified count reached total so release barrier */
	if (barrier.notified_count == barrier.total) {
		/* signal all task ids in wait list */
		for (i = 0; i < barrier.wait_count; i++)
			mars_task_module_signal_send(barrier.wait_id[i]);
		barrier.wait_count = 0;
	}

	mars_mutex_unlock_put(barrier_ea, (struct mars_mutex *)&barrier);

	return MARS_SUCCESS;
}

int mars_task_barrier_notify(uint64_t barrier_ea)
{
	return notify(barrier_ea, 0);
}

int mars_task_barrier_try_notify(uint64_t barrier_ea)
{
	return notify(barrier_ea, 1);
}

static int wait(uint64_t barrier_ea, int try)
{
	int i;
	struct mars_task_context *task;

	/* check function params */
	if (!barrier_ea)
		return MARS_ERROR_NULL;
	if (barrier_ea & MARS_TASK_BARRIER_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea && !try)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(barrier_ea, (struct mars_mutex *)&barrier);

	/* not all tasks notified barrier so need to wait */
	if (barrier.notified_count != barrier.total) {
		/* only try so return busy */
		if (try) {
			mars_mutex_unlock_put(barrier_ea,
					      (struct mars_mutex *)&barrier);
			return MARS_ERROR_BUSY;
		}

		/* check if barrier wait limit reached */
		if (barrier.wait_count == barrier.total) {
			mars_mutex_unlock_put(barrier_ea,
					      (struct mars_mutex *)&barrier);
			return MARS_ERROR_LIMIT;
		}

		/* add id to wait list */
		barrier.wait_id[barrier.wait_count] = task->id.workload_id;
		barrier.wait_count++;

		mars_mutex_unlock_put(barrier_ea,
				      (struct mars_mutex *)&barrier);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		mars_mutex_lock_get(barrier_ea, (struct mars_mutex *)&barrier);
	}

	/* increment waited count */
	barrier.waited_count++;

	/* all tasks have called wait so reset barrier */
	if (barrier.waited_count == barrier.total) {
		barrier.notified_count = 0;
		barrier.waited_count = 0;

		/* signal all task ids in notify wait list */
		for (i = 0; i < barrier.notify_wait_count; i++)
			mars_task_module_signal_send(barrier.notify_wait_id[i]);
		barrier.notify_wait_count = 0;
	}

	mars_mutex_unlock_put(barrier_ea, (struct mars_mutex *)&barrier);

	return MARS_SUCCESS;
}

int mars_task_barrier_wait(uint64_t barrier_ea)
{
	return wait(barrier_ea, 0);
}

int mars_task_barrier_try_wait(uint64_t barrier_ea)
{
	return wait(barrier_ea, 1);
}
