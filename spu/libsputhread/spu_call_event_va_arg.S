.section .text

.type  _spu_call_event_va_arg, @function
.globl _spu_call_event_va_arg
_spu_call_event_va_arg:
	rchcnt	$2, $ch29
	brnz	$2, 2f
	stqd	$4, -256($1)
	stqd	$5, -240($1)
	stqd	$6, -224($1)
	stqd	$7, -208($1)
	stqd	$8, -192($1)
	stqd	$9, -176($1)
	stqd	$10, -160($1)
	stqd	$11, -144($1)
	stqd	$12, -128($1)
	stqd	$13, -112($1)
	stqd	$14, -96($1)
	stqd	$15, -80($1)
	stqd	$16, -64($1)
	stqd	$17, -48($1)
	stqd	$18, -32($1)
	stqd	$19, -16($1)
	or		$2, $2, $1
	ai		$2, $2, -256
	wrch	$ch28, $2
	dsync
	wrch	$ch30, $3
	rdch	$3, $ch29
	brnz	$3, 1f
	rdch	$3, $ch29
1:	bi		$0
2:	ilh		$3, 32769	# 8001
	ori		$3, $3, 10
	bi		$0
	nop		$0
	lnop
