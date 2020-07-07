.section .interrupt, "ax", @progbits
	heq $LR, $LR, $LR
	
.section .init

.globl _init
.type  _init, @function
_init:
	stqd $LR, 16($SP)
	stqd $SP, -32($SP)
	ai $SP, $SP, -32
	
	ai $SP, $SP, 32
	lqd $LR, 16($SP)
	bi $LR
	
.section .fini

.globl _fini
.type  _fini, @function
_fini:
	stqd $LR, 16($SP)
	stqd $SP, -32($SP)
	ai $SP, $SP, -32
	
	ai $SP, $SP, 32
	lqd $LR, 16($SP)
	bi $LR
		
.section .text

.globl _start
.type  _start, @function
_start:
	il $LR, 0
	ila $SP, __stack
	
	stqd	$LR, 0($SP)			/* init back chain to NULL */
	stqd	$SP, -32($SP)			/* init stack frame */
	ai	$SP, $SP, -32			/* push stack frame */
	
	brsl $LR, _init
	brsl $LR, main
	br exit
.size 	_start, .-_start
	