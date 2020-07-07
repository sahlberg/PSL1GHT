#include <string.h>

#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/mutex.h>
#include <mars/workload_queue.h>

#include "mars/task_event_flag.h"

#include "task_event_flag_internal_types.h"


static inline uint64_t event_flag_bits_ea(uint64_t event_flag_ea)
{
	return event_flag_ea +
	       offsetof(struct mars_task_event_flag, bits);
}

int mars_task_event_flag_create(struct mars_context *mars,
				uint64_t *event_flag_ea_ret,
				uint8_t direction,
				uint8_t clear_mode)
{
	struct mars_task_event_flag *event_flag;
	uint64_t event_flag_ea;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!event_flag_ea_ret)
		return MARS_ERROR_NULL;
	if (direction != MARS_TASK_EVENT_FLAG_HOST_TO_MPU &&
		direction != MARS_TASK_EVENT_FLAG_MPU_TO_HOST &&
		direction != MARS_TASK_EVENT_FLAG_MPU_TO_MPU)
		return MARS_ERROR_PARAMS;
	if (clear_mode != MARS_TASK_EVENT_FLAG_CLEAR_AUTO &&
		clear_mode != MARS_TASK_EVENT_FLAG_CLEAR_MANUAL)
		return MARS_ERROR_PARAMS;

	/* allocate event flag instance */
	event_flag_ea = mars_ea_memalign(MARS_TASK_EVENT_FLAG_ALIGN,
					 MARS_TASK_EVENT_FLAG_SIZE);
	if (!event_flag_ea)
		return MARS_ERROR_MEMORY;

	/* prepare work area for initialization */
	event_flag = mars_ea_work_area_get(event_flag_ea,
					   MARS_TASK_EVENT_FLAG_ALIGN,
					   MARS_TASK_EVENT_FLAG_SIZE);

	/* intialize event flag instance on work area */
	event_flag->mars_context_ea = mars_ptr_to_ea(mars);
	event_flag->bits = 0;
	event_flag->direction = direction;
	event_flag->clear_mode = clear_mode;
	event_flag->wait_count = 0;

	/* update event flag on EA */
	mars_ea_put(event_flag_ea, event_flag, MARS_TASK_EVENT_FLAG_SIZE);
	mars_ea_sync();

	mars_mutex_reset(event_flag_ea);

	/* return event flag instance pointer */
	*event_flag_ea_ret = event_flag_ea;

	return MARS_SUCCESS;
}

int mars_task_event_flag_destroy(uint64_t event_flag_ea)
{
	struct mars_task_event_flag *event_flag;

	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	event_flag = mars_ea_work_area_get(event_flag_ea,
					   MARS_TASK_EVENT_FLAG_ALIGN,
					   MARS_TASK_EVENT_FLAG_SIZE);

	mars_ea_get(event_flag_ea, event_flag, MARS_TASK_EVENT_FLAG_SIZE);

	/* make sure no tasks in wait list */
	if (event_flag->wait_count)
		return MARS_ERROR_STATE;

	mars_ea_free(event_flag_ea);

	return MARS_SUCCESS;
}

int mars_task_event_flag_clear(uint64_t event_flag_ea, uint32_t bits)
{
	uint32_t new_bits;
	uint64_t bits_ea;

	if (!event_flag_ea)
		return MARS_ERROR_NULL;

	bits_ea = event_flag_bits_ea(event_flag_ea);

	mars_mutex_lock(event_flag_ea);

	/* clear the necessary bits */
	new_bits = mars_ea_get_uint32(bits_ea) & ~bits;
	mars_ea_put_uint32(bits_ea, new_bits);

	mars_mutex_unlock(event_flag_ea);

	return MARS_SUCCESS;
}

int mars_task_event_flag_set(uint64_t event_flag_ea, uint32_t bits)
{
	int ret;
	int i;
	struct mars_context *mars;
	struct mars_task_event_flag *event_flag;

	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	event_flag = mars_ea_work_area_get(event_flag_ea,
					   MARS_TASK_EVENT_FLAG_ALIGN,
					   MARS_TASK_EVENT_FLAG_SIZE);

	/* get event flag from EA */
	mars_mutex_lock_get(event_flag_ea, (struct mars_mutex *)event_flag);

	/* check event flag status */
	ret = MARS_ERROR_NULL;
	if (!event_flag->mars_context_ea)
		goto end;
	ret = MARS_ERROR_STATE;
	if (event_flag->direction != MARS_TASK_EVENT_FLAG_HOST_TO_MPU)
		goto end;

	/* get mars context pointer */
	mars = mars_ea_to_ptr(event_flag->mars_context_ea);

	/* set the necessary bits */
	event_flag->bits |= bits;

	/* save current set bits */
	bits = event_flag->bits;

	/* search through wait list for tasks to be signalled */
	for (i = 0; i < event_flag->wait_count; i++) {
		/* check condition based on wait mode */
		switch (event_flag->wait_mask_mode[i]) {
		case MARS_TASK_EVENT_FLAG_MASK_OR:
			if ((bits & event_flag->wait_mask[i]) == 0)
				continue;
			break;
		case MARS_TASK_EVENT_FLAG_MASK_AND:
			if ((bits & event_flag->wait_mask[i]) !=
			    event_flag->wait_mask[i])
				continue;
			break;
		}

		/* signal the task to go to ready state */
		ret = mars_workload_queue_signal_send(mars,
						      event_flag->wait_id[i]);
		if (ret != MARS_SUCCESS)
			goto end;

		/* flush id from wait list */
		event_flag->wait_count--;
		memmove(&event_flag->wait_id[i],
			&event_flag->wait_id[i + 1],
			sizeof(uint16_t) * (event_flag->wait_count - i));
		memmove(&event_flag->wait_mask[i],
			&event_flag->wait_mask[i + 1],
			sizeof(uint32_t) * (event_flag->wait_count - i));
		memmove(&event_flag->wait_mask_mode[i],
			&event_flag->wait_mask_mode[i + 1],
			sizeof(uint8_t) * (event_flag->wait_count - i));
		i--;
	}

	ret = MARS_SUCCESS;

end:
	mars_mutex_unlock_put(event_flag_ea, (struct mars_mutex *)event_flag);

	return ret;
}

static int test_any_bits(uint32_t bits, void *param)
{
	const uint32_t *mask = (const uint32_t *)param;

	return (bits & *mask) ? MARS_SUCCESS : -1;
}

static int test_all_bits(uint32_t bits, void *param)
{
	const uint32_t *mask = (const uint32_t *)param;

	return ((bits & *mask) == *mask) ? MARS_SUCCESS : -1;
}

static int wait(uint64_t event_flag_ea,
		uint32_t mask, uint8_t mask_mode, uint32_t *bits, int try)
{
	int ret;
	struct mars_task_event_flag *event_flag;
	uint64_t bits_ea;

	/* check function params */
	if (!event_flag_ea)
		return MARS_ERROR_NULL;

	/* prepare work area */
	event_flag = mars_ea_work_area_get(event_flag_ea,
					   MARS_TASK_EVENT_FLAG_ALIGN,
					   MARS_TASK_EVENT_FLAG_SIZE);

	/* get event flag from EA */
	mars_mutex_lock_get(event_flag_ea, (struct mars_mutex *)event_flag);

	/* check function params */
	ret = MARS_ERROR_STATE;
	if (event_flag->direction != MARS_TASK_EVENT_FLAG_MPU_TO_HOST)
		goto end;
	ret = MARS_ERROR_PARAMS;
	if (mask_mode != MARS_TASK_EVENT_FLAG_MASK_OR &&
		mask_mode != MARS_TASK_EVENT_FLAG_MASK_AND)
		goto end;

	/* check condition based on wait mode */
	bits_ea = event_flag_bits_ea(event_flag_ea);
	switch (mask_mode) {
	case MARS_TASK_EVENT_FLAG_MASK_OR:
		while ((event_flag->bits & mask) == 0) {
			/* get current bits status if return bits requested */
			if (bits)
				*bits = event_flag->bits;

			mars_mutex_unlock_put(event_flag_ea,
					      (struct mars_mutex *)event_flag);

			/* only try so return busy */
			if (try)
				return MARS_ERROR_BUSY;

			/* wait until condition is met */
			ret = mars_ea_cond_wait(bits_ea, test_any_bits, &mask);
			if (ret)
				return ret;

			mars_mutex_lock_get(event_flag_ea,
					    (struct mars_mutex *)event_flag);
		}
		break;
	case MARS_TASK_EVENT_FLAG_MASK_AND:
		while ((event_flag->bits & mask) != mask) {
			/* get current bits status if return bits requested */
			if (bits)
				*bits = event_flag->bits;

			mars_mutex_unlock_put(event_flag_ea,
					      (struct mars_mutex *)event_flag);

			/* only try so return busy */
			if (try)
				return MARS_ERROR_BUSY;

			/* wait until condition is met */
			ret = mars_ea_cond_wait(bits_ea, test_all_bits, &mask);
			if (ret)
				return ret;

			mars_mutex_lock_get(event_flag_ea,
					    (struct mars_mutex *)event_flag);
		}
		break;
	}

	/* return bits if requested */
	if (bits)
		*bits = event_flag->bits;

	/* clear event if clear mode is auto */
	if (event_flag->clear_mode == MARS_TASK_EVENT_FLAG_CLEAR_AUTO)
		event_flag->bits &= ~mask;

	ret = MARS_SUCCESS;

end:
	mars_mutex_unlock_put(event_flag_ea, (struct mars_mutex *)event_flag);

	return ret;
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
