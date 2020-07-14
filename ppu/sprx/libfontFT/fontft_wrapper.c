#include <ppu-asm.h>
#include <ppu-types.h>

#include <font/font.h>
#include <font/fontFT.h>

extern s32 fontInitLibraryFreeTypeWithRevision(u64 revisionFlags,fontLibraryConfigFT *config,const fontLibrary* ATTRIBUTE_PRXPTR *lib);

void fontFTGetStubRevisionFlags(u64 *revisionFlags)
{
	if(revisionFlags == NULL) return;
	*revisionFlags = 0x14;
}

s32 fontInitLibraryFreeType(fontLibraryConfigFT *config,const fontLibrary **lib)
{
	s32 ret;
	u64 revisionFlags = 0LL;
	const fontLibrary *l ATTRIBUTE_PRXPTR;
	
	if(config == NULL || lib == NULL) return 0x80540002;
	
	fontFTGetStubRevisionFlags(&revisionFlags);
	
	ret = fontInitLibraryFreeTypeWithRevision(revisionFlags,config,&l);
	*lib = ret == 0 ? l : NULL;
	
	return ret;
}
