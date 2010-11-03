#pragma once

#define HNODE_USED 1
#define HNODE_UNUSED 0

typedef struct _hnode
{
	u64 address;
	u64 aligned_address;
	u32 used;
	u32 size;
	struct _hnode *prev;
	struct _hnode *next;
} __attribute__((packed)) hnode_t;

typedef struct _heap
{
	u64 start;
	u64 size;
	u32 usedmem;
	hnode_t *first;
} __attribute__((packed)) heap_t;

