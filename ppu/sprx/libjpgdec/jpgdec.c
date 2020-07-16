/* libjpg.c

Copyright (c) 2010 Hermes <www.elotrolado.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of
conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list
of conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.
- The names of the contributors may not be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <malloc.h>
#include <string.h>
#include <ppu-asm.h>

#include <jpgdec/jpgdec.h>

static void* jpg_malloc(u32 size,void *usrdata)
{
	return malloc(size);
}

static void jpg_free(void *ptr,void *usrdata)
{
	free(ptr);
}

static s32 decodeJPEG(jpgDecSource *src,jpgData *out)
{
	s32 mHandle,sHandle,ret;
	jpgDecInfo DecInfo;
	jpgDecInParam inParam;
	jpgDecOpnInfo openInfo;
	jpgDecOutParam outParam;
	jpgDecDataInfo DecDataInfo;
	jpgDecThreadInParam InThdParam;
	jpgDecThreadOutParam OutThdParam;
	jpgDecDataCtrlParam dataCtrlParam;

	InThdParam.spu_enable = JPGDEC_SPU_THREAD_DISABLE;
	InThdParam.ppu_prio = 512;
	InThdParam.spu_prio = 200;
	InThdParam.malloc_func = (jpgCbCtrlMalloc)__get_opd32(jpg_malloc);
	InThdParam.malloc_arg = NULL;
	InThdParam.free_func = (jpgCbCtrlFree)__get_opd32(jpg_free);
	InThdParam.free_arg = NULL;

	ret = jpgDecCreate(&mHandle,&InThdParam,&OutThdParam);

	out->bmp_out = NULL;
	if(ret==0) {
		ret = jpgDecOpen(mHandle,&sHandle,src,&openInfo);
		if(ret==0) {
			ret = jpgDecReadHeader(mHandle,sHandle,&DecInfo);
			if(ret==0 && DecInfo.color_space==0) ret = -1;

			if(ret==0) {
				inParam.cmd_ptr = NULL;
				inParam.down_scale = 1;
				inParam.quality_mode = JPGDEC_FAST;
				inParam.output_mode = JPGDEC_TOP_TO_BOTTOM;
				inParam.color_space = JPGDEC_ARGB;
				inParam.alpha = 0xff;

				ret = jpgDecSetParameter(mHandle,sHandle,&inParam,&outParam);
			}

			if(ret==0) {
				out->pitch = outParam.width*4;
				out->bmp_out = malloc(out->pitch*outParam.height);
				if(!out->bmp_out)
					ret = -1;
				else {
					memset(out->bmp_out,0,(out->pitch*outParam.height));

					dataCtrlParam.output_bytes_per_line = out->pitch;
					ret = jpgDecDecodeData(mHandle,sHandle,out->bmp_out,&dataCtrlParam,&DecDataInfo);
					if(ret==0 && DecDataInfo.decode_status==0) {
						out->width = outParam.width;
						out->height = outParam.height;

						ret = 0;
					}
				}
			}
			jpgDecClose(mHandle,sHandle);
		}
		if(ret && out->bmp_out) {
			free(out->bmp_out);
			out->bmp_out = NULL;
		}

		jpgDecDestroy(mHandle);
	}
	return ret;
}

s32 jpgLoadFromFile(const char *filename,jpgData *out)
{
	jpgDecSource source;

	memset(&source,0,sizeof(jpgDecSource));

	source.stream_sel = JPGDEC_FILE;
	source.file_name = filename;
	source.spu_enable = JPGDEC_SPU_THREAD_DISABLE;

	return decodeJPEG(&source,out);
}

s32 jpgLoadFromBuffer(const void *buffer,u32 size,jpgData *out)
{
	jpgDecSource source;

	memset(&source,0,sizeof(jpgDecSource));

	source.stream_sel = JPGDEC_BUFFER;
	source.stream_ptr = (void*)buffer;
	source.stream_size = size;
	source.spu_enable = JPGDEC_SPU_THREAD_DISABLE;

	return decodeJPEG(&source,out);
}
