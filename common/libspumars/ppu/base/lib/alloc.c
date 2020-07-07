#include <stdlib.h>

#include <mars/base.h>

void *mars_malloc(size_t size)
{
	return malloc(size);
}

void *mars_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void mars_free(void *ptr)
{
	free(ptr);
}
