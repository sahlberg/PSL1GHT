#ifndef __HEAP_INL__
#define __HEAP_INL__

static __inline__ heap_block* __heap_head(heap_cntrl *theheap)
{
	return &theheap->free_list;
}

static __inline__ heap_block* __heap_tail(heap_cntrl *theheap)
{
	return &theheap->free_list;
}

static __inline__ heap_block* __heap_first(heap_cntrl *theheap)
{
	return __heap_head(theheap)->next;
}

static __inline__ heap_block* __heap_last(heap_cntrl *theheap)
{
	return __heap_tail(theheap)->prev;
}

static __inline__ void __heap_block_remove(heap_block *block)
{
	heap_block *next = block->next;
	heap_block *prev = block->prev;
	prev->next = next;
	next->prev = prev;
}

static __inline__ void __heap_block_replace(heap_block *old_block, heap_block *new_block)
{
	heap_block *next = old_block->next;
	heap_block *prev = old_block->prev;

	new_block->next = next;
	new_block->prev = prev;

	next->prev = new_block;
	prev->next = new_block;
}

static __inline__ void __heap_block_insert_after(heap_block *block_before, heap_block *new_block)
{
	heap_block *next = block_before->next;

	new_block->next = next;
	new_block->prev = block_before;

	block_before->next = new_block;
	next->prev = new_block;
}

static __inline__ void __heap_block_insert_before(heap_block *block_next, heap_block *new_block)
{
	heap_block *prev = block_next->prev;

	new_block->next = block_next;
	new_block->prev = prev;

	prev->next = new_block;
	block_next->prev = new_block;
}

static __inline__ bool __heap_is_aligned(uintptr_t value, uintptr_t alignment)
{
	return (value%alignment) == 0;
}

static __inline__ uintptr_t __heap_align_up(uintptr_t value, uintptr_t alignment)
{
	uintptr_t reminder = value%alignment;
	return (reminder != 0 ? (value - reminder + alignment) : value);
}

static __inline__ uintptr_t __heap_align_down(uintptr_t value, uintptr_t alignment)
{
	return (value - (value%alignment));
}

static __inline__ heap_block* __heap_block_at(const heap_block *block, uintptr_t offset)
{
	return (heap_block*)((uintptr_t)block + offset);
}

static __inline__ heap_block* __heap_block_prev(const heap_block *block)
{
	return (heap_block*)((uintptr_t)block + block->prev_size);
}

static __inline__ uintptr_t __heap_alloc_area_of_block(const heap_block *block)
{
	return (uintptr_t)block + HEAP_BLOCK_HEADER_SIZE;
}

static __inline__ heap_block* __heap_block_of_alloc_area(uintptr_t alloc_begin, uintptr_t page_size)
{
	return (heap_block*)(__heap_align_down(alloc_begin, page_size) - HEAP_BLOCK_HEADER_SIZE);
}

static __inline__ uintptr_t __heap_block_size(const heap_block *block)
{
	return (block->size&~HEAP_BLOCK_PREV_USED);
}

static __inline__ void __heap_block_set_size(heap_block *block, uintptr_t size)
{
	uintptr_t flag = block->size&HEAP_BLOCK_PREV_USED;
	block->size = size|flag;
}

static __inline__ bool __heap_prev_used(const heap_block *block)
{
	return (block->size&HEAP_BLOCK_PREV_USED);
}

static __inline__ bool __heap_is_used(const heap_block *block)
{
	const heap_block *const next_block = __heap_block_at(block, __heap_block_size(block));
	return __heap_prev_used(next_block);
}

static __inline__ bool __heap_is_free(const heap_block *block)
{
	return !__heap_is_used(block);
}

static __inline__ bool __heap_block_in(const heap_cntrl *theheap, const heap_block *block)
{
	return ((uintptr_t)block >= (uintptr_t)theheap->first_block && (uintptr_t)block <= (uintptr_t)theheap->last_block);
}

static __inline__ uintptr_t __heap_min_block_size(uintptr_t page_size)
{
	return __heap_align_up(sizeof(heap_block), page_size);
}

static __inline__ uintptr_t __heap_area_overhead(uintptr_t page_size)
{
	if (page_size != 0)
		page_size = __heap_align_up(page_size, CPU_ALIGNMENT);
	else
		page_size = CPU_ALIGNMENT;
	
	return 2*(page_size - 1) + HEAP_BLOCK_HEADER_SIZE;
}

static __inline__ uintptr_t __heap_size_with_overhead(uintptr_t page_size, uintptr_t size, uintptr_t alignment)
{
	if (page_size != 0)
		page_size = __heap_align_up(page_size, CPU_ALIGNMENT);
	else
		page_size = CPU_ALIGNMENT;
	
	if (page_size < alignment)
		page_size = alignment;
	
	return (HEAP_BLOCK_HEADER_SIZE + page_size - 1 + size);
}

static __inline__ void __heap_set_last_block_size(heap_cntrl *theheap)
{
	__heap_block_set_size(theheap->last_block, (uintptr_t)theheap->first_block - (uintptr_t)theheap->last_block);
}

#ifdef HEAP_DEBUG
	#include <assert.h>
	#define _HAssert(cond) \
		do { \
			if (!(cond)) { \
				__assert(__FILE__, __LINE__, #cond); \
			} \
		} while(0)
#else
	#define _HAssert(cond)
#endif // HEAP_DEBUG

#endif
