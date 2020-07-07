#include <ppu_intrinsics.h>
#include <unistd.h>

#include <sys/thread.h>

#include <mars/base.h>
#include <mars/error.h>
#include <mars/mutex.h>

union mars_mutex_header {
	struct mars_mutex_status status;
	uint32_t bits;
};

static void init_status(struct mars_mutex_status *status)
{
	status->lock = MARS_MUTEX_UNLOCKED;
	status->current_id = 0;
	status->next_id = 0;
}


int mars_mutex_create(uint64_t *mutex_ea_ret)
{
	struct mars_mutex *mutex;
	uint64_t mutex_ea;

	if (!mutex_ea_ret)
		return MARS_ERROR_NULL;

	mutex_ea = mars_ea_memalign(MARS_MUTEX_ALIGN, MARS_MUTEX_SIZE);
	if (!mutex_ea)
		return MARS_ERROR_MEMORY;

	mutex = mars_ea_to_ptr(mutex_ea);

	init_status(&mutex->status);
	__lwsync();

	*mutex_ea_ret = mutex_ea;

	return MARS_SUCCESS;
}

int mars_mutex_destroy(uint64_t mutex_ea)
{
	if (!mutex_ea)
		return MARS_ERROR_NULL;

	mars_ea_free(mutex_ea);

	return MARS_SUCCESS;
}

int mars_mutex_reset(uint64_t mutex_ea)
{
	struct mars_mutex *mutex = mars_ea_to_ptr(mutex_ea);

	if (!mutex_ea)
		return MARS_ERROR_NULL;

	init_status(&mutex->status);
	__lwsync();

	return MARS_SUCCESS;
}

int mars_mutex_lock(uint64_t mutex_ea)
{
	struct mars_mutex *mutex = mars_ea_to_ptr(mutex_ea);
	union mars_mutex_header header;
	uint8_t id;
	int retry;

	if (!mutex_ea)
		return MARS_ERROR_NULL;

	do {
		header.bits = __lwarx(&mutex->status);

		/* get my id */
		id = header.status.next_id++;
		if ((retry = !__stwcx(&mutex->status, header.bits)))
			sysThreadYield(); //sched_yield();
	} while (retry);

	do {
		header.bits = __lwarx(&mutex->status);

		if (header.status.lock == MARS_MUTEX_LOCKED ||
		    header.status.current_id != id) {
			/* wait until mutex is released */
			sysThreadYield(); //sched_yield();
			retry = 1;
		}
		else {
			/* get lock */
			header.status.lock = MARS_MUTEX_LOCKED;
			header.status.current_id++;
			retry = !__stwcx(&mutex->status, header.bits);
		}
	} while (retry);

	__isync();

	return MARS_SUCCESS;
}

int mars_mutex_unlock(uint64_t mutex_ea)
{
	struct mars_mutex *mutex = mars_ea_to_ptr(mutex_ea);
	union mars_mutex_header header;

	if (!mutex_ea)
		return MARS_ERROR_NULL;
	if (mutex->status.lock != MARS_MUTEX_LOCKED)
		return MARS_ERROR_STATE;

	__lwsync();

	do {
		header.bits = __lwarx(&mutex->status);
		header.status.lock = MARS_MUTEX_UNLOCKED;
	} while (!__stwcx(&mutex->status, header.bits));

	return MARS_SUCCESS;
}

int mars_mutex_lock_get(uint64_t mutex_ea, struct mars_mutex *mutex)
{
#ifdef MARS_ENABLE_DISCRETE_SHARED_MEMORY
	int ret;
	ret = mars_mutex_lock(mutex_ea);
	if (ret != MARS_SUCCESS)
		return ret;
	mars_ea_get(mutex_ea, mutex, MARS_MUTEX_SIZE);
	return MARS_SUCCESS;
#else /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
	(void)mutex; /* ignored */
	return mars_mutex_lock(mutex_ea);
#endif /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
}

int mars_mutex_unlock_put(uint64_t mutex_ea, struct mars_mutex *mutex)
{
#ifdef MARS_ENABLE_DISCRETE_SHARED_MEMORY
	int ret;
	mars_ea_put(
		mutex_ea + offsetof(struct mars_mutex, pad),
		mutex->pad, sizeof(mutex->pad));
	ret = mars_mutex_unlock(mutex_ea);
	if (ret == MARS_SUCCESS)
		mutex->status.lock = MARS_MUTEX_UNLOCKED;
	return ret;
#else /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
	(void)mutex; /* ignored */
	return mars_mutex_unlock(mutex_ea);
#endif /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
}
