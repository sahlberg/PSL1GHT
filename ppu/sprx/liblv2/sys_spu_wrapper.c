#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ppu-asm.h>
#include <sys/spu.h>

s32 sysSpuImageOpenELF(sysSpuImage* image, const char* path)
{
	s32 ret = -1;
	FILE *fd = NULL;
	size_t fsize = 0;
	void *data = NULL;

	fd = fopen(path, "rb");
	if(fd != NULL) {
		fseek(fd,0,SEEK_END);
		fsize = ftell(fd);
		fseek(fd,0,SEEK_SET);
		
		data = malloc(fsize);
		if(data != NULL) {
			fread(data,1,fsize,fd);
			ret = sysSpuImageImport(image,data,SPU_IMAGE_PROTECT);
			free(data);
		}
		fclose(fd);
	}
	
	return ret;
}
