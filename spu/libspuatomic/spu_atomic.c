#include "spu_atomic.h"

uint32_t spu_atomic_add32(uint32_t *ls, uint64_t ea, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value + value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_add64(uint64_t *ls, uint64_t ea, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value + value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_and32(uint32_t *ls, uint64_t ea, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value&value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_and64(uint64_t *ls, uint64_t ea, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value&value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_compare_and_swap32(uint32_t *ls, uint64_t ea, uint32_t compare, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		if(old_value != compare) return old_value;
		
		ls[i] = value;
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return value;
}

uint64_t spu_atomic_compare_and_swap64(uint64_t *ls, uint64_t ea, uint64_t compare, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		if(old_value != compare) return old_value;
		
		ls[i] = value;
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return value;
}

uint32_t spu_atomic_decr32(uint32_t *ls, uint64_t ea)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value - 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_decr64(uint64_t *ls, uint64_t ea)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value - 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_incr32(uint32_t *ls, uint64_t ea)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value + 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_incr64(uint64_t *ls, uint64_t ea)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value + 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_nop32(uint32_t *ls, uint64_t ea)
{
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	mfc_getllar(ls, ea, 0, 0);
	mfc_read_atomic_status();
	spu_dsync();
	return ls[i];
}

uint64_t spu_atomic_nop64(uint64_t *ls, uint64_t ea)
{
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	mfc_getllar(ls, ea, 0, 0);
	mfc_read_atomic_status();
	spu_dsync();
	return ls[i];
}

uint32_t spu_atomic_or32(uint32_t *ls, uint64_t ea, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value|value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_or64(uint64_t *ls, uint64_t ea, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value|value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_store32(uint32_t *ls, uint64_t ea, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = value;
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_store64(uint64_t *ls, uint64_t ea, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = value;
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_sub32(uint32_t *ls, uint64_t ea, uint32_t value)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value - value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_sub64(uint64_t *ls, uint64_t ea, uint64_t value)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		ls[i] = (old_value - value);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint32_t spu_atomic_test_and_decr32(uint32_t *ls, uint64_t ea)
{
	uint32_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 2;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		if(old_value == 0) break;
		
		ls[i] = (old_value - 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}

uint64_t spu_atomic_test_and_decr64(uint64_t *ls, uint64_t ea)
{
	uint64_t old_value = 0;
	unsigned int i = ((uint32_t)ea & 0x7f) >> 3;

	ea &= ~0x7f;
	do {
		mfc_getllar(ls, ea, 0, 0);
		(void)mfc_read_atomic_status();
		spu_dsync();

		old_value = ls[i];
		if(old_value == 0) break;
		
		ls[i] = (old_value - 1);
		
		spu_dsync();
		mfc_putllc(ls, ea, 0, 0);
	} while(__builtin_expect(mfc_read_atomic_status(), 0));
	
	return old_value;
}
