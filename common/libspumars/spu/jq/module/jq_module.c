#include <spu_mfcio.h>

#include <mars/error.h>
#include <mars/module.h>

#include <sys/spu_printf.h>

#include "jq_module.h"

static struct mars_jq_context *jobQ;

extern void jq_exit(void);

static uint32_t get_ticks(void)
{
    return mars_module_get_ticks();
}

static struct mars_jq_context* get_jobQ(void)
{
    return (struct mars_jq_context*)mars_module_get_workload();
}

static struct mars_jq_module_syscalls jq_module_syscalls =
{
    get_ticks,
    get_jobQ
};

void __module_main(void)
{
    spu_printf("jq module main\n");

    jobQ = get_jobQ();

    spu_printf("jq workload id: %04x\n", jobQ->id.workload_id);

    jq_exit();
}
