#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/workload_queue.h>

#include "mars/task_signal.h"

int mars_task_signal_send(struct mars_task_id *id)
{
	int ret;
	struct mars_context *mars;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	/* get mars context pointer */
	mars = mars_ea_to_ptr(id->mars_context_ea);
	if (!mars)
		return MARS_ERROR_PARAMS;

	/* send signal to workload */
	ret = mars_workload_queue_signal_send(mars, id->workload_id);
	if (ret != MARS_SUCCESS)
		return ret;

	return MARS_SUCCESS;
}
