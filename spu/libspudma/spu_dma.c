#include "spu_dma.h"

void spu_dma_large_cmd(uintptr_t ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t cmd)
{
	vec_uint4 tmp;
	while(size) {
		tmp = spu_sel((vec_uint4)si_from_uint(1), (vec_uint4)si_from_uint(2), spu_cmpgt((vec_uint4)si_from_uint(size), 1));
		tmp = spu_sel((vec_uint4)si_from_uint(4), tmp, spu_cmpeq(spu_cmpgt((vec_uint4)si_from_uint(size), 3), 0));
		tmp = spu_sel((vec_uint4)si_from_uint(8), tmp, spu_cmpeq(spu_cmpgt((vec_uint4)si_from_uint(size), 7), 0));
		tmp = spu_sel(spu_and((vec_uint4)si_from_uint(size), -16), tmp, spu_cmpeq(spu_cmpgt((vec_uint4)si_from_uint(size), 15), 0));
		tmp = spu_sel((vec_uint4)si_from_uint(MAX_DMA_BLOCK_SIZE), tmp, spu_cmpeq(spu_cmpgt((vec_uint4)si_from_uint(size), 16383), 0));
		
		uint32_t chunk = spu_extract(tmp, 0);
		spu_mfcdma64((volatile void*)ls,mfc_ea2h(ea),mfc_ea2l(ea),chunk,tag,cmd);

		size -= chunk;
		ls = (uintptr_t)((uint32_t)ls + chunk);
		ea += chunk;
	}
}

void spu_dma_and_wait(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t cmd)
{
	spu_mfcdma64((volatile void*)ls,mfc_ea2h(ea),mfc_ea2l(ea),size,tag,cmd);
	mfc_write_tag_mask(1<<tag);
	spu_mfcstat(MFC_TAG_UPDATE_ALL);
}

