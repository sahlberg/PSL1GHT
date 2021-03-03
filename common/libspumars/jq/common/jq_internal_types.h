#ifndef __MARS_JQ_INTERNAL_TYPES_H__
#define __MARS_JQ_INTERNAL_TYPES_H__

#include <stdint.h>

#include "mars/workload_types.h"

#define MARS_JQ_MODULE_NAME                     "MARS JOB"

#define MARS_JQ_CONTEXT_SIZE					MARS_WORKLOAD_CONTEXT_SIZE
#define MARS_JQ_CONTEXT_ALIGN					MARS_WORKLOAD_CONTEXT_ALIGN

struct mars_jq_context {
	uint8_t workload_reserved[MARS_WORKLOAD_RESERVED_SIZE];

    struct mars_jq_id id;

	uint8_t pad[MARS_JQ_CONTEXT_SIZE -
				(MARS_WORKLOAD_RESERVED_SIZE	+
				 sizeof(uint64_t)*3  			+
				 sizeof(uint32_t)*10 			+
				 sizeof(struct mars_jq_id))
			   ];
} __attribute__((aligned(MARS_JQ_CONTEXT_ALIGN)));

#endif /* __MARS_JQ_INTERNAL_TYPES_H__ */
