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
    printf ( "OUT: httpClientSetAuthenticationStateCallbackEx(%d, %p, %p)\n", cid, (opd32*)__get_opd32(cb), arg) ;
    return httpClientSetAuthenticationCallbackEx(cid, (opd32*)__get_opd32(cb), arg);
}

s32 httpClientSetTransactionStateCallback(httpClientId cid,httpTransactionStateCallback cb,void *arg)
{
    printf ( "IN: httpClientSetTransactionStateCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetTransactionStateCallbackEx(%d, %p, %p)\n", cid, (opd32*)__get_opd32(cb), arg) ;
    return httpClientSetTransactionStateCallbackEx(cid, (opd32*)__get_opd32(cb), arg);
}

s32 httpClientSetRedirectCallback(httpClientId cid,httpRedirectCallback cb,void *arg)
{
    printf ( "IN: httpClientSetRedirectCallback(%d, %p, %p)\n", cid, cb, arg) ;
    printf ( "OUT: httpClientSetRedirectCallbackEx(%d, %p, %p)\n", cid, (opd32*)__get_opd32(cb), arg) ;
    return httpClientSetRedirectCallbackEx(cid, (opd32*)__get_opd32(cb), arg);
}

s32 httpClientSetCookieSendCallback(httpClientId cid, httpCookieSendCallback cb, void *arg)
{
    printf ( "IN: httpClientSetCookieSendCallback(%d, %p, %p)\n", cid, cb, arg ) ;
    printf ( "OUT: httpClientSetCookieSendCallbackEx(%d, %p, %p)\n", cid, (opd32*)__get_opd32(cb), arg) ;
    return httpClientSetCookieSendCallbackEx(cid, (opd32*)__get_opd32(cb), arg);
}

s32 httpClientSetCookieRecvCallback(httpClientId cid, httpCookieRecvCallback cb, void *arg)
{
    printf ( "IN: httpClientSetCookieRecvCallback(%d, %p, %p)\n", cid, cb, arg ) ;
    printf ( "OUT: httpClientSetCookieRecvCallbackEx(%d, %p, %p)\n", cid, (opd32*)__get_opd32(cb), arg) ;
    return httpClientSetCookieRecvCallbackEx(cid, (opd32*)__get_opd32(cb), arg);
}

