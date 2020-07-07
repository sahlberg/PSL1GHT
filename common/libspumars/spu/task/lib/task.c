#include <stdlib.h>

#include <mars/error.h>
#include <mars/task.h>
//#include <mars/module.h>

#include "../module/task_module.h"

const struct mars_task_module_syscalls *mars_task_module_syscalls;

int main(const struct mars_task_args *args,
	 const struct mars_task_module_syscalls *module_syscalls)
{
	struct mars_task_context *task;
	
	/* save the module syscalls pointer */
	mars_task_module_syscalls = module_syscalls;

	/* get task context */
	task = mars_task_module_get_task();

	/* call task main function and store return value in task context */
	task->exit_code = mars_task_main(args);

	/* exit */
	mars_task_module_exit();

	return MARS_SUCCESS;
}

void mars_task_exit(int32_t exit_code)
{
	struct mars_task_context *task;

	/* get task context */
	task = mars_task_module_get_task();

	/* store exit code in task context */
	task->exit_code = exit_code;
	
	/* exit */
	mars_task_module_exit();
}

int mars_task_yield(void)
{
	struct mars_task_context *task;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea)
		return MARS_ERROR_FORMAT;

	mars_task_module_yield(mars_task_module_get_heap());

	return MARS_SUCCESS;
}

int mars_task_schedule(const struct mars_task_id *id,
		       const struct mars_task_args *args,
		       uint8_t priority)
{
	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	return mars_task_module_schedule(id->workload_id, args, priority);
}

int mars_task_unschedule(const struct mars_task_id *id, int32_t exit_code)
{
	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	return mars_task_module_unschedule(id->workload_id, exit_code);
}

int mars_task_wait(const struct mars_task_id *id, int32_t *exit_code)
{
	struct mars_task_context *task;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea)
		return MARS_ERROR_FORMAT;

	mars_task_module_wait(id->workload_id, mars_task_module_get_heap());

	/* exit code requested so get it from the task context and return it */
	if (exit_code) {
		task = (struct mars_task_context *)
			mars_task_module_get_task_by_id(id);
		if (!task)
			return MARS_ERROR_INTERNAL;

		*exit_code = task->exit_code;
	}

	return MARS_SUCCESS;
}

int mars_task_try_wait(const struct mars_task_id *id, int32_t *exit_code)
{
	struct mars_task_context *task;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;

	mars_task_module_try_wait(id->workload_id);

	/* exit code requested so get it from the task context and return it */
	if (exit_code) {
		task = (struct mars_task_context *)
			mars_task_module_get_task_by_id(id);
		if (!task)
			return MARS_ERROR_INTERNAL;

		*exit_code = task->exit_code;
	}

	return MARS_SUCCESS;
}

int mars_task_call_host(uint64_t callback_ea,
			const struct mars_callback_args *in,
			struct mars_callback_args *out)
{
	struct mars_task_context *task;

	/* get task context */
	task = mars_task_module_get_task();

	/* make sure task context has a context save area */
	if (!task->context_save_area_ea)
		return MARS_ERROR_FORMAT;

	return mars_task_module_call_host(callback_ea, in, out,
					  mars_task_module_get_heap());
}

uint32_t mars_task_get_ticks(void)
{
	return mars_task_module_get_ticks();
}

uint16_t mars_task_get_kernel_id(void)
{
	return mars_task_module_get_kernel_id();
}

const struct mars_task_id *mars_task_get_id(void)
{
	struct mars_task_context *task;

	task = mars_task_module_get_task();

	return &task->id;
}

const char *mars_task_get_name(void)
{
	struct mars_task_context *task;

	task = mars_task_module_get_task();

	return task->id.name[0] ? (const char *)task->id.name : NULL;
}
