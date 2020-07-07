#include <_ansi.h>
#include <_syslist.h>
#include <sys/reent.h>
#include <sys/types.h>

#include <sys/mutex.h>

#include <mars/error.h>

#include "context_internal.h"

sys_mutex_t mars_shared_context_lock;

static void __mars_shared_context_lock_init() __attribute__((constructor(105)));
static void __mars_shared_context_lock_init()
{
	sys_mutex_attr_t attr;
	
	sysMutexAttrInitialize(attr);
	sysMutexCreate(&mars_shared_context_lock, &attr);
}

int mars_host_mutex_lock(mars_host_mutex_t *mutex)
{
	int ret = sysMutexLock(*mutex, 0);
	if(ret)
		return MARS_ERROR_INTERNAL;
		
	return MARS_SUCCESS;
}

int mars_host_mutex_unlock(mars_host_mutex_t *mutex)
{
	int ret = sysMutexUnlock(*mutex);
	if(ret)
		return MARS_ERROR_INTERNAL;
		
	return MARS_SUCCESS;
}
