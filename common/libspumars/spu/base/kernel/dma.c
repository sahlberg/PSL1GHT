#include <stdint.h>
#include <spu_mfcio.h>

#include <mars/error.h>

#include "kernel_internal_types.h"

#define MARS_DMA_TAG_MAX				31
#define MARS_DMA_SIZE_MAX				16384
#define MARS_DMA_ALIGN_MASK				0xf

static int dma_large(void *ls, uint64_t ea, uint32_t size, uint32_t tag, unsigned int dma_cmd)
{
	unsigned int cmd = MFC_CMD_WORD(0, 0, dma_cmd);

	if (tag > MARS_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;
	if (((uintptr_t)ls & MARS_DMA_ALIGN_MASK) ||
	    ((uintptr_t)ea & MARS_DMA_ALIGN_MASK))
		return MARS_ERROR_ALIGN;

	while (size) {
		unsigned int block_size;

		block_size = (size < MARS_DMA_SIZE_MAX) ?
			      size : MARS_DMA_SIZE_MAX;

		spu_mfcdma64((volatile void *)ls, mfc_ea2h(ea), mfc_ea2l(ea),
			     block_size, tag, cmd);

		ls += block_size;
		ea += block_size;
		size -= block_size;
	}

	return MARS_SUCCESS;
}

int dma_get(void *ls, uint64_t ea, uint32_t size, uint32_t tag)
{
	return dma_large(ls, ea, size, tag, MFC_GET_CMD);
}

int dma_put(const void *ls, uint64_t ea, uint32_t size, uint32_t tag)
{
	return dma_large((void *)ls, ea, size, tag, MFC_PUT_CMD);
}

int dma_wait(uint32_t tag)
{
	if (tag > MARS_DMA_TAG_MAX)
		return MARS_ERROR_PARAMS;

	mfc_write_tag_mask(1 << tag);
	mfc_write_tag_update_all();
	mfc_read_tag_status();

	return MARS_SUCCESS;
}
