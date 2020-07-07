#include <mars/error.h>
#include <mars/task_types.h>
#include <mars/task_signal.h>

#include "../module/task_module.h"

int mars_task_signal_send(struct mars_task_id *id)
{
	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	return mars_task_module_signal_send(id->workload_id);
}

int mars_task_signal_wait(void)
{
	struct mars_task_context *task;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea)
		return MARS_ERROR_FORMAT;

	return mars_task_module_signal_wait(mars_task_module_get_heap());
}

int mars_task_signal_try_wait(void)
{
	return mars_task_module_signal_try_wait();
}
