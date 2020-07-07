#ifndef __WORKLOAD_INTERNAL_TYPES_H__
#define __WORKLOAD_INTERNAL_TYPES_H__

#include <stdint.h>

#include "mars/callback_types.h"
#include "mars/workload_types.h"

#define MARS_WORKLOAD_STATE_NONE					0x00	/* workload undefined */
#define MARS_WORKLOAD_STATE_ADDING					0x01	/* adding now */
#define MARS_WORKLOAD_STATE_REMOVING				0x02	/* removing now */
#define MARS_WORKLOAD_STATE_SCHEDULING				0x04	/* scheduling now */
#define MARS_WORKLOAD_STATE_UNSCHEDULING			0x08	/* unscheduling now */
#define MARS_WORKLOAD_STATE_READY					0x10	/* ready to schedule */
#define MARS_WORKLOAD_STATE_WAITING					0x20	/* waiting for sync */
#define MARS_WORKLOAD_STATE_RUNNING					0x40	/* currently running */
#define MARS_WORKLOAD_STATE_FINISHED				0x80	/* not allow schedule */

#define MARS_WORKLOAD_PRIORITY_MIN					0x00	/* minimum priority */
#define MARS_WORKLOAD_PRIORITY_MAX					0xff	/* maximum priority */

#define MARS_WORKLOAD_COUNTER_MIN					0x0000	/* minimum counter */
#define MARS_WORKLOAD_COUNTER_MAX					0x7fff	/* maximum counter */

#define MARS_WORKLOAD_SIGNAL_OFF					0x0	/* signal set off */
#define MARS_WORKLOAD_SIGNAL_ON						0x1	/* signal set on */

#define MARS_WORKLOAD_ID_NONE						0xffff	/* workload id none */
#define MARS_WORKLOAD_ID_MAX						799	/* workload id max */

#define MARS_WORKLOAD_PER_BLOCK						16	/* wl/block (lock+15) */
#define MARS_WORKLOAD_NUM_BLOCKS					50	/* total blocks */
#define MARS_WORKLOAD_MAX							750	/* blocks * wl/block */

#define MARS_WORKLOAD_QUEUE_SIZE					198528	/* size 198528 bytes */
#define MARS_WORKLOAD_QUEUE_ALIGN					128	/* align to 128 bytes */
#define MARS_WORKLOAD_QUEUE_HEADER_SIZE				128	/* size of 128 bytes */
#define MARS_WORKLOAD_QUEUE_HEADER_ALIGN			128	/* align to 128 bytes */
#define MARS_WORKLOAD_QUEUE_BLOCK_SIZE				128	/* size to 128 bytes */
#define MARS_WORKLOAD_QUEUE_BLOCK_ALIGN				128	/* align to 128 bytes */

#define MARS_WORKLOAD_QUEUE_FLAG_NONE				0x0	/* no flag set */
#define MARS_WORKLOAD_QUEUE_FLAG_EXIT				0x1	/* exit flag */

#define MARS_WORKLOAD_BLOCK_PRIORITY_MIN			MARS_WORKLOAD_PRIORITY_MIN
#define MARS_WORKLOAD_BLOCK_PRIORITY_MAX			MARS_WORKLOAD_PRIORITY_MAX

#define MARS_WORKLOAD_BLOCK_COUNTER_MIN				0x00
#define MARS_WORKLOAD_BLOCK_COUNTER_MAX				0x3f

#define MARS_WORKLOAD_BLOCK_READY_OFF				0x0
#define MARS_WORKLOAD_BLOCK_READY_ON				0x1

#define MARS_WORKLOAD_BLOCK_WAITING_OFF				0x0
#define MARS_WORKLOAD_BLOCK_WAITING_ON				0x1
/*
 * MARS workload queue header block bits (16-bits)
 * ------------------------------------------
 * |[15.....8]|[7.....2]|[   1   ]|[   0   ]|
 * ------------------------------------------
 * |  8-bits  | 6-bits  |  1-bit  |  1-bit  |
 * ------------------------------------------
 * | PRIORITY | COUNTER |  READY  | WAITING |
 * ------------------------------------------
 */
#define MARS_BITS_SHIFT_BLOCK_PRIORITY				8
#define MARS_BITS_SHIFT_BLOCK_COUNTER				2
#define MARS_BITS_SHIFT_BLOCK_READY					1
#define MARS_BITS_SHIFT_BLOCK_WAITING				0

#define MARS_BITS_MASK_BLOCK_PRIORITY				0x000000000000ff00ULL
#define MARS_BITS_MASK_BLOCK_COUNTER				0x00000000000000fcULL
#define MARS_BITS_MASK_BLOCK_READY					0x0000000000000002ULL
#define MARS_BITS_MASK_BLOCK_WAITING				0x0000000000000001ULL

/*
 * MARS workload queue block workload bits (64-bits)
 * ------------------------------------------------------------------
 * |[63....56]|[55....48]|[47....33]|[  32  ]|[31.....16]|[15......0]|
 * ------------------------------------------------------------------
 * |  8-bits  |  8-bits  |  15-bits |  1-bit |  16-bits  |  16-bits  |
 * ------------------------------------------------------------------
 * |  STATE   | PRIORITY |  COUNTER | SIGNAL |  WAIT_ID  | KERNEL_ID |
 * ------------------------------------------------------------------
 */
#define MARS_BITS_SHIFT_WORKLOAD_STATE				56
#define MARS_BITS_SHIFT_WORKLOAD_PRIORITY			48
#define MARS_BITS_SHIFT_WORKLOAD_COUNTER			33
#define MARS_BITS_SHIFT_WORKLOAD_SIGNAL				32
#define MARS_BITS_SHIFT_WORKLOAD_WAIT_ID			16
#define MARS_BITS_SHIFT_WORKLOAD_KERNEL_ID			0

#define MARS_BITS_MASK_WORKLOAD_STATE				0xff00000000000000ULL
#define MARS_BITS_MASK_WORKLOAD_PRIORITY			0x00ff000000000000ULL
#define MARS_BITS_MASK_WORKLOAD_COUNTER				0x0000fffe00000000ULL
#define MARS_BITS_MASK_WORKLOAD_SIGNAL				0x0000000100000000ULL
#define MARS_BITS_MASK_WORKLOAD_WAIT_ID				0x00000000ffff0000ULL
#define MARS_BITS_MASK_WORKLOAD_KERNEL_ID			0x000000000000ffffULL

#define MARS_HOST_SIGNAL_EXIT						0x0	/* host exit flag */

#define MARS_WORKLOAD_MODULE_SIZE					64
#define MARS_WORKLOAD_MODULE_ALIGN					16

#define MARS_WORKLOAD_CALLBACK_SIZE					64
#define MARS_WORKLOAD_CALLBACK_ALIGN				16

#define MARS_BITS_GET(bits, name)  \
	((*(bits)&MARS_BITS_MASK_##name)>>MARS_BITS_SHIFT_##name)

#define MARS_BITS_SET(bits, name, val) \
	(*bits) = ((*(bits)&~MARS_BITS_MASK_##name) | \
		((uint64_t)(val)<<MARS_BITS_SHIFT_##name))

/* 128 byte workload queue header structure */
struct mars_workload_queue_header {
	uint32_t lock;
	uint32_t access;
	uint64_t queue_ea;
	uint64_t context_ea;
	uint32_t flag;
	uint16_t bits[MARS_WORKLOAD_NUM_BLOCKS];
} __attribute__((aligned(MARS_WORKLOAD_QUEUE_HEADER_ALIGN)));

/* 128 byte workload queue block structure */
struct mars_workload_queue_block {
	/* bits[0] reserved for mutex lock */
	uint64_t bits[MARS_WORKLOAD_PER_BLOCK];
} __attribute__((aligned(MARS_WORKLOAD_QUEUE_BLOCK_ALIGN)));

/* mars workload queue structure */
struct mars_workload_queue {
	struct mars_workload_queue_header header;
	struct mars_workload_queue_block block[MARS_WORKLOAD_NUM_BLOCKS];
	struct mars_workload_context context[MARS_WORKLOAD_MAX];
} __attribute__((aligned(MARS_WORKLOAD_QUEUE_ALIGN)));

/* mars workload module structure */
struct mars_workload_module {
	uint64_t text_ea;
	uint64_t data_ea;
	uint32_t text_vaddr;
	uint32_t data_vaddr;
	uint32_t text_size;
	uint32_t data_size;
	uint32_t bss_size;
	uint32_t entry;
	uint8_t name[MARS_WORKLOAD_MODULE_NAME_LEN_MAX + 1];
} __attribute__((aligned(MARS_WORKLOAD_MODULE_ALIGN)));

/* mars workload callback structure */
struct mars_workload_callback {
	struct mars_callback_args callback_args;
	uint64_t callback_ea;
	uint32_t callback_ret;
	uint32_t pad;
} __attribute__((aligned(MARS_WORKLOAD_CALLBACK_ALIGN)));

#endif
