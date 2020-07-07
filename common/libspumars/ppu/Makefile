#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include	$(PSL1GHT)/ppu_rules

BUILD		:=	build

#---------------------------------------------------------------------------------
ifeq ($(strip $(PLATFORM)),)
#---------------------------------------------------------------------------------
export BASEDIR		:= $(CURDIR)
export DEPS			:= $(BASEDIR)/deps
export LIBS			:= $(BASEDIR)/lib

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

export LIBDIR		:= $(LIBS)/$(PLATFORM)
export DEPSDIR		:= $(DEPS)/$(PLATFORM)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

LIBRARY				:= $(LIBDIR)/libspumars

KERNELMODULE		:= $(BASEDIR)/../spu/lib/mars_kernel.elf
TASKMODULE			:= $(BASEDIR)/../spu/lib/mars_task_module.elf

#---------------------------------------------------------------------------------
INCLUDES	:= -I$(BASEDIR) \
			   -I$(BASEDIR)/../include/common \
			   -I$(BASEDIR)/../include/ppu \
			   -I$(BASEDIR)/../base/common \
			   -I$(BASEDIR)/../task/common \
			   -I$(BASEDIR)/../../../ppu/include

CFLAGS		:= -O2 -mregnames -Wall -mcpu=cell $(MACHDEP) -DLIBSPUMARS_INTERNAL $(INCLUDES) -Wa,-mcell
ASFLAGS		:=	$(MACHDEP) -mregnames -mcpu=cell -D__ASSEMBLY__ -Wa,-mcell $(INCLUDES)

#---------------------------------------------------------------------------------
VPATH :=	$(BASEDIR) \
			$(BASEDIR)/base/lib \
			$(BASEDIR)/task/lib

#---------------------------------------------------------------------------------
OBJS		:= alloc.o ea_cell.o workload_queue.o context.o \
			   mpu_cell.o mutex_cell.o cond_cell.o callback_cell.o \
			   host_mutex_psl1ght.o mars_kernel.o \
			   task.o task_signal.o task_semaphore.o task_queue.o \
			   task_event_flag.o task_barrier.o mars_task_module.o
			
all: ppu

#---------------------------------------------------------------------------------
ppu:
#---------------------------------------------------------------------------------
	@[ -d $(LIBS)/ppu ] || mkdir -p $(LIBS)/ppu
	@[ -d $(DEPS)/ppu ] || mkdir -p $(DEPS)/ppu
	@[ -d ppu ] || mkdir -p ppu
	@$(MAKE) PLATFORM=ppu lib -C ppu -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
install-header:
#---------------------------------------------------------------------------------
	@[ -d $(PSL1GHT)/ppu/include ] || mkdir -p $(PSL1GHT)/ppu/include
	@cp -frv $(CURDIR)/../include/common/mars $(PSL1GHT)/ppu/include
	@cp -frv $(CURDIR)/../include/ppu/mars $(PSL1GHT)/ppu/include

#---------------------------------------------------------------------------------
install: all install-header
#---------------------------------------------------------------------------------
	@[ -d $(PSL1GHT)/ppu/lib ] || mkdir -p $(PSL1GHT)/ppu/lib
	@cp -frv $(CURDIR)/lib/ppu/*.a $(PSL1GHT)/ppu/lib

#---------------------------------------------------------------------------------
$(LIBRARY).a: $(OBJS)

mars_kernel.c: $(KERNELMODULE)
	@echo generating ... $(notdir $@)
	@echo '__attribute__((aligned(128))) const unsigned char mars_kernel_entry[] = {' > $@.tmp
	@od -v -A n -t x1 $(KERNELMODULE) | \
		sed -e '/^$$/d' -e 's/  */,0x/g' -e 's/^,//' -e 's/ *$$/,/' -e 's/,0x,$$//' >> $@.tmp
	@echo '};' >> $@.tmp
	@mv -f $@.tmp $@
	
mars_task_module.c: $(TASKMODULE)
	@echo generating ... $(notdir $@)
	@echo '__attribute__((aligned(128))) const unsigned char mars_task_module_entry[] = {' > $@.tmp
	@od -v -A n -t x1 $(TASKMODULE) | \
		sed -e '/^$$/d' -e 's/  */,0x/g' -e 's/^,//' -e 's/ *$$/,/' -e 's/,0x,$$//' >> $@.tmp
	@echo '};' >> $@.tmp
	@mv -f $@.tmp $@
	
#---------------------------------------------------------------------------------

.PHONY: lib ppu install

#---------------------------------------------------------------------------------
lib: mars_kernel.c mars_task_module.c $(LIBRARY).a
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	@echo clean ...
	@rm -rf ppu
	@rm -rf $(DEPS)
	@rm -rf $(LIBS)

-include $(DEPSDIR)/*.d
