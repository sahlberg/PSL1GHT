#include <stdio.h>
#include <string.h>
#include <ppu_intrinsics.h>

#include <sys/spu.h>
#include <sys/thread.h>

#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/mutex.h>
#include <mars/workload_queue.h>

#include "callback_internal_types.h"
#include "kernel_internal_types.h"
#include "workload_internal_types.h"

#include "context_internal.h"

uint32_t mars_get_ticks(void)
{
	return __mftb() & 0xffffffff;
}

static void kernel_ticks_sync(uint64_t kernel_ticks_ea)
{
	uint64_t flag_ea =
		kernel_ticks_ea + offsetof(struct mars_kernel_ticks, flag);
	uint64_t offset_ea =
		kernel_ticks_ea + offsetof(struct mars_kernel_ticks, offset);

	/* wait until kernel sets the sync begin flag */
	do {
	} while (mars_ea_get_uint32(flag_ea) !=
		 MARS_KERNEL_TICKS_FLAG_SYNC_BEGIN);

	mars_ea_sync();

	mars_ea_put_uint32(offset_ea, mars_get_ticks());

	mars_ea_sync();

	/* set the sync end flag so kernel can finish sync */
	mars_ea_put_uint32(flag_ea, MARS_KERNEL_TICKS_FLAG_SYNC_END);
}

static int kernel_params_init(struct mars_context *mars, uint64_t params_ea,
			      uint16_t kernel_id)
{
	struct mars_kernel_params *params =
		mars_ea_work_area_get(params_ea,
				      MARS_KERNEL_PARAMS_ALIGN,
				      MARS_KERNEL_PARAMS_SIZE);

	if (!params)
		return MARS_ERROR_MEMORY;

	/* zero kernel params */
	memset(params, 0, MARS_KERNEL_PARAMS_SIZE);

	params->kernel_id = kernel_id;
	params->mars_context_ea = mars_ptr_to_ea(mars);
	params->workload_queue_ea = mars->workload_queue_ea;
	params->callback_queue_ea = mars->callback_queue_ea;

	/* update params on EA */
	mars_ea_put(params_ea, params, MARS_KERNEL_PARAMS_SIZE);
	mars_ea_sync();

	return MARS_SUCCESS;
}

static int mpu_context_create(struct mars_context *mars, uint32_t num_mpus, uint32_t spu_prio, uint32_t ppu_prio)
{
	int ret;
	uint16_t i;
	mars_mpu_context_t *mpu = mars->mpu_context;
		
	mpu->num_spu = num_mpus;
	mpu->spu_prio = spu_prio;
	mpu->ppu_prio = ppu_prio;

	ret = mars_mpu_create(mpu, num_mpus, spu_prio, ppu_prio);	
	if(ret != MARS_SUCCESS)
		return ret;

	/* create threads for mpu context */
	for (i = 0; i < num_mpus; i++) {
		uint64_t params_ea = mars->kernel_params_ea +
				     MARS_KERNEL_PARAMS_SIZE * i;

		/* initialize kernel params for current mpu context */
		ret = kernel_params_init(mars, params_ea, i);
		if (ret != MARS_SUCCESS)
			return ret;

		/* run current mpu context */
		ret = mars_mpu_initialize(mpu, params_ea, i);
		if (ret != MARS_SUCCESS)
			return ret;

		/* increment mars context mpu context count */
		mars->mpu_context_count++;
	}
	mars_mpu_run(mpu);
	
	for(i = 0; i < num_mpus; i++) {
		uint64_t params_ea = mars->kernel_params_ea +
				     MARS_KERNEL_PARAMS_SIZE * i;
		/* sync kernel ticks for current mpu context */
		kernel_ticks_sync(params_ea +
			offsetof(struct mars_kernel_params, kernel_ticks));
	}
	
	return MARS_SUCCESS;
}

static int mpu_context_destroy(struct mars_context *mars)
{
	int ret;

	/* wait for mpu context */
	ret = mars_mpu_wait_all(mars->mpu_context);
	if (ret != MARS_SUCCESS)
		return ret;

	return MARS_SUCCESS;
}

static int system_sanity_check(void)
{
	if ((MARS_CALLBACK_ARGS_SIZE !=
		sizeof(struct mars_callback_args)) ||
	    (MARS_CALLBACK_QUEUE_SIZE !=
		sizeof(struct mars_callback_queue)) ||
	    (MARS_KERNEL_PARAMS_SIZE !=
		sizeof(struct mars_kernel_params)) ||
	    (MARS_MUTEX_SIZE !=
		sizeof(struct mars_mutex)) ||
	    (MARS_WORKLOAD_CALLBACK_SIZE !=
		sizeof(struct mars_workload_callback)) ||
	    (MARS_WORKLOAD_CONTEXT_SIZE !=
		sizeof(struct mars_workload_context)) ||
	    (MARS_WORKLOAD_MODULE_SIZE !=
		sizeof(struct mars_workload_module)) ||
	    (MARS_WORKLOAD_QUEUE_SIZE !=
		sizeof(struct mars_workload_queue)) ||
	    (MARS_WORKLOAD_QUEUE_HEADER_SIZE !=
		sizeof(struct mars_workload_queue_header)) ||
	    (MARS_WORKLOAD_QUEUE_BLOCK_SIZE !=
		sizeof(struct mars_workload_queue_block)))
		return MARS_ERROR_INTERNAL;

	return MARS_SUCCESS;
}

int mars_context_create(struct mars_context **mars_ret, uint32_t num_mpus, uint32_t spu_prio, uint32_t ppu_prio)
{
	int ret;
	uint32_t num_mpus_max;
	struct mars_context *mars = NULL;

	/* check function params */
	if (!mars_ret)
		return MARS_ERROR_NULL;

	/* check number of mpus to use */
	ret = mars_mpu_max(&num_mpus_max);
	if (ret != MARS_SUCCESS)
		return ret;
	if (!num_mpus_max)
		return MARS_ERROR_LIMIT;
	if (num_mpus > num_mpus_max)
		return MARS_ERROR_PARAMS;
	if (!num_mpus)
		num_mpus = num_mpus_max;
	
	/* system sanity check */
	ret = system_sanity_check();
	if (ret != MARS_SUCCESS)
		return ret;

	/* lock mutex */
	ret = mars_host_mutex_lock(&mars_shared_context_lock);
	if (ret != MARS_SUCCESS)
		return ret;

	/* allocate context */
	mars = mars_malloc(sizeof(struct mars_context));
	if (!mars) {
		ret = MARS_ERROR_MEMORY;
		goto error;
	}

	/* zero context */
	memset(mars, 0, sizeof(struct mars_context));

	/* increment reference count */
	mars->reference_count++;

	/* allocate kernel params */
	mars->kernel_params_ea = mars_ea_memalign(
			MARS_KERNEL_PARAMS_ALIGN,
			MARS_KERNEL_PARAMS_SIZE * num_mpus_max);
	if (!mars->kernel_params_ea) {
		ret = MARS_ERROR_MEMORY;
		goto error_malloc_kernel_params;
	}

	/* allocate mpu context thread array */
	mars->mpu_context = (mars_mpu_context_t *)mars_malloc(sizeof(mars_mpu_context_t));
	if (!mars->mpu_context) {
		ret = MARS_ERROR_MEMORY;
		goto error_malloc_mpu_context;
	}

	/* create workload queue */
	ret = mars_workload_queue_create(mars);
	if (ret != MARS_SUCCESS)
		goto error_workload_queue_create;

	/* create callback queue */
	ret = mars_callback_queue_create(mars, ppu_prio);
	if (ret != MARS_SUCCESS)
		goto error_callback_queue_create;

	/* create mpu contexts */
	ret = mpu_context_create(mars, num_mpus, spu_prio, ppu_prio);
	if (ret != MARS_SUCCESS)
		goto error_mpu_context_create;

	/* return mars context pointer */
	*mars_ret = mars;

	/* unlock mutex */
	ret = mars_host_mutex_unlock(&mars_shared_context_lock);
	if (ret != MARS_SUCCESS)
		goto error_shared_context_unlock;

	return MARS_SUCCESS;

error_shared_context_unlock:
	mpu_context_destroy(mars);
error_mpu_context_create:
	mars_callback_queue_exit(mars);
	mars_callback_queue_destroy(mars);
error_callback_queue_create:
	mars_workload_queue_exit(mars);
	mars_workload_queue_destroy(mars);
error_workload_queue_create:
	mars_free(mars->mpu_context);
error_malloc_mpu_context:
	mars_ea_free(mars->kernel_params_ea);
error_malloc_kernel_params:
	mars_free(mars);
error:
	mars_host_mutex_unlock(&mars_shared_context_lock);

	return ret;
}

int mars_context_destroy(struct mars_context *mars)
{
	int ret;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;

	/* lock mutex */
	ret = mars_host_mutex_lock(&mars_shared_context_lock);
	if (ret != MARS_SUCCESS)
		return ret;

	/* decrement reference count */
	mars->reference_count--;

	/* reference count is not 0 so return */
	if (mars->reference_count)
		goto done;

	/* shutdown the workload queue so mpu context threads exit */
	ret = mars_workload_queue_exit(mars);
	if (ret != MARS_SUCCESS)
		return ret;

	/* shutdown the callback queue so callback handler threads exit */
	ret = mars_callback_queue_exit(mars);
	if (ret != MARS_SUCCESS)
		return ret;

	/* destroy mpu contexts */
	if (mars->mpu_context_count) {
		ret = mpu_context_destroy(mars);
		if (ret != MARS_SUCCESS)
			goto error;
	}

	/* destroy callback queue */
	if (mars->callback_queue_ea) {
		ret = mars_callback_queue_destroy(mars);
		if (ret != MARS_SUCCESS)
			goto error;
	}

	/* destroy workload queue */
	if (mars->workload_queue_ea) {
		ret = mars_workload_queue_destroy(mars);
		if (ret != MARS_SUCCESS)
			goto error;
	}

	/* free allocated memory */
	mars_free(mars->mpu_context);
	mars_ea_free(mars->kernel_params_ea);
	mars_free(mars);

done:
	/* unlock mutex */
	ret = mars_host_mutex_unlock(&mars_shared_context_lock);
	if (ret != MARS_SUCCESS)
		return ret;

	return MARS_SUCCESS;

error:
	mars_host_mutex_unlock(&mars_shared_context_lock);

	return ret;
}

uint32_t mars_context_get_num_mpus(struct mars_context *mars)
{
	/* check function params */
	if (!mars)
		return 0;

	return mars->mpu_context_count;
}
