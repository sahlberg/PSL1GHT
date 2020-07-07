.section .bss

.align 4
.globl __kernel_stack
__kernel_stack:
	.space 16


.text

.globl workload_run
.type  workload_run, @function
workload_run:
	stqd $LR, 16($SP)
	stqd $SP, -32($SP)
	ai $SP, $SP, -32
	
	stqa $SP, __kernel_stack
	brsl $LR, __workload_run
	
	ai $SP, $SP, 32
	lqd $LR, 16($SP)
	bi $LR
.size workload_run, .-workload_run

.globl workload_exit
.type workload_exit, @function
workload_exit:
	stqd $LR, 16($SP)
	stqd $SP, -32($SP)
	ai $SP, $SP, -32
	
	brsl $LR, __workload_exit
	lqa $SP, __kernel_stack
	
	ai $SP, $SP, 32
	lqd $LR, 16($SP)
	bi $LR
.size workload_exit, .-workload_exit
