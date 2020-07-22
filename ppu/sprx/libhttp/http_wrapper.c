#include <stdio.h>
#include <ppu-asm.h>

#include <http/http.h>


extern s32 httpClientSetAuthenticationCallbackEx(httpClientId cid, opd32 *opd, void *arg);
extern s32 httpClientSetTransactionStateCallbackEx(httpClientId cid, opd32 *opd, void *arg);
extern s32 httpClientSetRedirectCallbackEx(httpClientId cid, opd32 *opd, void *arg);
extern s32 httpClientSetCookieSendCallbackEx(httpClientId cid, opd32 *opd, void *arg);
extern s32 httpClientSetCookieRecvCallbackEx(httpClientId cid, opd32 *opd, void *arg);


s32 httpClientSetAuthenticationCallback(httpClientId cid,httpAuthenticationCallback cb,void *arg)
{
    printf ( "IN: httpClientSetAuthenticationStateCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetAuthenticationStateCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetAuthenticationCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

s32 httpClientSetTransactionStateCallback(httpClientId cid,httpTransactionStateCallback cb,void *arg)
{
    printf ( "IN: httpClientSetTransactionStateCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetTransactionStateCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetTransactionStateCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

s32 httpClientSetRedirectCallback(httpClientId cid,httpRedirectCallback cb,void *arg)
{
    printf ( "IN: httpClientSetRedirectCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetRedirectCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetRedirectCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

s32 httpClientSetCookieSendCallback(httpClientId cid, httpCookieSendCallback cb, void *arg)
{
    printf ( "IN: httpClientSetCookieSendCallback(%d, %p, %p)\n", cid, cb, arg ) ;
    printf ( "OUT: httpClientSetCookieSendCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetCookieSendCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

s32 httpClientSetCookieRecvCallback(httpClientId cid, httpCookieRecvCallback cb, void *arg)
{
    printf ( "IN: httpClientSetCookieRecvCallback(%d, %p, %p)\n", cid, cb, arg ) ;
    printf ( "OUT: httpClientSetCookieRecvCallbackEx(%d, %p, %p)\n", cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg) ;
    return httpClientSetCookieRecvCallbackEx(cid, (cb != NULL ? (opd32*)__get_opd32(cb) : NULL), arg);
}

