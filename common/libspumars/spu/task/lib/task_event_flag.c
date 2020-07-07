#include <stddef.h>
#include <string.h>

#include <mars/error.h>
#include <mars/task_event_flag.h>
#include <mars/task_types.h>

#include "task_event_flag_internal_types.h"
#include "task_internal_types.h"
#include "../module/task_module.h"

static struct mars_task_event_flag event_flag;

int mars_task_event_flag_clear(uint64_t event_flag_ea, uint32_t bits)
{
	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;
	if (event_flag_ea & MARS_TASK_EVENT_FLAG_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	mars_mutex_lock_get(event_flag_ea, (struct mars_mutex *)&event_flag);

	/* clear the necessary bits */
	event_flag.bits &= ~bits;

	mars_mutex_unlock_put(event_flag_ea, (struct mars_mutex *)&event_flag);

	return MARS_SUCCESS;
}

int mars_task_event_flag_set(uint64_t event_flag_ea, uint32_t bits)
{
	int i;

	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;
	if (event_flag_ea & MARS_TASK_EVENT_FLAG_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	mars_mutex_lock_get(event_flag_ea, (struct mars_mutex *)&event_flag);

	/* check for valid direction */
	if (event_flag.direction != MARS_TASK_EVENT_FLAG_MPU_TO_HOST &&
	    event_flag.direction != MARS_TASK_EVENT_FLAG_MPU_TO_MPU) {
		mars_mutex_unlock_put(event_flag_ea,
				      (struct mars_mutex *)&event_flag);
		return MARS_ERROR_STATE;
	}

	/* set the necessary bits */
	event_flag.bits |= bits;

	/* save current set bits */
	bits = event_flag.bits;

	/* search through wait list for tasks to be signalled */
	for (i = 0; i < event_flag.wait_count; i++) {
		/* check condition based on wait mode */
		switch (event_flag.wait_mask_mode[i]) {
		case MARS_TASK_EVENT_FLAG_MASK_OR:
			if ((bits & event_flag.wait_mask[i]) == 0)
				continue;
			break;
		case MARS_TASK_EVENT_FLAG_MASK_AND:
			if ((bits & event_flag.wait_mask[i])
				!= event_flag.wait_mask[i])
				continue;
			break;
		}

		/* signal the waiting tasks */
		mars_task_module_signal_send(event_flag.wait_id[i]);

		/* flush id from wait list */
		event_flag.wait_count--;
		memmove(&event_flag.wait_id[i],
			&event_flag.wait_id[i + 1],
			sizeof(uint16_t) * (event_flag.wait_count - i));
		memmove(&event_flag.wait_mask[i],
			&event_flag.wait_mask[i + 1],
			sizeof(uint32_t) * (event_flag.wait_count - i));
		memmove(&event_flag.wait_mask_mode[i],
			&event_flag.wait_mask_mode[i + 1],
			sizeof(uint8_t) * (event_flag.wait_count - i));
		i--;
	}

	mars_mutex_unlock_put(event_flag_ea, (struct mars_mutex *)&event_flag);

	/* signal the waiting host */
	if (event_flag.direction != MARS_TASK_EVENT_FLAG_HOST_TO_MPU &&
	    event_flag.direction != MARS_TASK_EVENT_FLAG_MPU_TO_MPU)
		mars_task_module_signal_host(event_flag_ea +
			offsetof(struct mars_task_event_flag, bits));

	return MARS_SUCCESS;
}

static int wait(uint64_t event_flag_ea,uint32_t mask, uint8_t mask_mode,
		uint32_t *bits, int try)
{
	int wait = 0;
	struct mars_task_context *task;

	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;
	if (event_flag_ea & MARS_TASK_EVENT_FLAG_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (mask_mode != MARS_TASK_EVENT_FLAG_MASK_OR &&
		mask_mode != MARS_TASK_EVENT_FLAG_MASK_AND)
		return MARS_ERROR_PARAMS;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea && !try)
		return MARS_ERROR_FORMAT;

	mars_mutex_lock_get(event_flag_ea, (struct mars_mutex *)&event_flag);

	/* check for valid direction */
	if (event_flag.direction != MARS_TASK_EVENT_FLAG_HOST_TO_MPU &&
	    event_flag.direction != MARS_TASK_EVENT_FLAG_MPU_TO_MPU) {
		mars_mutex_unlock_put(event_flag_ea,
				      (struct mars_mutex *)&event_flag);
		return MARS_ERROR_STATE;
	}

	/* check condition based on wait mode */
	switch (mask_mode) {
	case MARS_TASK_EVENT_FLAG_MASK_OR:
		if ((event_flag.bits & mask) == 0)
			wait = 1;
		break;
	case MARS_TASK_EVENT_FLAG_MASK_AND:
		if ((event_flag.bits & mask) != mask)
			wait = 1;
		break;
	}

	if (wait) {
		/* only try so return busy */
		if (try) {
			/* get current bits status if return bits requested */
			if (bits)
				*bits = event_flag.bits;

			mars_mutex_unlock_put(event_flag_ea,
					      (struct mars_mutex *)&event_flag);

			return MARS_ERROR_BUSY;
		}

		/* check if event flag wait limit reached */
		if (event_flag.wait_count == MARS_TASK_EVENT_FLAG_WAIT_MAX) {
			mars_mutex_unlock_put(event_flag_ea,
					      (struct mars_mutex *)&event_flag);
			return MARS_ERROR_LIMIT;
		}

		/* add id to wait list */
		event_flag.wait_id[event_flag.wait_count] =
			task->id.workload_id;
		event_flag.wait_mask[event_flag.wait_count] = mask;
		event_flag.wait_mask_mode[event_flag.wait_count] = mask_mode;
		event_flag.wait_count++;

		mars_mutex_unlock_put(event_flag_ea,
				      (struct mars_mutex *)&event_flag);

		/* wait for signal */
		mars_task_module_signal_wait(mars_task_module_get_heap());

		mars_mutex_lock_get(event_flag_ea,
				    (struct mars_mutex *)&event_flag);
	}

	/* return bits if requested */
	if (bits)
		*bits = event_flag.bits;

	/* clear event if clear mode is auto */
	if (event_flag.clear_mode == MARS_TASK_EVENT_FLAG_CLEAR_AUTO)
		event_flag.bits &= ~mask;

	mars_mutex_unlock_put(event_flag_ea, (struct mars_mutex *)&event_flag);

	return MARS_SUCCESS;
}

int mars_task_event_flag_wait(uint64_t event_flag_ea,
			      uint32_t mask, uint8_t mask_mode,
			      uint32_t *bits)
{
	return wait(event_flag_ea, mask, mask_mode, bits, 0);
}

int mars_task_event_flag_try_wait(uint64_t event_flag_ea,
				  uint32_t mask, uint8_t mask_mode,
				  uint32_t *bits)
{
	return wait(event_flag_ea, mask, mask_mode, bits, 1);
}
