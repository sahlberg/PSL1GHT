#include <ppu-asm.h>
#include <rsx/gcm_sys.h>

extern void gcmSetVBlankHandlerEx(opd32 *opd);
extern void gcmSetFlipHandlerEx(opd32 *opd);
extern void gcmSetGraphicsHandlerEx(opd32 *opd);
extern void gcmSetSecondVHandlerEx(opd32 *opd);
extern void gcmSetUserHandlerEx(opd32 *opd);
extern void gcmSetQueueHandlerEx(opd32 *opd);
extern void gcmSetUserCommandEx(opd32 *opd);
extern s32 gcmIoOffsetToAddressEx(u32 offset,void* ATTRIBUTE_PRXPTR *address);
extern s32 gcmInitBodyEx(gcmContextData* ATTRIBUTE_PRXPTR *ctx,const u32 cmdSize,const u32 ioSize,const void *ioAddress);

s32 gcmInitBody(gcmContextData **ctx,const u32 cmdSize,const u32 ioSize,const void *ioAddress)
{
	s32 ret;
	gcmContextData *context ATTRIBUTE_PRXPTR;
	
	if(ctx == NULL) return -1;
	
	ret = gcmInitBodyEx(&context,cmdSize,ioSize,ioAddress);
	*ctx = ret == 0 ? context : NULL;
	
	return ret;
}

s32 gcmIoOffsetToAddress(u32 offset,void **address)
{
	s32 ret;
	void *addr ATTRIBUTE_PRXPTR;
	
	if(address == NULL) return -1;
	
	ret = gcmIoOffsetToAddressEx(offset,&addr);
	*address = ret == 0 ? addr : NULL;
	
	return ret;
}

void gcmSetVBlankHandler(void (*handler)(const u32 head))
{
	gcmSetVBlankHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetFlipHandler(void (*handler)(const u32 head))
{
	gcmSetFlipHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetGraphicsHandler(void (*handler)(const u32 val))
{
	gcmSetGraphicsHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetSecondVHandler(void (*handler)(const u32 head))
{
	gcmSetSecondVHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetUserHandler(void (*handler)(const u32 cause))
{
	gcmSetUserHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetQueueHandler(void (*handler)(const u32 head))
{
	gcmSetQueueHandlerEx((opd32*)__get_opd32(handler));
}

void gcmSetUserCommand(void (*handler)(const u32 cause))
{
	gcmSetUserCommandEx((opd32*)__get_opd32(handler));
}
