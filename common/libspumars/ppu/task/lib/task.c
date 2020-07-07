#include <stdio.h>
#include <string.h>
#include <sys/spu.h>

#include <mars/base.h>
#include <mars/context.h>
#include <mars/error.h>
#include <mars/workload_queue.h>

#include "mars/task.h"

#include "elf.h"
#include "task_internal_types.h"

extern const unsigned char mars_task_module_entry[];

static uint64_t task_exit_code_ea(uint64_t task_ea)
{
	return task_ea + offsetof(struct mars_task_context, exit_code);
}

static int task_map_elf(struct mars_task_context *task, const void *elf_image)
{
	int ret, i;
	int text_found = 0;
	int data_found = 0;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;

	/* process elf header information */
	ehdr = (Elf32_Ehdr *)elf_image;
	phdr = (Elf32_Phdr *)((void *)ehdr + ehdr->e_phoff);

	/* elf is not executable */
	if (ehdr->e_type != ET_EXEC)
		return MARS_ERROR_FORMAT;

	/* iterate through program header segments */
	for (i = 0; i < ehdr->e_phnum; i++) {
		/* readonly text segment */
		if (phdr->p_type == PT_LOAD &&
		    phdr->p_flags == PF_R + PF_X &&
		    phdr->p_align == 0x80) {
			/* make sure base addr is what we expect */
			if (text_found ||
			    phdr->p_vaddr != MARS_TASK_BASE_ADDR ||
			    phdr->p_memsz != phdr->p_filesz) {
				ret = MARS_ERROR_FORMAT;
				goto error;
			}

			/* initialize the task context text info */
			task->text_ea = mars_ea_map((void *)ehdr +
						    phdr->p_offset,
						    phdr->p_filesz);
			if (!task->text_ea) {
				ret = MARS_ERROR_MEMORY;
				goto error;
			}

			task->text_vaddr = phdr->p_vaddr;
			task->text_size = phdr->p_filesz;

			/* make sure we only find 1 text segment */
			text_found = 1;
		/* read-write data segment */
		} else if (phdr->p_type == PT_LOAD &&
			   phdr->p_flags == PF_R + PF_W &&
			   phdr->p_align == 0x80) {
			if (data_found) {
				ret = MARS_ERROR_FORMAT;
				goto error;
			}

			task->data_ea = mars_ea_map((void *)ehdr +
						    phdr->p_offset,
						    phdr->p_filesz);
			if (!task->data_ea) {
				ret = MARS_ERROR_MEMORY;
				goto error;
			}

			task->data_vaddr = phdr->p_vaddr;
			task->data_size = phdr->p_filesz;
			task->bss_size = phdr->p_memsz - phdr->p_filesz;

			/* make sure we only find 1 data segment */
			data_found = 1;
		}

		/* increment program header */
		phdr = (void *)phdr + ehdr->e_phentsize;
	}

	/* make sure text and data segment is found */
	if (!text_found || !data_found) {
		ret = MARS_ERROR_FORMAT;
		goto error;
	}

	/* set the entry point of execution */
	task->entry = ehdr->e_entry;

	return MARS_SUCCESS;

error:
	if (text_found)
		mars_ea_unmap(task->text_ea, task->text_size);
	if (data_found)
		mars_ea_unmap(task->data_ea, task->data_size);

	return ret;
}

int mars_task_create(struct mars_context *mars,
		     struct mars_task_id *id_ret,
		     const char *name, const void *elf_image,
		     uint32_t context_save_size)
{
	int ret;
	uint16_t workload_id;
	uint64_t workload_ea;
	struct mars_task_context *task;

	/* check function params */
	if (!mars)
		return MARS_ERROR_NULL;
	if (!id_ret)
		return MARS_ERROR_NULL;
	if (!elf_image)
		return MARS_ERROR_NULL;
	if (name && strlen(name) > MARS_TASK_NAME_LEN_MAX)
		return MARS_ERROR_PARAMS;
	if (context_save_size > MARS_TASK_CONTEXT_SAVE_SIZE_MAX)
		return MARS_ERROR_PARAMS;

	/* invalidate id */
	id_ret->mars_context_ea = 0;

	/* begin process to add the task to the workload queue */
	ret = mars_workload_queue_add_begin(mars, &workload_id, &workload_ea,
					    mars_task_module_entry,
					    MARS_TASK_MODULE_NAME);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	task = mars_ea_work_area_get(workload_ea,
				     MARS_TASK_CONTEXT_ALIGN,
				     MARS_TASK_CONTEXT_SIZE);

	/* map task ELF */
	ret = task_map_elf(task, elf_image);
	if (ret != MARS_SUCCESS) {
		mars_workload_queue_add_end(mars, workload_id, 1);
		return ret;
	}

	/* initialize task id */
	task->id.mars_context_ea = mars_ptr_to_ea(mars);
	task->id.workload_id = workload_id;
	if (name)
		strcpy((char *)task->id.name, name);
	else
		task->id.name[0] = 0;

	/* initialize task exit code */
	task->exit_code = 0;
	
	/* no context save - run complete */
	if (context_save_size) {
		/* allocate context save area */
		task->context_save_area_ea =
			mars_ea_memalign(MARS_TASK_CONTEXT_SAVE_ALIGN,
					 context_save_size +
					 MARS_TASK_REGISTER_SAVE_AREA_SIZE);
		if (!task->context_save_area_ea) {
			mars_workload_queue_add_end(mars, workload_id, 1);
			return MARS_ERROR_MEMORY;
		}
	} else
		task->context_save_area_ea = 0;

	/* update task context on EA */
	mars_ea_put(workload_ea, task, MARS_TASK_CONTEXT_SIZE);
	mars_ea_sync();

	/* end process to add the task to the workload queue */
	ret = mars_workload_queue_add_end(mars, workload_id, 0);
	if (ret != MARS_SUCCESS) {
		mars_ea_free(task->context_save_area_ea);
		mars_workload_queue_add_end(mars, workload_id, 1);
		return ret;
	}

	/* return id to caller */
	*id_ret = task->id;

	return MARS_SUCCESS;
}

int mars_task_destroy(struct mars_task_id *id)
{
	int ret;
	struct mars_context *mars;
	struct mars_task_context *task;
	uint64_t workload_ea;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;
	if (!id->mars_context_ea)
		return MARS_ERROR_PARAMS;

	/* get mars context pointer from task id */
	mars = mars_ea_to_ptr(id->mars_context_ea);

	/* begin process to remove the task from the workload queue */
	ret = mars_workload_queue_remove_begin(mars, id->workload_id,
					       &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	task = mars_ea_work_area_get(workload_ea,
				     MARS_TASK_CONTEXT_ALIGN,
				     MARS_TASK_CONTEXT_SIZE);

	/* get task context from EA */
	mars_ea_get(workload_ea, task, MARS_TASK_CONTEXT_SIZE);

	/* free the allocated context save area if it has one */
	if (task->context_save_area_ea)
		mars_ea_free(task->context_save_area_ea);

	/* unmap task ELF */
	mars_ea_unmap(task->text_ea, task->text_size);
	mars_ea_unmap(task->data_ea, task->data_size);

	/* invalidate id */
	id->mars_context_ea = 0;

	/* end process to remove the task from the workload queue */
	return mars_workload_queue_remove_end(mars, id->workload_id, 0);
}

int mars_task_schedule(const struct mars_task_id *id,
		       const struct mars_task_args *args,
		       uint8_t priority)
{
	int ret;
	struct mars_context *mars;
	struct mars_task_context *task;
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
						 priority, &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	task = mars_ea_work_area_get(workload_ea,
				     MARS_TASK_CONTEXT_ALIGN,
				     MARS_TASK_CONTEXT_SIZE);

	/* get task context from EA */
	mars_ea_get(workload_ea, task, MARS_TASK_CONTEXT_SIZE);

	/* initialize task specific context variables */
	task->stack = 0;
	task->exit_code = 0;
	if (args)
		memcpy(&task->args, args, sizeof(struct mars_task_args));

	/* update task context on EA */
	mars_ea_put(workload_ea, task, MARS_TASK_CONTEXT_SIZE);
	mars_ea_sync();

	/* end process to schedule the workload in the workload queue */
	return mars_workload_queue_schedule_end(mars, id->workload_id, 0);
}

int mars_task_unschedule(const struct mars_task_id *id, int32_t exit_code)
{
	int ret;
	struct mars_context *mars;
	struct mars_task_context *task;
	uint64_t workload_ea;

	/* check function params */
	if (!id)
		return MARS_ERROR_NULL;
	if (!id->mars_context_ea)
		return MARS_ERROR_PARAMS;

	/* get mars context pointer from task id */
	mars = mars_ea_to_ptr(id->mars_context_ea);

	/* begin process to schedule the workload in the workload queue */
	ret = mars_workload_queue_unschedule_begin(mars, id->workload_id,
						   &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	/* prepare work area for task context */
	task = mars_ea_work_area_get(workload_ea,
				     MARS_TASK_CONTEXT_ALIGN,
				     MARS_TASK_CONTEXT_SIZE);

	/* get task context from EA */
	mars_ea_get(workload_ea, task, MARS_TASK_CONTEXT_SIZE);

	/* store exit code in task context */
	task->exit_code = exit_code;

	/* update task context on EA */
	mars_ea_put(workload_ea, task, MARS_TASK_CONTEXT_SIZE);
	mars_ea_sync();

	/* end process to unschedule the workload in the workload queue */
	return mars_workload_queue_unschedule_end(mars, id->workload_id);
}

int mars_task_wait(const struct mars_task_id *id, int32_t *exit_code)
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

	/* exit_code requested so return it to caller */
	if (exit_code)
		*exit_code = mars_ea_get_uint32(task_exit_code_ea(workload_ea));

	return MARS_SUCCESS;
}

int mars_task_try_wait(const struct mars_task_id *id, int32_t *exit_code)
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

	/* non-blocking wait for workload completion */
	ret = mars_workload_queue_try_wait(mars, id->workload_id, &workload_ea);
	if (ret != MARS_SUCCESS)
		return ret;

	/* exit_code requested so return it to caller */
	if (exit_code)
		*exit_code = mars_ea_get_uint32(task_exit_code_ea(workload_ea));

	return MARS_SUCCESS;
}

uint32_t mars_task_get_ticks(void)
{
	return mars_get_ticks();
}
