#ifndef __MARS_TASK_INTERNAL_TYPES_H__
#define __MARS_TASK_INTERNAL_TYPES_H__

#include <stdint.h>

#include "mars/workload_types.h"

#define MARS_TASK_MODULE_NAME					"MARS TASK"

#define MARS_TASK_CONTEXT_SIZE					MARS_WORKLOAD_CONTEXT_SIZE
#define MARS_TASK_CONTEXT_ALIGN					MARS_WORKLOAD_CONTEXT_ALIGN
#define MARS_TASK_CONTEXT_SAVE_ALIGN			128

#define MARS_TASK_REGISTER_SAVE_AREA_SIZE 		(16 * (127 - 80))

struct mars_task_context {
	uint8_t workload_reserved[MARS_WORKLOAD_RESERVED_SIZE];

	uint64_t text_ea;			/* ea of text segment */
	uint64_t data_ea;			/* ea of data segment */
	uint32_t text_vaddr;			/* vaddr of text segment */
	uint32_t data_vaddr;			/* vaddr of data segment */
	uint32_t text_size;			/* size of text segment */
	uint32_t data_size;			/* size of data segment */
	uint32_t bss_size;			/* size of bss segment */
	uint32_t entry;				/* entry address of exec */
	uint32_t stack;				/* stack pointer of exec */
	uint32_t exit_code;			/* exit code */
	uint64_t context_save_area_ea;		/* context save area */
	uint32_t context_save_area_low_size;	/* size of low save area */
	uint32_t context_save_area_high_size;	/* size of high save area */
	struct mars_task_id id;			/* task id */
	struct mars_task_args args;		/* task args */

	uint8_t pad[MARS_TASK_CONTEXT_SIZE -
				(MARS_WORKLOAD_RESERVED_SIZE	+
				 sizeof(uint64_t)*3  			+
				 sizeof(uint32_t)*10 			+
				 sizeof(struct mars_task_id)	+
				 sizeof(struct mars_task_args))
			   ];
} __attribute__((aligned(MARS_TASK_CONTEXT_ALIGN)));

#endif
