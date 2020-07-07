#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

all:
	@$(MAKE) -C spu --no-print-directory
	@$(MAKE) -C ppu --no-print-directory
	
install:
	@$(MAKE) -C spu install --no-print-directory
	@$(MAKE) -C ppu install --no-print-directory
	
clean:
	@$(MAKE) -C spu clean --no-print-directory
	@$(MAKE) -C ppu clean --no-print-directory
	
.PHONY: all clean install
