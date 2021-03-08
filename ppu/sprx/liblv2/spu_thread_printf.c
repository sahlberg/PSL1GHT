#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ppu-asm.h>
#include <sys/spu.h>

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define is_digit(c)	((c) >= '0' && (c) <= '9')

#define __DOUTBUFSIZE			256

#define do_div(n,base) \
({ \
	s64 _res; \
	_res = ((u64)n)%(unsigned)base; \
	n = ((u64)n)/(unsigned)base; \
	_res; \
})

#define GET_CHAR(pos) \
({ \
	u64 _rval; \
	sysSpuThreadReadLocalStorage(id, pos, &_rval, sizeof(char)); \
	(char)_rval; \
})

#define GET_ARG(pos, type) \
({ \
	u64 _rval; \
	sysSpuThreadReadLocalStorage(id, arg_addr + (1 + pos)*16, &_rval, sizeof(type)); \
	(type)_rval; \
})

typedef struct
{
	u32 sign : 1;
	u32 exponent : 11;
	u32 mantissah : 20;
	u32 mantissal : 32;
} double_t;

static char __outstr[__DOUTBUFSIZE] __attribute__((aligned(16)));

static int _isinf(double __x)
{
	union
	{
		double *__x;
		double_t *x;
	} x;
	x.__x = &__x;
	return (x.x->exponent == 0x7ff && x.x->mantissah == 0 && x.x->mantissal == 0);
}

static int _isnan(double __x)
{
	union
	{
		double *__x;
		double_t *x;
	} x;
	x.__x = &__x;
	return (x.x->exponent == 0x7ff && (x.x->mantissah != 0 || x.x->mantissal != 0));
}

static int skip_atoi(sys_spu_thread_t id, u64 *fmt_addr) 
{
	char c;
	int i = 0;

	while(is_digit((c=GET_CHAR(*fmt_addr)))) {
		i = i*10 + c - '0';
		(*fmt_addr)++;
	}
		
	return i;
}

static int __strnlen(sys_spu_thread_t id, u32 arg_addr, s32 maxlen)
{
	char c;
	int len = 0;

	for(;;) {
		c = GET_CHAR(arg_addr++);
		if(c == 0) break;
		if(maxlen > -1 && len >= maxlen) break;
		len++;
	}	
	return len;
}

static char* print_string(sys_spu_thread_t id, char *str, u32 arg_addr, s32 field_width, s32 precision, u32 flags)
{
	int i, len;
	const char *nil = "<NULL>";
	
	if(arg_addr == 0) {
		len = strnlen(nil, precision);
		
		if(!(flags&LEFT))
			while(len < field_width--)
				*str++ = ' ';
		for(i=0;i < len;i++)
			*str++ = *nil++;
		while(len < field_width--)
			*str++ = ' ';
		return str;
	}
	
	len = __strnlen(id, arg_addr, precision);
	if(!(flags&LEFT))
		while(len < field_width--)
			*str++ = ' ';
	for(i=0;i < len;i++)
		*str++ = GET_CHAR(arg_addr++);
	while(len < field_width--)
		*str++ = ' ';
	return str;
}

static char* number(char *str, s64 num, s32 base, s32 size, s32 precision, s32 type)
{
	int i;
	char c,sign,tmp[66];
	const char *digits;
	const char *small_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	const char *large_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	digits = (type&LARGE) ? large_digits : small_digits;

	if(type&LEFT)
		type &= ~ZEROPAD;
	if(base < 2 || base > 36)
		return NULL;
		
	sign = 0;
	c = (type&ZEROPAD) ? '0' : ' ';
	if(type&SIGN) {
		if(num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if(type&PLUS) {
			sign = '+';
			size--;
		} else if(type&SPACE) {
			sign = ' ';
			size--;
		}
	}
	if(type&SPECIAL) {
		if(base == 16)
			size -= 2;
		else if(base == 8)
			size--;
	}
	
	i = 0;
	if(num == 0)
		tmp[i++] = '0';
	else while(num != 0)
		tmp[i++] = digits[do_div(num,base)];
	
	if(i > precision)
		precision = i;
	size -= precision;
		
	if(!(type&(ZEROPAD + LEFT)))
		while(size-- > 0)
			*str++ = ' ';
		
	if(sign)
		*str++ = sign;
	
	if(type&SPECIAL) {
		if(base == 8)
			*str++ = '0';
		else if(base == 16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}
	
	if(!(type&LEFT))
		while(size-- > 0)
			*str++ = c;
	while(i < precision--)
		*str++ = '0';
	while(i-- > 0)
		*str++ = tmp[i];
	while(size-- > 0)
		*str++ = ' ';
		
	return str;
}

static char* number_double(char *str, f64 num, s32 size, s32 precision, s32 type)
{	
	int i;
	char c,sign,buf[66];
	const char *ss;
	const char *digits;
	const char *small_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	const char *large_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static const f64 pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

	digits = (type&LARGE) ? large_digits : small_digits;

	if(_isnan(num)) {
		ss = "Nan";
		for (i=0;i < 3;i++)
			*str++ = *ss++;
		return str;
	} else if (_isinf(num) < 0) {
		ss = "-Inf";
		for (i=0;i < 4;i++)
			*str++ = *ss++;
		return str;
	} else if (_isinf(num) > 0) {
		ss = "+Inf";
		for (i=0;i < 4;i++)
			*str++ = *ss++;
		return str;
	}

	sign = 0;
	c = (type&ZEROPAD) ? '0' : ' ';
	if(type&SIGN) {
		if(num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if(type&PLUS) {
			sign = '+';
			size--;
		} else if(type&SPACE) {
			sign = ' ';
			size--;
		}
	}

	if (precision == -1)
		precision = 6;

	i = 0;
	while (precision > 9) {
		buf[i++] = '0';
		precision--;
	}

	s32 whole = (s32)num;
	f64 tmp = (num - whole)*pow10[precision];
	u64 frac = (u64)tmp;
	f64 diff = tmp - frac;

	if (diff > 0.5) {
		frac++;

		if (frac >= pow10[precision]) {
			frac = 0;
			whole++;
		}
	} else if (diff < 0.5){
	} else if (frac == 0 || frac&1)
		frac++;

	if (precision == 0) {
		diff = num - (f64)whole;
		
		if ((!(diff < 0.5) || (diff > 0.5)) && whole&1)
			whole++;
	} else {
		u32 count = precision;
		do {
			count--;
			buf[i++] = digits[do_div(frac, 10)];
		} while (frac);

		while (count-- > 0)
			buf[i++] = '0';
		
		buf[i++] = '.';
	}

	do {
		buf[i++] = digits[do_div(whole, 10)];
	} while (whole);

	size -= precision;
		
	if(!(type&(ZEROPAD + LEFT)))
		while(size-- > 0)
			*str++ = ' ';
		
	if(sign)
		*str++ = sign;
	
	if(!(type&LEFT))
		while(size-- > 0)
			*str++ = c;
	while(i < precision--)
		*str++ = '0';
	while(i-- > 0)
		*str++ = buf[i];
	while(size-- > 0)
		*str++ = ' ';
		
	return str;
}

s32 spu_thread_sprintf(char *buf, sys_spu_thread_t id, u32 arg_addr)
{
	u64 num;
	s32 base;
	u32 flags;
	char *str;
	u32 arg_pos;
	u64 fmt_addr;
	s32 qualifier;
	s32 precision;
	s32 field_width;
	ieee64 v;
	
	sysSpuThreadReadLocalStorage(id, arg_addr, &fmt_addr, sizeof(u32));
	
	arg_pos = 0;
	for(str=buf;;fmt_addr++) {
		char c;

		c = GET_CHAR(fmt_addr);		
		if(c == 0) break;
				
		if(c != '%') {
			*str++ = c;
			continue;
		}
		
		flags = 0;
repeat:
		++fmt_addr;
		switch(GET_CHAR(fmt_addr)) {
			case '-': flags |= LEFT; goto repeat;
			case '+': flags |= PLUS; goto repeat;
			case ' ': flags |= SPACE; goto repeat;
			case '#': flags |= SPECIAL; goto repeat;
			case '0': flags |= ZEROPAD; goto repeat;
		}
		
		field_width = -1;
		c = GET_CHAR(fmt_addr);
		if(is_digit(c))
			field_width = skip_atoi(id, &fmt_addr);
		else if(c == '*') {
			++fmt_addr;
			
			field_width = GET_ARG(arg_pos++, int);
			if(field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}
		
		precision = -1;
		c = GET_CHAR(fmt_addr);
		if(c == '.') {
			++fmt_addr;
			c = GET_CHAR(fmt_addr);
			if(is_digit(c)) {
				precision = skip_atoi(id, &fmt_addr);
			} else if(c == '*') {
				++fmt_addr;
				precision = GET_ARG(arg_pos++, int);
			}
		}
		
		qualifier = -1;
		c = GET_CHAR(fmt_addr);
		if(c == 'h' || c == 'l' || c == 'L') {
			qualifier = c;
			++fmt_addr;
		}
		
		base = 10;
		c = GET_CHAR(fmt_addr);
		switch(c) {
			case 'c':
				if(!(flags&LEFT))
					while(--field_width>0)
						*str++ = ' ';
				*str++ = GET_ARG(arg_pos++, char);
				while(--field_width>0)
					*str++ = ' ';
				continue; 
			case 's':
				str = print_string(id, str, GET_ARG(arg_pos++, u64), field_width, precision, flags);
				continue;
			case '%':
				*str++ = '%';
				continue;
			case 'o':
				base = 8;
				break;
			case 'X':
				flags |= LARGE;
			case 'x':
				base = 16;
				break;
			case 'd':
			case 'i':
				flags |= SIGN;
			case 'u':
				break;
			case 'f':
				v.u = GET_ARG(arg_pos++, u64);
				str = number_double(str, v.d, field_width, precision, flags);
				continue;
			default:
				*str++ = '%';
				if(c)
					*str++ = c;
				else
					--fmt_addr;
				continue;
		}
		if(qualifier == 'l')
			num = GET_ARG(arg_pos++, u64);
		else if(qualifier == 'h') {
			num = GET_ARG(arg_pos++, u16);
			if(flags&SIGN)
				num = (s16)num;
		} else if(flags&SIGN)
			num = GET_ARG(arg_pos++, s32);
		else
			num = GET_ARG(arg_pos++, u32);
		str = number(str, num, base, field_width, precision, flags);
	}
	
	*str = '\0';
	return str - buf;
}

s32 spu_thread_printf(sys_spu_thread_t id, u32 arg_addr)
{
	int len;

	len = spu_thread_sprintf(__outstr, id, arg_addr);
	fwrite(__outstr, 1, len, stdout);

	return len;
}
