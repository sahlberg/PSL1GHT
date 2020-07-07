#include <mars/base.h>
#include <mars/error.h>
#include <mars/mutex.h>

#include "mars/task_semaphore.h"

#include "task_semaphore_internal_types.h"

int mars_task_semaphore_create(struct mars_context *mars,
			       uint64_t *semaphore_ea_ret,
			       int32_t count)
{
	struct mars_task_semaphore *semaphore;
	uint64_t semaphore_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!semaphore_ea_ret)
		return MARS_ERROR_NULL;

	/* allocate semaphore instance */
	semaphore_ea = mars_ea_memalign(MARS_TASK_SEMAPHORE_ALIGN,
					MARS_TASK_SEMAPHORE_SIZE);
	if (!semaphore_ea)
		return MARS_ERROR_MEMORY;

	/* prepare work area for initialization */
	semaphore = mars_ea_work_area_get(semaphore_ea,
					  MARS_TASK_SEMAPHORE_ALIGN,
					  MARS_TASK_SEMAPHORE_SIZE);

	/* initialize semaphore instance */
	semaphore->mars_context_ea = mars_ptr_to_ea(mars);
	semaphore->count = count;
	semaphore->wait_count = 0;
	semaphore->wait_head = 0;

	/* update semaphore on EA */
	mars_ea_put(semaphore_ea, semaphore, MARS_TASK_SEMAPHORE_SIZE);
	mars_ea_sync();

	mars_mutex_reset(semaphore_ea);

	/* return semaphore instance */
	*semaphore_ea_ret = semaphore_ea;

	return MARS_SUCCESS;
}

int mars_task_semaphore_destroy(uint64_t semaphore_ea)
{
	struct mars_task_semaphore *semaphore;

	/* check function params */
	if (!semaphore_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	semaphore = mars_ea_work_area_get(semaphore_ea,
					  MARS_TASK_SEMAPHORE_ALIGN,
					  MARS_TASK_SEMAPHORE_SIZE);

	mars_ea_get(semaphore_ea, semaphore, MARS_TASK_SEMAPHORE_SIZE);

	/* make sure no tasks in wait list */
	if (semaphore->wait_count)
		return MARS_ERROR_STATE;

	mars_ea_free(semaphore_ea);

	return MARS_SUCCESS;
}
