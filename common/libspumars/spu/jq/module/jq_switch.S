.section .bss

/* void *__module_stack */
.align	4
.globl	__module_stack
__module_stack:
.space 16

.text

/* void mars_module_main(void) */
.global mars_module_main
.type	mars_module_main, @function
mars_module_main:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain */
	ai	$SP, $SP, -32			/* push stack frame */

	stqa	$SP, __module_stack		/* save module stack */
	brsl	$LR, __module_main		/* call module main body */

	ai	$SP, $SP, 32			/* pop stack frame */
	lqd	$LR, 16($SP)			/* restore link register */
	bi	$LR				/* return */

.size	mars_module_main, .-mars_module_main

/* void jq_exit(void) */
.global	jq_exit
.type	jq_exit, @function
jq_exit:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain */
	ai	$SP, $SP, -32			/* push stack frame */

	lqa	$SP, __module_stack		/* restore module stack */
	br	mars_module_workload_finish	/* module finish (no return) */

.size	jq_exit, .-jq_exit
