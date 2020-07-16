#ifndef __EXPORTS_H__
#define __EXPORTS_H__

EXPORT(jpgDecCreate, 0xa7978f59);
EXPORT(jpgDecDestroy, 0xd8ea91f8);
EXPORT(jpgDecOpen, 0x976ca5c2);
EXPORT(jpgDecClose, 0x9338a07a);
EXPORT(jpgDecReadHeader, 0x6d9ebccf);
EXPORT(jpgDecDecodeData, 0xaf8bb012);
EXPORT(jpgDecSetParameter, 0xe08f3910);

EXPORT(jpgDecExtSetParameter, 0x65cbbb16);
EXPORT(jpgDecExtDecodeData, 0x716f8792);
EXPORT(jpgDecExtOpen, 0xa9f703e3);
EXPORT(jpgDecExtReadHeader, 0xb91eb3d2);

#endif
