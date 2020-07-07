/* NOTE: Order of defines must be the same as the member order of
         struct mars_kernel_syscalls declaration in kernel_internal_types.h */
#define get_ticks						0
#define get_mars_context_ea				4
#define get_kernel_id					8
#define get_workload_id					12
#define get_workload					16
#define get_workload_by_id				20
#define workload_exit					24
#define workload_query					28
#define workload_wait_set				32
#define workload_wait_reset				36
#define workload_signal_set				40
#define workload_signal_reset			44
#define workload_schedule_begin			48
#define workload_schedule_end			52
#define workload_unschedule_begin		56
#define workload_unschedule_end			60
#define host_signal_send				64
#define host_callback_set				68
#define host_callback_reset				72
#define mutex_lock_get					76
#define mutex_unlock_put				80
#define dma_get							84
#define dma_put							88
#define dma_wait						92

/* NOTE: Value of defines must equal defines in workload_internal_types.h */
#define WORKLOAD_EXIT_STATE_READY		0x10 /* MARS_WORKLOAD_STATE_READY */
#define WORKLOAD_EXIT_STATE_WAITING		0x20 /* MARS_WORKLOAD_STATE_WAITING */
#define WORKLOAD_EXIT_STATE_FINISHED	0x80 /* MARS_WORKLOAD_STATE_FINISHED */

.section .bss

/* const struct mars_kernel_syscalls *kernel_syscalls */
.align	4
.globl	kernel_syscalls
kernel_syscalls:
.space	16


.section .text

/* void mars_module_entry(const struct mars_kernel_syscalls *syscalls) */
.global	mars_module_entry
.type	mars_module_entry, @function
mars_module_entry:
	ila	$SP, __stack			/* switch to module stack */
	il	$LR, 0				/* set link register to NULL */
	stqd	$LR, 0($SP)			/* init back chain to NULL */
	stqd	$SP, -32($SP)			/* init stack frame */
	ai	$SP, $SP, -32			/* push stack frame */

	stqr	$3, kernel_syscalls		/* save kernel syscalls ptr */
	brsl	$LR, _init			/* call _init() */
	brsl	$LR, mars_module_main		/* call mars_module_main() */
	br	mars_module_workload_finish	/* return to kernel */
.size	mars_module_entry, .-mars_module_entry


/* uint32_t mars_module_get_ticks(void) */
.global	mars_module_get_ticks
.type	mars_module_get_ticks, @function
mars_module_get_ticks:
	il	$2, get_ticks
	br	call_kernel_syscall
.size	mars_module_get_ticks, .-mars_module_get_ticks


/* uint64_t mars_module_get_mars_context_ea(void) */
.global	mars_module_get_mars_context_ea
.type	mars_module_get_mars_context_ea, @function
mars_module_get_mars_context_ea:
	il	$2, get_mars_context_ea
	br	call_kernel_syscall
.size	mars_module_get_mars_context_ea, .-mars_module_get_mars_context_ea


/* uint16_t mars_module_get_kernel_id(void) */
.global	mars_module_get_kernel_id
.type	mars_module_get_kernel_id, @function
mars_module_get_kernel_id:
	il	$2, get_kernel_id
	br	call_kernel_syscall
.size	mars_module_get_kernel_id, .-mars_module_get_kernel_id

/* uint16_t mars_module_get_workload_id(void) */
.global	mars_module_get_workload_id
.type	mars_module_get_workload_id, @function
mars_module_get_workload_id:
	il	$2, get_workload_id
	br	call_kernel_syscall
.size	mars_module_get_workload_id, .-mars_module_get_workload_id


/* struct mars_workload_context *mars_module_get_workload(void) */
.global	mars_module_get_workload
.type	mars_module_get_workload, @function
mars_module_get_workload:
	il	$2, get_workload
	br	call_kernel_syscall
.size	mars_module_get_workload, .-mars_module_get_workload


/* struct mars_workload_context *mars_module_get_workload_by_id(uint16_t id) */
.global	mars_module_get_workload_by_id
.type	mars_module_get_workload_by_id, @function
mars_module_get_workload_by_id:
	il	$2, get_workload_by_id
	br	call_kernel_syscall
.size	mars_module_get_workload_by_id, .-mars_module_get_workload_by_id


/* int mars_module_workload_query(uint16_t id, int query) */
.global	mars_module_workload_query
.type	mars_module_workload_query, @function
mars_module_workload_query:
	il	$2, workload_query
	br	call_kernel_syscall
.size	mars_module_workload_query, .-mars_module_workload_query


/* int mars_module_workload_wait_set(uint16_t id) */
.global	mars_module_workload_wait_set
.type	mars_module_workload_wait_set, @function
mars_module_workload_wait_set:
	il	$2, workload_wait_set
	br	call_kernel_syscall
.size	mars_module_workload_wait_set, .-mars_module_workload_wait_set

/* int mars_module_workload_wait_reset(void) */
.global	mars_module_workload_wait_reset
.type	mars_module_workload_wait_reset, @function
mars_module_workload_wait_reset:
	il	$2, workload_wait_reset
	br	call_kernel_syscall
.size	mars_module_workload_wait_reset, .-mars_module_workload_wait_reset


/* int mars_module_workload_signal_set(uint16_t id) */
.global	mars_module_workload_signal_set
.type	mars_module_workload_signal_set, @function
mars_module_workload_signal_set:
	il	$2, workload_signal_set
	br	call_kernel_syscall
.size	mars_module_workload_signal_set, .-mars_module_workload_signal_set


/* int mars_module_workload_signal_reset(void) */
.global	mars_module_workload_signal_reset
.type	mars_module_workload_signal_reset, @function
mars_module_workload_signal_reset:
	il	$2, workload_signal_reset
	br	call_kernel_syscall
.size	mars_module_workload_signal_reset, .-mars_module_workload_signal_reset


/* int mars_module_workload_schedule_begin(uint16_t id, uint8_t priority,
				struct mars_workload_context **workload) */
.global	mars_module_workload_schedule_begin
.type	mars_module_workload_schedule_begin, @function
mars_module_workload_schedule_begin:
	il	$2, workload_schedule_begin
	br	call_kernel_syscall
.size	mars_module_workload_schedule_begin, .-mars_module_workload_schedule_begin


/* int mars_module_workload_schedule_end(uint16_t id, int cancel) */
.global	mars_module_workload_schedule_end
.type	mars_module_workload_schedule_end, @function
mars_module_workload_schedule_end:
	il	$2, workload_schedule_end
	br	call_kernel_syscall
.size	mars_module_workload_schedule_end, .-mars_module_workload_schedule_end


/* int mars_module_workload_unschedule_begin(uint16_t id,
				struct mars_workload_context **workload) */
.global	mars_module_workload_unschedule_begin
.type	mars_module_workload_unschedule_begin, @function
mars_module_workload_unschedule_begin:
	il	$2, workload_unschedule_begin
	br	call_kernel_syscall
.size	mars_module_workload_unschedule_begin, .-mars_module_workload_unschedule_begin


/* int mars_module_workload_unschedule_end(uint16_t id) */
.global	mars_module_workload_unschedule_end
.type	mars_module_workload_unschedule_end, @function
mars_module_workload_unschedule_end:
	il	$2, workload_unschedule_end
	br	call_kernel_syscall
.size	mars_module_workload_unschedule_end, .-mars_module_workload_unschedule_end


/* void mars_module_workload_wait(void) */
.global	mars_module_workload_wait
.type	mars_module_workload_wait, @function
mars_module_workload_wait:
	il	$3, WORKLOAD_EXIT_STATE_WAITING
	il	$2, workload_exit
	br	call_kernel_syscall
.size	mars_module_workload_wait, .-mars_module_workload_wait


/* void mars_module_workload_yield(void) */
.global	mars_module_workload_yield
.type	mars_module_workload_yield, @function
mars_module_workload_yield:
	il	$3, WORKLOAD_EXIT_STATE_READY
	il	$2, workload_exit
	br	call_kernel_syscall
.size	mars_module_workload_yield, .-mars_module_workload_yield


/* void mars_module_workload_finish(void) */
.global	mars_module_workload_finish
.type	mars_module_workload_finish, @function
mars_module_workload_finish:
	il	$3, WORKLOAD_EXIT_STATE_FINISHED
	il	$2, workload_exit
	br	call_kernel_syscall
.size	mars_module_workload_finish, .-mars_module_workload_finish

/* int mars_module_host_signal_send(uint64_t watch_point_ea) */
.global	mars_module_host_signal_send
.type	mars_module_host_signal_send, @function
mars_module_host_signal_send:
	il	$2, host_signal_send
	br	call_kernel_syscall
.size	mars_module_host_signal_send, .-mars_module_host_signal_send


/* int mars_module_host_callback_set(uint64_t callback_ea,
				     const struct mars_callback_args *in) */
.global	mars_module_host_callback_set
.type	mars_module_host_callback_set, @function
mars_module_host_callback_set:
	il	$2, host_callback_set
	br	call_kernel_syscall
.size	mars_module_host_callback_set, .-mars_module_host_callback_set


/* int mars_module_host_callback_reset(struct mars_callback_args *out) */
.global	mars_module_host_callback_reset
.type	mars_module_host_callback_reset, @function
mars_module_host_callback_reset:
	il	$2, host_callback_reset
	br	call_kernel_syscall
.size	mars_module_host_callback_reset, .-mars_module_host_callback_reset


/* int mars_module_mutex_lock_get(uint64_t mutex_ea,
				  struct mars_mutex *mutex) */
.global	mars_module_mutex_lock_get
.type	mars_module_mutex_lock_get, @function
mars_module_mutex_lock_get:
	il	$2, mutex_lock_get
	br	call_kernel_syscall
.size	mars_module_mutex_lock_get, .-mars_module_mutex_lock_get


/* int mars_module_mutex_unlock_put(uint64_t mutex_ea,
				    struct mars_mutex *mutex)  */
.global	mars_module_mutex_unlock_put
.type	mars_module_mutex_unlock_put, @function
mars_module_mutex_unlock_put:
	il	$2, mutex_unlock_put
	br	call_kernel_syscall
.size	mars_module_mutex_unlock_put, .-mars_module_mutex_unlock_put


/* int mars_module_dma_get(void *ls, uint64_t ea, uint32_t size,
			   uint32_t tag)  */
.global	mars_module_dma_get
.type	mars_module_dma_get, @function
mars_module_dma_get:
	il	$2, dma_get
	br	call_kernel_syscall
.size	mars_module_dma_get, .-mars_module_dma_get


/* int mars_module_dma_put(const void *ls, uint64_t ea, uint32_t size,
			   uint32_t tag) */
.global	mars_module_dma_put
.type	mars_module_dma_put, @function
mars_module_dma_put:
	il	$2, dma_put
	br	call_kernel_syscall
.size	mars_module_dma_put, .-mars_module_dma_get


/* int mars_module_dma_wait(uint32_t tag) */
.global	mars_module_dma_wait
.type	mars_module_dma_wait, @function
mars_module_dma_wait:
	il	$2, dma_wait
	br	call_kernel_syscall
.size	mars_module_dma_wait, .-mars_module_dma_wait


call_kernel_syscall:
	lqr	$76, kernel_syscalls
	lqx	$77, $76, $2
	a	$78, $76, $2
	rotqby  $79, $77, $78
	bi	$79
