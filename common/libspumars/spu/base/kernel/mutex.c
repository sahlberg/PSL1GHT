#include <stdint.h>
#include <spu_mfcio.h>

#include <mars/error.h>
#include <mars/mutex_types.h>

#include "kernel_internal_types.h"

#define MARS_MUTEX_STATE_NONE			0
#define MARS_MUTEX_STATE_DONE			2

static struct mars_mutex mutex_buffer;

int mutex_lock_get(uint64_t mutex_ea, struct mars_mutex *mutex)
{
	int state = MARS_MUTEX_STATE_NONE;
	uint8_t id = 0;

	/* check function params */
	if (!mutex_ea)
		return MARS_ERROR_NULL;
	if (!mutex)
		return MARS_ERROR_NULL;
	if (mutex_ea & MARS_MUTEX_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if ((uintptr_t)mutex & MARS_MUTEX_ALIGN_MASK)
		return MARS_ERROR_ALIGN;

	/* set event mask for the lost event */
	spu_write_event_mask(MFC_LLR_LOST_EVENT);

	/* update waiting state */
	while (state != MARS_MUTEX_STATE_DONE) {
		mfc_getllar(mutex, mutex_ea, 0, 0);
		mfc_read_atomic_status();

		if (state &&
		    (mutex->status.lock == MARS_MUTEX_LOCKED ||
		     mutex->status.current_id != id)) {
			/* wait until mutex is released */
			spu_read_event_status();
			spu_write_event_ack(MFC_LLR_LOST_EVENT);
		}
		else {
			if (state) {
				/* get lock */
				mutex->status.lock = MARS_MUTEX_LOCKED;
				mutex->status.current_id++;
			}
			else {
				/* get my id */
				id = mutex->status.next_id++;
			}

			spu_dsync();
			mfc_putllc(mutex, mutex_ea, 0, 0);
			if (!(mfc_read_atomic_status() & MFC_PUTLLC_STATUS))
				state++;
		}
	}

	/* clear any remnant lost event */
	spu_write_event_ack(MFC_LLR_LOST_EVENT);

	return MARS_SUCCESS;
}

int mutex_unlock_put(uint64_t mutex_ea, struct mars_mutex *mutex)
{
	int status;

	/* check function params */
	if (!mutex_ea)
		return MARS_ERROR_NULL;
	if (!mutex)
		return MARS_ERROR_NULL;
	if (mutex_ea & MARS_MUTEX_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if ((uintptr_t)mutex & MARS_MUTEX_ALIGN_MASK)
		return MARS_ERROR_ALIGN;
	if (mutex->status.lock != MARS_MUTEX_LOCKED)
		return MARS_ERROR_STATE;

	mfc_sync(0);

	/* set event mask for the lost event */
	spu_write_event_mask(MFC_LLR_LOST_EVENT);

	do {
		mfc_getllar(&mutex_buffer, mutex_ea, 0, 0);
		mfc_read_atomic_status();

		mutex->status = mutex_buffer.status;
		mutex->status.lock = MARS_MUTEX_UNLOCKED;

		spu_dsync();
		mfc_putllc(mutex, mutex_ea, 0, 0);
		status = mfc_read_atomic_status() & MFC_PUTLLC_STATUS;
	} while (status);

	/* clear any remnant lost event */
	spu_write_event_ack(MFC_LLR_LOST_EVENT);

	return MARS_SUCCESS;
}
