#include <spu_mfcio.h>

#include <mars/error.h>
#include <mars/module.h>

#include "task_module.h"

#define MARS_TASK_MODULE_DMA_SIZE_MAX		16384
#define MARS_TASK_MODULE_DMA_SIZE_MASK		0x7f

/* global task variables */
static struct mars_task_context *task;

/* called by task_switch.S */
void __module_main(void);
void __dma_registers(void *ptr, int put);
void __task_save(void *task_heap);
void __task_restore(int task_cached);

/* defined in task_switch.S */
extern void *__task_stack;
extern void task_exit(void);
extern void task_save(void *task_heap, int wait);
extern void task_restore(int task_cached);

/* task entry */
typedef void (*mars_task_entry)(struct mars_task_args *args,
	struct mars_task_module_syscalls *task_module_syscalls);

static uint32_t get_ticks(void)
{
	return mars_module_get_ticks();
}

static uint16_t get_kernel_id(void)
{
	return mars_module_get_kernel_id();
}

static struct mars_task_context *get_task(void)
{
	return (struct mars_task_context *)mars_module_get_workload();
}

static struct mars_task_context *
	get_task_by_id(const struct mars_task_id *task_id)
{
	return (struct mars_task_context *)
		mars_module_get_workload_by_id(task_id->workload_id);
}

static void dma_wait(void)
{
	mars_module_dma_wait(MARS_TASK_MODULE_DMA_TAG);
}

static void dma(void *ls, uint64_t ea, int size, int put)
{
	if (put)
		mars_module_dma_put(ls, ea, size, MARS_TASK_MODULE_DMA_TAG);
	else
		mars_module_dma_get(ls, ea, size, MARS_TASK_MODULE_DMA_TAG);
}

/*
 * Structure of Context Save Area
 *                                      High Address
 * +------------------------------------+
 * |       Local Storage (stack)        |
 * +------------------------------------+
 * | Local Storage (text + data + heap) |
 * +------------------------------------+
 * |      Non-volatile Registers        |
 * +------------------------------------+
 *                                      Low Address
*/
static void dma_context(uint32_t low_size, uint32_t high_size, int put)
{
	/* save or restore data segment and heap (low address) */
	dma((void *)task->data_vaddr, task->context_save_area_ea +
	    MARS_TASK_REGISTER_SAVE_AREA_SIZE,
	    low_size, put);

	/* save or restore stack (high address) */
	dma((void *)MARS_TASK_BASE_ADDR + MARS_TASK_CONTEXT_SAVE_SIZE_MAX -
	    high_size, task->context_save_area_ea +
	    MARS_TASK_REGISTER_SAVE_AREA_SIZE + low_size,
	    high_size, put);

	dma_wait();
}

void __dma_registers(void *ptr, int put)
{
	/* dma registers state to/from the context save area */
	dma(ptr, task->context_save_area_ea, MARS_TASK_REGISTER_SAVE_AREA_SIZE,
	    put);

	dma_wait();
}

void __task_save(void *task_heap)
{
	/* save workload stack pointer */
	task->stack = (uint32_t)__task_stack;

	/* save data segment and heap size (low address) */
	task->context_save_area_low_size =
		((uintptr_t)task_heap - task->data_vaddr +
		 MARS_TASK_MODULE_DMA_SIZE_MASK) &
		~MARS_TASK_MODULE_DMA_SIZE_MASK;

	/* save used stack size (high address) */
	task->context_save_area_high_size =
		(MARS_TASK_BASE_ADDR + MARS_TASK_CONTEXT_SAVE_SIZE_MAX -
		 (uintptr_t)__task_stack +
		 MARS_TASK_MODULE_DMA_SIZE_MASK) &
		~MARS_TASK_MODULE_DMA_SIZE_MASK;

	/* save context MPU storage state */
	dma_context(task->context_save_area_low_size,
		    task->context_save_area_high_size, 1);
}

void __task_restore(int task_cached)
{
	/* if task not cashed restore context MPU storage state */
	if (!task_cached)
		dma_context(task->context_save_area_low_size,
			    task->context_save_area_high_size, 0);

	/* restore workload stack pointer */
	__task_stack = (void *)task->stack;
}

static void task_yield(void *task_heap)
{
	task_save(task_heap, 0);
}

static void task_arg_copy(struct mars_task_args* __restrict__ dst, const struct mars_task_args* __restrict__ src)
{
	__vector const int *v_src = (__vector const int*)src;
	__vector int *v_dst = (__vector int*)dst;
	__vector int *v_end = (__vector int*)((char*)src + sizeof(*src));
	while(__builtin_expect(v_src < v_end, 1))
		*v_dst++ = *v_src++;
}
 
static int task_schedule(uint16_t workload_id,
			 const struct mars_task_args *args,
			 uint8_t priority)
{
	int ret;
	struct mars_task_context *schedule_task;
	struct mars_workload_context *schedule_workload;

	ret = mars_module_workload_schedule_begin(workload_id, priority,
						  &schedule_workload);
	if (ret != MARS_SUCCESS)
		return ret;

	/* cast workload context to task context */
	schedule_task = (struct mars_task_context *)schedule_workload;

	/* initialize task specific context variables */
	schedule_task->stack = 0;
	schedule_task->exit_code = 0;
	if (args) 
		task_arg_copy(&schedule_task->args, args);
	
	/* end process to schedule the workload in the workload queue */
	return mars_module_workload_schedule_end(workload_id, 0);
}

static int task_unschedule(uint16_t workload_id, int32_t exit_code)
{
	int ret;
	struct mars_task_context *unschedule_task;
	struct mars_workload_context *unschedule_workload;

	ret = mars_module_workload_unschedule_begin(workload_id,
						    &unschedule_workload);
	if (ret != MARS_SUCCESS)
		return ret;

	/* cast workload context to task context */
	unschedule_task = (struct mars_task_context *)unschedule_workload;

	unschedule_task->exit_code = exit_code;

	/* end process to unschedule the workload in the workload queue */
	return mars_module_workload_unschedule_end(workload_id);
}

static int task_wait(uint16_t workload_id, void *task_heap)
{
	int ret;

	ret = mars_module_workload_wait_set(workload_id);
	if (ret != MARS_SUCCESS)
		return ret;

	task_save(task_heap, 1);

	return mars_module_workload_wait_reset();
}

static int task_try_wait(uint16_t workload_id)
{
	/* make sure workload is initialized */
	if (!mars_module_workload_query(workload_id,
					MARS_WORKLOAD_QUERY_IS_INITIALIZED))
		return MARS_ERROR_STATE;

	/* if workload not finished return busy */
	if (!mars_module_workload_query(workload_id,
					MARS_WORKLOAD_QUERY_IS_FINISHED))
		return MARS_ERROR_BUSY;

	return mars_module_workload_wait_reset();
}

static void task_signal_host(uint64_t watch_point_ea)
{
	mars_module_host_signal_send(watch_point_ea);
}

static int task_signal_send(uint16_t workload_id)
{
	return mars_module_workload_signal_set(workload_id);
}

static int task_signal_wait(void *task_heap)
{
	task_save(task_heap, 1);

	return mars_module_workload_signal_reset();
}

static int task_signal_try_wait(void)
{
	/* if signal not yet received return busy */
	if (!mars_module_workload_query(mars_module_get_workload_id(),
					MARS_WORKLOAD_QUERY_IS_SIGNAL_SET))
		return MARS_ERROR_BUSY;

	return mars_module_workload_signal_reset();
}

static int task_call_host(uint64_t callback_ea,
			  const struct mars_callback_args *in,
			  struct mars_callback_args *out, void *task_heap)
{
	int ret;

	ret = mars_module_host_callback_set(callback_ea, in);
	if (ret != MARS_SUCCESS)
		return ret;

	task_save(task_heap, 1);

	mars_module_workload_signal_reset();

	return mars_module_host_callback_reset(out);
}

static struct mars_task_module_syscalls task_module_syscalls =
{
	get_ticks,
	get_kernel_id,
	get_task,
	get_task_by_id,

	task_exit,
	task_yield,
	task_schedule,
	task_unschedule,
	task_wait,
	task_try_wait,
	task_signal_host,
	task_signal_send,
	task_signal_wait,
	task_signal_try_wait,
	task_call_host,

	mars_module_mutex_lock_get,
	mars_module_mutex_unlock_put,

	mars_module_dma_get,
	mars_module_dma_put,
	mars_module_dma_wait
};

static void task_run(void)
{
	__vector unsigned char *bss_ptr, *bss_end;

	/* load the read-write data segment */
	dma((void *)task->data_vaddr, task->data_ea, task->data_size, 0);

	/* 0 the bss section */
	bss_ptr = (__vector unsigned char *)(task->data_vaddr +
					     task->data_size);
	bss_end = (__vector unsigned char *)((void *)bss_ptr +
					     task->bss_size);

	while (__builtin_expect(bss_ptr < bss_end, 1))
		*bss_ptr++ = spu_splats((unsigned char)0);

	dma_wait();

	/* sync before executing loaded code */
	spu_sync();

	/* call entry function */
	((mars_task_entry)task->entry)(&task->args, &task_module_syscalls);
}

void __module_main(void)
{
	int task_cached = 0;
	int text_cached = 0;
	uint64_t *cached_text_ea = (uint64_t *)(MARS_TASK_BASE_ADDR - 16);

	/* get task context */
	task = get_task();

	/* check if this workload module was in the cache */
	if (mars_module_workload_query(mars_module_get_workload_id(),
				       MARS_WORKLOAD_QUERY_IS_MODULE_CACHED)) {
		/* check if task context is cached in mpu storage */
		task_cached = mars_module_workload_query(
					mars_module_get_workload_id(),
					MARS_WORKLOAD_QUERY_IS_CONTEXT_CACHED);

		/* check if text is cached in mpu storage */
		text_cached = task_cached || (*cached_text_ea == task->text_ea);
	}

	/* only reload the readonly text segment if different from cached */
	if (!text_cached) {
		/* set info of current cached text */
		*cached_text_ea = task->text_ea;

		/* dma text segment from host storage to mpu storage */
		dma((void *)MARS_TASK_BASE_ADDR,
		    task->text_ea, task->text_size, 0);
	}

	/* if stack pointer is uninitialized run fresh, otherwise resume */
	if (!task->stack)
		task_run();
	else
		task_restore(task_cached);

	/* we should never reach here */
}
