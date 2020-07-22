#include <stdio.h>
#include <ppu-asm.h>

#include <http/https.h>


extern s32 httpClientSetSslCallbackEx(httpClientId cid, opd32 *opd, void *userArg);


s32 httpClientSetSslCallback(httpClientId cid, httpsSslCallback cb, void *arg)
{
    printf ( "IN: httpClientSetSslCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetSslCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetSslCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

