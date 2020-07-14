#include <ppu-asm.h>
#include <ppu-types.h>

#include <font/font.h>

extern s32 fontGetLibraryEx(font *f,const fontLibrary* ATTRIBUTE_PRXPTR *lib,u32 *type);
extern s32 fontGetBindingRendererEx(font *f,fontRenderer* ATTRIBUTE_PRXPTR *renderer);
extern s32 fontGenerateCharGlyphEx(font *f,u32 code,fontGlyph* ATTRIBUTE_PRXPTR *glyph);
extern s32 fontGenerateCharGlyphVerticalEx(font *f,u32 code,fontGlyph* ATTRIBUTE_PRXPTR *glyph);

void fontGetStubRevisionFlags(u64 *revisionFlags)
{
	if(revisionFlags == NULL) return;
	*revisionFlags = 0x14;
}

s32 fontGetLibrary(font *f,const fontLibrary **lib,u32 *type)
{
	s32 ret;
	const fontLibrary *l ATTRIBUTE_PRXPTR;
	
	if(f == NULL || lib == NULL || type == NULL) return 0x80540002;

	ret = fontGetLibraryEx(f,&l,type);
	*lib = ret == 0 ? l : NULL;
	
	return ret;	 
}

s32 fontGetBindingRenderer(font *f,fontRenderer **renderer)
{
	s32 ret;
	fontRenderer *r ATTRIBUTE_PRXPTR;
	
	if(f == NULL || renderer == NULL) return 0x80540002;
	
	ret = fontGetBindingRendererEx(f,&r);
	*renderer = ret == 0 ? r : NULL;
	
	return ret;
}

s32 fontGenerateCharGlyph(font *f,u32 code,fontGlyph **glyph)
{
	s32 ret;
	fontGlyph *g ATTRIBUTE_PRXPTR;
	
	if(f == NULL || glyph == NULL) return 0x80540002;
	
	ret = fontGenerateCharGlyphEx(f,code,&g);
	*glyph = ret == 0 ? g : NULL;
	
	return ret;
}

s32 fontGenerateCharGlyphVertical(font *f,u32 code,fontGlyph **glyph)
{
	s32 ret;
	fontGlyph *g ATTRIBUTE_PRXPTR;
	
	if(f == NULL || glyph == NULL) return 0x80540002;
	
	ret = fontGenerateCharGlyphVerticalEx(f,code,&g);
	*glyph = ret == 0 ? g : NULL;
	
	return ret;
}



