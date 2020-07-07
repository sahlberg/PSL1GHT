#include <string.h>

#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>

#include <sys/spu.h>
#include <sys/thread.h>

#include "context_internal.h"
#include "workload_internal_types.h"
#include "kernel_internal_types.h"
#include "numa_internal.h"

#define NUM_OF_SPU 					6

extern const unsigned char mars_kernel_entry[];

/* This function must not be called */
static int numa_mpu_max(void)
{
	return -1;
}

static void mpu_mars_handler_entry(void *arg)
{
	int ret;
	mars_mpu_context_t *mpu = (mars_mpu_context_t*)arg;
	
	for(;;) {
		uint64_t ea;
		sys_event_t event;
		
		ret = sysEventQueueReceive(mpu->spu_ppu_event_queue, &event, 0);
		if(ret != 0)
			sysThreadExit(-1);

		ea = ((uint64_t)(event.data_2&0x00ffffff)<<32)|event.data_3;
		if(ea == MARS_HOST_SIGNAL_EXIT)
			break;
			
		mars_ea_cond_signal(ea, 1);
	}
	sysThreadExit(0);
}


int mars_mpu_max(uint32_t *num)
{
	int mpu_max;

	if (mars_numa_enabled())
		mpu_max = numa_mpu_max();
	else
		mpu_max = NUM_OF_SPU;

	if (mpu_max < 0)
		return MARS_ERROR_INTERNAL;

	*num = mpu_max;

	return MARS_SUCCESS;
}

int mars_mpu_create(mars_mpu_context_t *mpu, uint32_t num_mpus, uint32_t spu_prio, uint32_t ppu_prio)
{
	int ret;
	sysSpuThreadGroupAttribute attr;
	sys_event_queue_attr_t event_queue_attr = { SYS_EVENT_QUEUE_PRIO, SYS_EVENT_QUEUE_PPU, "" };

	sysSpuThreadGroupAttributeInitialize(attr);

	mpu->spu_threads = (sys_spu_thread_t*)mars_malloc(sizeof(sys_spu_thread_t)*num_mpus);
	if(!mpu->spu_threads)
		return MARS_ERROR_MEMORY;
	
	ret = sysSpuImageImport(&mpu->spu_image,mars_kernel_entry,SPU_IMAGE_PROTECT);
	if(ret)
		goto error_kernel_image_import;
		
	ret = sysSpuThreadGroupCreate(&mpu->spu_thread_group, num_mpus, spu_prio,  &attr);
	if(ret)
		goto error_thread_group_create;
	
	ret = sysEventQueueCreate(&mpu->spu_ppu_event_queue, &event_queue_attr, SYS_EVENT_QUEUE_KEY_LOCAL, 127);
	if(ret)
		goto error_event_queue_create;

	ret = sysThreadCreate(&mpu->ppu_handler_thread, mpu_mars_handler_entry, mpu, ppu_prio, 4096, THREAD_JOINABLE, "mpu_mars_handler");
	if(ret)
		goto error_ppu_thread_create;

	return MARS_SUCCESS;

error_ppu_thread_create:
	sysEventQueueDestroy(mpu->spu_ppu_event_queue, 0);
error_event_queue_create:
	sysSpuThreadGroupDestroy(mpu->spu_thread_group);
error_thread_group_create:
	sysSpuImageClose(&mpu->spu_image);
error_kernel_image_import:
	free(mpu->spu_threads);
	return MARS_ERROR_INTERNAL;
}

int mars_mpu_run(mars_mpu_context_t *mpu)
{
	return sysSpuThreadGroupStart(mpu->spu_thread_group);
}

int mars_mpu_initialize(mars_mpu_context_t *mpu, uint64_t params_ea, uint32_t idx)
{
	int ret;
	sysSpuThreadArgument arg;
	sysSpuThreadAttribute attr;
	
	sysSpuThreadArgumentInitialize(arg);
	sysSpuThreadAttributeInitialize(attr);
	
	arg.arg0 = params_ea;
	ret = sysSpuThreadInitialize(&mpu->spu_threads[idx],mpu->spu_thread_group,idx,&mpu->spu_image,&attr,&arg);
	if(ret) {
		return MARS_ERROR_INTERNAL;
	}
	
	ret = sysSpuThreadConnectEvent(mpu->spu_threads[idx],mpu->spu_ppu_event_queue,SPU_THREAD_EVENT_USER,MARS_KERNEL_SPU_EVENT_PORT);
	if(ret) {
		return MARS_ERROR_INTERNAL;
	}	
	sysSpuPrintfAttachThread(mpu->spu_threads[idx]);

	return MARS_SUCCESS;
}

int mars_mpu_wait_all(mars_mpu_context_t *mpu)
{
	int ret, i;
	u64 ret_val;
	u32 status, cause;
	
	for(i=0;i < mpu->num_spu;i++) {
		sysSpuPrintfDetachThread(mpu->spu_threads[i]);
	}
	
	ret = sysThreadJoin(mpu->ppu_handler_thread, &ret_val);
	if(ret || ret_val)
		return MARS_ERROR_INTERNAL;
	
	ret = sysSpuThreadGroupJoin(mpu->spu_thread_group, &cause, &status);
	if(ret)
		return MARS_ERROR_INTERNAL;
		
	ret = sysSpuThreadGroupDestroy(mpu->spu_thread_group);
	if(ret)
		return MARS_ERROR_INTERNAL;
		
	ret = sysSpuImageClose(&mpu->spu_image);
	if(ret)
		return MARS_ERROR_INTERNAL;
	
	free(mpu->spu_threads);
	return MARS_SUCCESS;
}
