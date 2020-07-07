#define NUM_REGS			48	/* number of registers to be saved/restored */

.section .bss

/* void *__module_stack */
.align	4
.globl	__module_stack
__module_stack:
.space 16

/* void *__task_stack */
.align	4
.globl	__task_stack
__task_stack:
.space 16

/* void *__work_stack*/
.align	4
.global	__work_stack
__work_stack:
.space (NUM_REGS + 3) * 16


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


/* void task_exit(void) */
.global	task_exit
.type	task_exit, @function
task_exit:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain */
	ai	$SP, $SP, -32			/* push stack frame */

	lqa	$SP, __module_stack		/* restore module stack */
	br	mars_module_workload_finish	/* module finish (no return) */

.size	task_exit, .-task_exit


/* void task_save(void *task_heap, int wait) */
.global	task_save
.type	task_save, @function
task_save:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain */
	ai	$SP, $SP, -32			/* push stack frame */

	stqa	$4, __work_stack		/* save func param */

	stqa	$SP, __task_stack		/* save task stack */

	lqa	$SP, __module_stack		/* restore module stack */

	brsl	$LR, __task_save		/* call task save body */

	brsl	$LR, __registers_save		/* save registers */

	lqa	$2, __work_stack		/* restore func param */

	brz	$2, mars_module_workload_yield	/* module yield (no return) */
	br	mars_module_workload_wait	/* module wait (no return) */

.size	task_save, .-task_save


/* void task_restore(int task_cached) */
.global	task_restore
.type	task_restore, @function
task_restore:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain */
	ai	$SP, $SP, -32			/* push stack frame */

	stqa	$3, __work_stack		/* save func param */

	brsl	$LR, __registers_restore	/* restore registers */

	lqa	$3, __work_stack		/* restore func param */

	brsl	$LR, __task_restore		/* call task restore body */

	lqa	$SP, __task_stack		/* restore task stack */
	sync					/* sync before execution */

	ai	$SP, $SP, 32			/* pop task_save stack frame */
	lqd	$LR, 16($SP)			/* restore link register */
	bi	$LR				/* return from task_save call */

.size	task_restore, .-task_restore


/*
 * Registers are saved/restored to task module work stack
 *
 *                          High Address
* +------------------------+ <--- TOP OF STACK   (__work_stack + 48 + 768)
 * | Non-volatile Registers |          |
 * |         $127           |          |
 * |         $126           |          |
 * |         $125           |       (size)
 * |         ...            |    NUM_REGS * 16   (48 * 16 = 768)
 * |         $82            |          |
 * |         $81            |          |
 * |         $80            |          |
 * +------------------------+ <--- REGS_PTR      (__work_stack + 48)
 * |    Work Code Block     |
 * +------------------------+ <--- CODE_PTR + 16 (__work_stack + 32)
 * |    Loop Code Block     |
 * +------------------------+ <--- CODE_PTR      (__work_stack + 16)
 * |    Save Func Param     |
 * +------------------------+ <--- __work_stack
 *                          Low Address
 */

#define CODE_PTR	$74	/* where code will be loaded into stack */
#define REGS_PTR	$75	/* where regs will be placed into stack */
#define REGS_INC	$76	/* register increment value for instruction */
#define TEMP		$77	/* temporary register */
#define INST		$78	/* register to store volatile instruction */
#define INST_MASK	$79	/* mask instruction to store or load */

__registers_save:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain pointer */
	ai	$SP, $SP, -32			/* push stack frame */

	il	INST_MASK, 0			/* no mask - default is stqd */
	br	body				/* jump to the body */

__registers_restore:
	stqd	$LR, 16($SP)			/* save link register */
	stqd	$SP, -32($SP)			/* save back chain pointer */
	ai	$SP, $SP, -32			/* push stack frame */

	ila	$3, __work_stack + 48		/* ptr = __work_stack */
	il	$4, 0				/* restore so put = 0 */
	brsl	$LR, __dma_registers		/* call __dma_registers(0) */

	il	INST_MASK, 16			/* set necessary bits */
	shlqbyi	INST_MASK, INST_MASK, 15	/* shift to correct position */

body:
	ila	CODE_PTR, __work_stack + 16	/* set code load addr pointer */
	ila	REGS_PTR, __work_stack + 48	/* set regs load addr pointer */
	lqr	TEMP, loop			/* load loop code to register */
	stqd	TEMP, 0(CODE_PTR)		/* store loop code to stack */
	lqr	INST, inst			/* load inst code to register */
	or	INST, INST, INST_MASK		/* mask instr to stqd or lqd */
	il	REGS_INC, 1			/* set bit in reg incrementer */
	shlqbyi	REGS_INC, REGS_INC, 12		/* shift to correct word slot */
	il	TEMP, NUM_REGS - 1		/* initialize loop counter */
	sync					/* wait till loop code stored */
	bisl	$LR, CODE_PTR			/* jump and return */

	brnz	INST_MASK, done			/* restore called so done */

	ila	$3, __work_stack + 48		/* ptr = __work_stack */
	il	$4, 1				/* regs save so put = 1 */
	brsl	$LR, __dma_registers		/* call __dma_registers(1) */

done:
	ai	$SP, $SP, 32			/* pop stack frame */
	lqd	$LR, 16($SP)			/* load saved link register */
	bi	$LR				/* done so return to caller */

	.balignl	16, 0			/* align self modifying code */
loop:
	stqd	INST, 16(CODE_PTR)		/* store inst code to stack */
	a	INST, INST, REGS_INC		/* increment register number */
	ai	TEMP, TEMP, -1			/* decrment loop counter */
	sync					/* wait till inst code stored */

inst:
	stqd	$80, 0(REGS_PTR)		/* store reg to stack */
	ai	REGS_PTR, REGS_PTR, 16		/* increment regs pointer */
	biz	TEMP, $LR			/* if (TEMP == 0) done */
	br	loop				/* if (TEMP != 0) loop */
