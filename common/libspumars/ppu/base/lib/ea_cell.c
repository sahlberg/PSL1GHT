#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ppu-types.h>
#include <ppu_intrinsics.h>

#include <mars/base.h>

#include "numa_internal.h"

/* These functions must not be called */
static uint64_t numa_ea_memalign(size_t boundary, size_t size)
{
	(void)boundary;
	(void)size;

	return 0;
}

static void numa_ea_free(uint64_t ea)
{
	(void)ea;
}

uint64_t mars_ea_memalign(size_t boundary, size_t size)
{
	if (mars_numa_enabled())
		return numa_ea_memalign(boundary, size);
	else
		return mars_ptr_to_ea(memalign(boundary, size));
}

void mars_ea_free(uint64_t ea)
{
	if (mars_numa_enabled())
		numa_ea_free(ea);
	else
		free(mars_ea_to_ptr(ea));
}

/* copy data from EA to host */
void mars_ea_get(uint64_t ea, void *mem, size_t size)
{
	const void *src = mars_ea_to_ptr(ea);
	if (src != mem)
		memcpy(mem, src, size);
}

/* get uint8 value from EA */
uint8_t mars_ea_get_uint8(uint64_t ea)
{
	return *(uint8_t *)mars_ea_to_ptr(ea);
}

/* get uint16 value from EA */
uint16_t mars_ea_get_uint16(uint64_t ea)
{
	return *(uint16_t *)mars_ea_to_ptr(ea);
}

/* get uint32 value from EA */
uint32_t mars_ea_get_uint32(uint64_t ea)
{
	return *(uint32_t *)mars_ea_to_ptr(ea);
}

/* get uint64 value from EA */
uint64_t mars_ea_get_uint64(uint64_t ea)
{
	return *(uint64_t *)mars_ea_to_ptr(ea);
}

/* copy data from host to EA */
void mars_ea_put(uint64_t ea, const void *mem, size_t size)
{
	void *dst = mars_ea_to_ptr(ea);
	if (dst != mem)
		memcpy(dst, mem, size);
}

/* put uint8 value to EA */
void mars_ea_put_uint8(uint64_t ea, uint8_t value)
{
	*(uint8_t *)mars_ea_to_ptr(ea) = value;
}

/* put uint16 value to EA */
void mars_ea_put_uint16(uint64_t ea, uint16_t value)
{
	*(uint16_t *)mars_ea_to_ptr(ea) = value;
}

/* put uint32 value to EA */
void mars_ea_put_uint32(uint64_t ea, uint32_t value)
{
	*(uint32_t *)mars_ea_to_ptr(ea) = value;
}

/* put uint64 value to EA */
void mars_ea_put_uint64(uint64_t ea, uint64_t value)
{
	*(uint64_t *)mars_ea_to_ptr(ea) = value;
}

/* map readonly area on host memory to EA */
uint64_t mars_ea_map(void *ptr, size_t size)
{
#ifdef MARS_ENABLE_DISCRETE_SHARED_MEMORY
	uint64_t copy_ea;

	copy_ea = mars_ea_memalign(16, size); /* FIXME: 16 -> any macro */
	if (!copy_ea)
		return 0;

	mars_ea_put(copy_ea, ptr, size);
	mars_ea_sync();

	return copy_ea;
#else /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
	(void)size;
	return mars_ptr_to_ea(ptr);
#endif /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
}

/* unmap area mapped by mars_ea_map */
void mars_ea_unmap(uint64_t ea, size_t size)
{
	(void)size;

#ifdef MARS_ENABLE_DISCRETE_SHARED_MEMORY
	mars_ea_free(ea);
#else /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
	(void)ea;
#endif /* !MARS_ENABLE_DISCRETE_SHARED_MEMORY */
}

/* sync EA */
void mars_ea_sync(void)
{
	__lwsync();
}
