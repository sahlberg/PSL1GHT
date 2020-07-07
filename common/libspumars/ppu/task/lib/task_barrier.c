#include <mars/base.h>
#include <mars/error.h>
#include <mars/mutex.h>

#include "mars/task_barrier.h"

#include "task_barrier_internal_types.h"

int mars_task_barrier_create(struct mars_context *mars,
			     uint64_t *barrier_ea_ret,
			     uint32_t total)
{
	struct mars_task_barrier *barrier;
	uint64_t barrier_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!barrier_ea_ret)
		return MARS_ERROR_NULL;
	if (!total || total > MARS_TASK_BARRIER_WAIT_MAX)
		return MARS_ERROR_PARAMS;

	/* allocate barrier instance */
	barrier_ea = mars_ea_memalign(MARS_TASK_BARRIER_ALIGN,
				      MARS_TASK_BARRIER_SIZE);
	if (!barrier_ea)
		return MARS_ERROR_MEMORY;

	/* prepare work area for initialization */
	barrier = mars_ea_work_area_get(barrier_ea,
					MARS_TASK_BARRIER_ALIGN,
					MARS_TASK_BARRIER_SIZE);

	/* initialize barrier instance on work area */
	barrier->mars_context_ea = mars_ptr_to_ea(mars);
	barrier->total = total;
	barrier->notified_count = 0;
	barrier->waited_count = 0;
	barrier->notify_wait_count = 0;
	barrier->wait_count = 0;

	/* update barrier on EA */
	mars_ea_put(barrier_ea, barrier, MARS_TASK_BARRIER_SIZE);
	mars_ea_sync();

	mars_mutex_reset(barrier_ea);

	/* return barrier instance pointer */
	*barrier_ea_ret = barrier_ea;

	return MARS_SUCCESS;
}

int mars_task_barrier_initialize(uint64_t barrier_ea,
								 uint32_t total)
{
	struct mars_task_barrier *barrier;

	/* check function params */
	if (!barrier_ea)
		return MARS_ERROR_NULL;
	if (!total || total > MARS_TASK_BARRIER_WAIT_MAX)
		return MARS_ERROR_PARAMS;

	/* prepare work area */
	barrier = mars_ea_work_area_get(barrier_ea,
					MARS_TASK_BARRIER_ALIGN,
					MARS_TASK_BARRIER_SIZE);

	mars_ea_get(barrier_ea, barrier, MARS_TASK_BARRIER_SIZE);

	/* initialize barrier instance on work area */
	barrier->total = total;
	barrier->notified_count = 0;
	barrier->waited_count = 0;
	barrier->notify_wait_count = 0;
	barrier->wait_count = 0;

	/* update barrier on EA */
	mars_ea_put(barrier_ea, barrier, MARS_TASK_BARRIER_SIZE);
	mars_ea_sync();

	mars_mutex_reset(barrier_ea);

	return MARS_SUCCESS;
}
								 
int mars_task_barrier_destroy(uint64_t barrier_ea)
{
	struct mars_task_barrier *barrier;

	/* check function params */
	if (!barrier_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	barrier = mars_ea_work_area_get(barrier_ea,
					MARS_TASK_BARRIER_ALIGN,
					MARS_TASK_BARRIER_SIZE);

	mars_ea_get(barrier_ea, barrier, MARS_TASK_BARRIER_SIZE);

	/* make sure no tasks in wait list */
	if (barrier->wait_count)
		return MARS_ERROR_STATE;

	mars_ea_free(barrier_ea);

	return MARS_SUCCESS;
}
