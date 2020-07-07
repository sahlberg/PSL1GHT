#ifndef __MARS_CONTEXT_INTERNAL_H__
#define __MARS_CONTEXT_INTERNAL_H__

#include <ppu-types.h>
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/spu.h>
#include <sys/event_queue.h>

#define MARS_SHARED_CONTEXT_MAX				16

typedef struct {
	uint32_t num_spu;
	uint32_t spu_prio;
	uint32_t ppu_prio;
	sys_event_queue_t spu_ppu_event_queue;
	sys_ppu_thread_t ppu_handler_thread;
	sys_spu_group_t spu_thread_group;
	sys_spu_thread_t *spu_threads;
	sysSpuImage spu_image;
} mars_mpu_context_t;

typedef sys_mutex_t mars_host_mutex_t;
typedef sys_ppu_thread_t mars_callback_t;

struct mars_context {
	/* parameters for the MARS kernel */
	uint64_t kernel_params_ea;
	/* workload queue where workloads are added */
	uint64_t workload_queue_ea;
	/* callback queue where host callback requests are added */
	uint64_t callback_queue_ea;
	/* reference count */
	uint32_t reference_count;
	/* num of mpu context threads */
	uint32_t mpu_context_count;
	/* array of mpu contexts */
	mars_mpu_context_t *mpu_context;
	/* callback handler */
	mars_callback_t callback_handler;
};

int mars_mpu_max(uint32_t *num);
int mars_mpu_run(mars_mpu_context_t *mpu);
int mars_mpu_initialize(mars_mpu_context_t *mpu, uint64_t params_ea, uint32_t idx);
int mars_mpu_create(mars_mpu_context_t *mpu, uint32_t num_mpus, uint32_t spu_prio, uint32_t ppu_prio);
int mars_mpu_wait_all(mars_mpu_context_t *mpu);

int mars_workload_queue_create(struct mars_context *mars);
int mars_workload_queue_destroy(struct mars_context *mars);
int mars_workload_queue_exit(struct mars_context *mars);

int mars_callback_queue_create(struct mars_context *mars, uint32_t ppu_prio);
int mars_callback_queue_destroy(struct mars_context *mars);
int mars_callback_queue_exit(struct mars_context *mars);

int mars_host_mutex_lock(mars_host_mutex_t *mutex);
int mars_host_mutex_unlock(mars_host_mutex_t *mutex);

extern mars_host_mutex_t mars_shared_context_lock;

#endif
