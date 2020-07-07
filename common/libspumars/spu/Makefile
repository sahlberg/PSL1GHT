#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include	$(PSL1GHT)/spu_rules

BUILD		:=	build

#---------------------------------------------------------------------------------
ifeq ($(strip $(PLATFORM)),)
#---------------------------------------------------------------------------------
export BASEDIR		:= $(CURDIR)
export DEPF			:= $(BASEDIR)/deps
export LIBF			:= $(BASEDIR)/lib

export LD			:=	$(CC)

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

export LIBDIR		:= $(LIBF)/$(PLATFORM)
export DEPSDIR		:= $(DEPF)/$(PLATFORM)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

LIBRARY			:= $(LIBDIR)/libspumars
KERNELMODULE	:= $(LIBF)/mars_kernel
TASKMODULE		:= $(LIBF)/mars_task_module

#---------------------------------------------------------------------------------
INCLUDES	:= -I$(BASEDIR) \
			   -I$(BASEDIR)/../include/common \
			   -I$(BASEDIR)/../include/spu \
			   -I$(BASEDIR)/../base/common \
			   -I$(BASEDIR)/../task/common \
			   -I$(BASEDIR)/../../../spu/include

LIBS 		:= -lsputhread
LIBPATHS	:= -L$(BASEDIR)/../../../spu/libsputhread/lib/spu

CFLAGS		:= -Wall -ffunction-sections -fdata-sections $(MACHDEP) -DLIBSPUMARS_INTERNAL $(INCLUDES)
ASFLAGS		:= $(MACHDEP) -D__ASSEMBLY__ $(INCLUDES)

#---------------------------------------------------------------------------------
VPATH :=	$(BASEDIR) \
			$(BASEDIR)/base/lib \
			$(BASEDIR)/base/kernel \
			$(BASEDIR)/task/lib \
			$(BASEDIR)/task/module

#---------------------------------------------------------------------------------
OBJS_LIB	:= module.o \
			   task.o task_barrier.o task_event_flag.o \
			   task_queue.o task_semaphore.o task_signal.o
			   
OBJS_KERNEL		:= kernel_crt.o switch.o kernel.o dma.o mutex.o

OBJS_MODULE		:= task_switch.o task_module.o
			
all: spu

#---------------------------------------------------------------------------------
spu:
#---------------------------------------------------------------------------------
	@[ -d $(LIBF)/spu ] || mkdir -p $(LIBF)/spu
	@[ -d $(DEPF)/spu ] || mkdir -p $(DEPF)/spu
	@[ -d spu ] || mkdir -p spu
	@$(MAKE) PLATFORM=spu lib -C spu -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
install-header:
#---------------------------------------------------------------------------------
	@[ -d $(PSL1GHT)/spu/include ] || mkdir -p $(PSL1GHT)/spu/include
	@cp -frv $(CURDIR)/../include/common/mars $(PSL1GHT)/spu/include
	@cp -frv $(CURDIR)/../include/spu/mars $(PSL1GHT)/spu/include

#---------------------------------------------------------------------------------
install: all install-header
#---------------------------------------------------------------------------------
	@[ -d $(PSL1GHT)/spu/lib ] || mkdir -p $(PSL1GHT)/spu/lib
	@cp -frv $(CURDIR)/lib/spu/*.a $(PSL1GHT)/spu/lib

#---------------------------------------------------------------------------------
$(LIBRARY).a: CFLAGS += -Os --param max-inline-insns-single=20
$(LIBRARY).a: $(OBJS_LIB)

$(KERNELMODULE).elf: CFLAGS += -Os -mfixed-range=80-127 -funroll-loops -fschedule-insns
$(KERNELMODULE).elf: $(OBJS_KERNEL)
	@echo linking ... $(notdir $@)
	$(VERB) $(LD)  $^ -nostdlib \
			   -Wl,--defsym=__stack=0x29f0 \
			   -Wl,-Map -Wl,$(CURDIR)/$(notdir $@).map -Wl,--cref \
			   -Wl,--gc-sections \
			   -Wl,--sort-common \
			   -Wl,--sort-section -Wl,alignment \
			   -Wl,-N \
			   $(LIBPATHS) $(LIBS) -o $@

$(TASKMODULE).elf: CFLAGS += -Os -mfixed-range=80-127 -funroll-loops -fschedule-insns
$(TASKMODULE).elf: $(OBJS_MODULE)
	@echo linking ... $(notdir $@)
	$(VERB) $(LD)  $^ -nodefaultlibs \
			   -Wl,--defsym=__stack=0x3fe0 -Wl,-Ttext-segment=0x2a00 \
			   -Wl,--entry,mars_module_entry -Wl,-u,mars_module_entry \
			   -Wl,-Map -Wl,$(CURDIR)/$(notdir $@).map -Wl,--cref \
			   -Wl,--gc-sections \
			   -Wl,--sort-common \
			   -Wl,--sort-section -Wl,alignment \
			   $(LIBPATHS) $(LIBS) -L$(LIBDIR) -lspumars -o $@

#---------------------------------------------------------------------------------

.PHONY: lib spu install

#---------------------------------------------------------------------------------
lib: $(LIBRARY).a $(KERNELMODULE).elf $(TASKMODULE).elf
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	@echo clean ...
	@rm -rf spu
	@rm -rf $(DEPF)
	@rm -rf $(LIBF)

-include $(DEPSDIR)/*.d
