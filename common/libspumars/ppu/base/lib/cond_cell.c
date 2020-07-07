#include <errno.h>
#include <limits.h>

#include <sys/thread.h>

#include <sys/mutex.h>
#include <sys/cond.h>

#include <mars/base.h>
#include <mars/error.h>

static sys_cond_t eaCond;
static sys_mutex_t eaCondMutex;

static void __mars_ea_cond_init() __attribute__((constructor(105)));
static void __mars_ea_cond_init()
{
	sys_cond_attr_t cattr;
	sys_mutex_attr_t mattr;
	
	sysCondAttrInitialize(cattr);
	sysMutexAttrInitialize(mattr);

	sysMutexCreate(&eaCondMutex, &mattr);
	sysCondCreate(&eaCond, eaCondMutex, &cattr);
}

int mars_ea_cond_wait(uint64_t watch_point_ea,
		      int (*test_cond)(uint32_t , void *),
		      void *test_cond_param)
{
	volatile int ret;
	
	sysMutexLock(eaCondMutex, 0);
	do {
		volatile uint32_t val = (volatile uint32_t)mars_ea_get_uint32(watch_point_ea);
		
		ret = (volatile int)(*test_cond)(val, test_cond_param);
		if (ret >= 0)
			break;
			
		sysCondWait(eaCond, 0);
	} while(ret < 0);
	sysMutexUnlock(eaCondMutex);
	
	return ret;
}

int mars_ea_cond_signal(uint64_t watch_point_ea, int broadcast)
{
	sysMutexLock(eaCondMutex, 0);
	if(broadcast)
		sysCondBroadcast(eaCond);
	else
		sysCondSignal(eaCond);
	sysMutexUnlock(eaCondMutex);
	return MARS_SUCCESS;
}
