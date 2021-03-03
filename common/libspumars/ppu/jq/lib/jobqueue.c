#include <stdio.h>
#include <string.h>
#include <sys/spu.h>

#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/workload_queue.h>

#include "mars/jq.h"

#include "elf.h"
#include "jq_internal_types.h"

extern const unsigned char mars_jq_module_entry[];

int mars_jq_create(struct mars_context *mars,struct mars_jq_id *id_ret,const char *name)
{
	int ret;
	uint16_t workload_id;
	uint64_t workload_ea;
	struct mars_jq_context *jq;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!id_ret)
		return MARS_ERROR_NULL;
	if (name && strlen(name) > MARS_JQ_NAME_LEN_MAX)
		return MARS_ERROR_PARAMS;

	/* begin process to add the task to the workload queue */
	ret = mars_workload_queue_add_begin(mars, &workload_id, &workload_ea,
					    mars_jq_module_entry,
					    MARS_JQ_MODULE_NAME);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	jq = mars_ea_work_area_get(workload_ea,
				     MARS_JQ_CONTEXT_ALIGN,
				     MARS_JQ_CONTEXT_SIZE);

	/* initialize task id */
	jq->id.mars_context_ea = mars_ptr_to_ea(mars);
	jq->id.workload_id = workload_id;
	if (name)
		strcpy((char *)jq->id.name, name);
	else
		jq->id.name[0] = 0;

	/* update task context on EA */
	mars_ea_put(workload_ea, jq, MARS_JQ_CONTEXT_SIZE);
	mars_ea_sync();

	/* end process to add the task to the workload queue */
	ret = mars_workload_queue_add_end(mars, workload_id, 0);
	if (ret != MARS_SUCCESS) {
		mars_workload_queue_add_end(mars, workload_id, 1);
		return ret;
	}

	/* return id to caller */
	*id_ret = jq->id;

	return MARS_SUCCESS;
}

int mars_jq_wait(struct mars_jq_id *id)
{
	int ret;
	struct mars_context *mars;
	uint64_t workload_ea;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;
	if (!id->mars_context_ea)
		return MARS_ERROR_PARAMS;

	/* get mars context pointer from task id */
	mars = mars_ea_to_ptr(id->mars_context_ea);

	/* blocking wait for workload completion */
	ret = mars_workload_queue_wait(mars, id->workload_id, &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	return MARS_SUCCESS;
}

int mars_jq_run(struct mars_jq_id *id)
{
	int ret;
	struct mars_context *mars;
	struct mars_jq_context *jq;
	uint64_t workload_ea;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;
	if (!id->mars_context_ea)
		return MARS_ERROR_PARAMS;

	/* get mars context pointer from task id */
	mars = mars_ea_to_ptr(id->mars_context_ea);

	/* begin process to schedule the workload in the workload queue */
	ret = mars_workload_queue_schedule_begin(mars, id->workload_id,
						 1, &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	jq = mars_ea_work_area_get(workload_ea,
				     MARS_JQ_CONTEXT_ALIGN,
				     MARS_JQ_CONTEXT_SIZE);

	/* get task context from EA */
	mars_ea_get(workload_ea, jq, MARS_JQ_CONTEXT_SIZE);

	/* end process to schedule the workload in the workload queue */
	return mars_workload_queue_schedule_end(mars, id->workload_id, 0);
}