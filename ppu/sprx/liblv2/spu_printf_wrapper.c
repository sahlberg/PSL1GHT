#include <stdio.h>
#include <errno.h>
#include <ppu-asm.h>
#include <sys/spu.h>
#include <sys/thread.h>
#include <sys/event_queue.h>
#include <sys/spu_thread_printf.h>

#define	SPU_PORT_PRINTF			1
#define MAX_QUEUE_SIZE			127
#define	TERMINATING_PORT_NAME	0xFEE1DEAD

static u32 spu_printf_initialized = 0;

static sys_event_queue_t eventQ;
static sys_ppu_thread_t	spu_printf_handler;
static sys_event_port_t	terminating_port;

static void spu_printf_handler_entry(void *arg)
{
	int ret;
	
	(void)arg;
	
	for(;;) {
		sys_event_t event;
		sys_spu_thread_t thread_id;
		
		ret = sysEventQueueReceive(eventQ, &event, 0);

		
		if (event.source == TERMINATING_PORT_NAME) {
			sysThreadExit(0);
		}
		thread_id = event.data_1;
		int sret = spu_thread_printf(thread_id, event.data_3);
		ret = sysSpuThreadWriteMb(thread_id, sret);
		if (ret) {
			sysThreadExit(-1);
		}
	}
	
	sysThreadExit(0);
}
	
s32 sysSpuPrintfInitialize(int prio, void (*entry)(void*))
{
	s32 ret;
	void (*run_entry)(void*) = entry;
	sys_event_queue_attr_t evQAttr = { SYS_EVENT_QUEUE_PRIO, SYS_EVENT_QUEUE_PPU, "" };
	
	if(spu_printf_initialized)
		return 0;
		
	if(run_entry == NULL)
		run_entry = spu_printf_handler_entry;

	ret = sysEventQueueCreate(&eventQ, &evQAttr, SYS_EVENT_QUEUE_KEY_LOCAL, MAX_QUEUE_SIZE);
	if (ret) {
		return ret;
	}
	
	ret = sysThreadCreate(&spu_printf_handler, run_entry, &eventQ, 200, 4096, THREAD_JOINABLE, "spu_printf_handler");
	if (ret) {
		return ret;
	}
	
	ret = sysEventPortCreate(&terminating_port, SYS_EVENT_PORT_LOCAL, TERMINATING_PORT_NAME);
	if (ret) {
		return ret;
	}
	
	ret = sysEventPortConnectLocal(terminating_port, eventQ);
	if (ret) {
		return ret;
	}
	
	spu_printf_initialized = 1;
	return 0;
}

s32 sysSpuPrintfAttachThread(sys_spu_thread_t thread)
{
	int ret;
	
	if(!spu_printf_initialized)
		return -1;
		
	/*attach event_port for spu_printf to thread*/
	ret = sysSpuThreadConnectEvent(thread, eventQ, SPU_THREAD_EVENT_USER, SPU_PORT_PRINTF);
	if (ret) {
		return ret;
	}
	return 0;
}

s32 sysSpuPrintfDetachThread(sys_spu_thread_t thread)
{
	s32 ret;

	if(!spu_printf_initialized)
		return -1;

	/*dettach event_port for spu_printf from thread*/
	ret = sysSpuThreadDisconnectEvent(thread, SPU_THREAD_EVENT_USER, SPU_PORT_PRINTF);
	if (ret) {
		return ret;
	}
	return 0;
}

s32 sysSpuPrintfAttachGroup(sys_spu_group_t group)
{
	int ret;
	
	if(!spu_printf_initialized)
		return -1;

	ret = sysSpuThreadGroupConnectEvent(group, eventQ, SPU_THREAD_EVENT_USER);
	if (ret) {
		return ret;
	}
	return 0;
}

s32 sysSpuPrintfDetachGroup(sys_spu_group_t group)
{
	int ret;
	
	if(!spu_printf_initialized)
		return -1;

	ret = sysSpuThreadGroupDisconnectEvent(group, SPU_THREAD_EVENT_USER);
	if (ret) {
		return ret;
	}
	return 0;
}

s32 sysSpuPrintfFinalize()
{
	s32 ret;
	u64	exit_code;

	if(!spu_printf_initialized)
		return 0;

	/*
	 * send event for temination.
	 */
	ret = sysEventPortSend(terminating_port, 0, 0, 0);
	if (ret) {
		return ret;
	}
	/*	wait for termination of the handler thread */
	ret = sysThreadJoin(spu_printf_handler, &exit_code);
	if (ret) {
		return ret;
	}

	/* Disconnect and destroy the terminating port */
	ret = sysEventPortDisconnect(terminating_port);
	if (ret) {
		return ret;
	}
	ret = sysEventPortDestroy(terminating_port);
	if (ret) {
		return ret;
	}	

	/*	clean event_queue for spu_printf */
	ret = sysEventQueueDestroy(eventQ, 0);
	if (ret) {
		return ret;
	}

	spu_printf_initialized = 0;
	return 0;
}
