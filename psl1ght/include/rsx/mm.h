#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//Initialize rsx heap.
s32 rsxHeapInit();
//Aligned Allocate from rsx heap.
void *rsxMemAlign(u32 size, u32 align);
//Non Aligned Allocate from rsx heap.
void *rsxAlloc(u32 size);
//Free allocated rsx memory.
void rsxFree(void *ptr);

#ifdef __cplusplus
}
#endif
