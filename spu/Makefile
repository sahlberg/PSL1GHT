#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

all:
	@$(MAKE) -C libsputhread --no-print-directory
	@$(MAKE) -C libspudma --no-print-directory
	@$(MAKE) -C libspuatomic --no-print-directory

install-headers:
	@[ -d $(PSL1GHT)/spu ] || mkdir -p $(PSL1GHT)/spu
	@cp -frv include $(PSL1GHT)/spu

install: install-headers
	@$(MAKE) -C libsputhread install --no-print-directory
	@$(MAKE) -C libspudma install --no-print-directory
	@$(MAKE) -C libspuatomic install --no-print-directory

clean:
	@$(MAKE) -C libsputhread clean --no-print-directory
	@$(MAKE) -C libspudma clean --no-print-directory
	@$(MAKE) -C libspuatomic clean --no-print-directory

.PHONY: all clean install
