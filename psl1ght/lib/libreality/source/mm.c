#include <malloc.h>

#include "rsx/reality.h"
#include "rsx/mm.h"
#include "mm-internal.h"

static u32 initialized = 0;
static gcmConfiguration config;
static u64 rsx_heap_ptr;
static heap_t *rsx_heap;

#define _ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

//Collect unused nodes and merge them to bigger ones.
static void _rsxHeapCollect()
{
  hnode_t *node = rsx_heap->first;

  //Iterate over all nodes.
  while(node != NULL)
  {
    //If we find an unused node ...
    if(node->used == HNODE_UNUSED)
      //... and if the previous node is also unused ...
      if(node->prev != NULL && node->prev->used == HNODE_UNUSED)
      {
        //... merge them.
        node->prev->size += node->size;
        node->prev->next = node->next;
        if(node->next != NULL)
          node->next->prev = node->prev;
        //Free node.
        free(node);
      }
    node = node->next;
  }
}

s32 rsxHeapInit()
{
  if(initialized == 0)
  {
    //Get config.
    gcmGetConfiguration(&config);
    //Setup heap.
    rsx_heap_ptr = (u64)config.localAddress;
    //Allocate base heap.
    if((rsx_heap = (heap_t *)malloc(sizeof(heap_t))) == NULL)
      return 0;
    //Start address and size.
    rsx_heap->start = rsx_heap_ptr;
    rsx_heap->size = config.localSize;
    //No memory used.
    rsx_heap->usedmem = 0;
    rsx_heap->first = NULL;
    //And done.
    initialized = 1;
  }

  return 1;
}

void *rsxMemAlign(u32 align, u32 size)
{
  hnode_t *node, *new;
  u32 search = 1;

  if(initialized == 0)
    return NULL;

  //First node?
  if(rsx_heap->first == NULL)
  {
    if(size > rsx_heap->size)
      return NULL;
    //Allocate new node.
    if((node = (hnode_t *)malloc(sizeof(hnode_t))) == NULL)
      return NULL;

    //The first address should be aligned anyway.
    node->address = rsx_heap->start;
    node->aligned_address = rsx_heap->start;
    node->used = HNODE_USED;
    node->size = size;
    node->prev = NULL;
    node->next = NULL;
    rsx_heap->first = node;
    rsx_heap->usedmem = size;

    return (void *)node->aligned_address;
  }

  node = rsx_heap->first;
  while(search == 1)
  {
    //Unused node?
    if(node->used == HNODE_UNUSED)
    {
      //20% bigger is acceptable.
      if(size <= node->size && node->size <= (size + node->size / 5))
      {
        //Check if the aligned address fits.
        if((node->next != NULL && 
          _ALIGN(node->address, align) + size < node->next->address) ||
          (node->next == NULL && //TODO: will the following check work correctly everytime?
          _ALIGN(node->address, align) + size <= rsx_heap->start + rsx_heap->size))
          {
            node->used = HNODE_USED;
            //Align the address.
            node->aligned_address = _ALIGN(node->address, align);
            //Size + alignment difference.
            node->size = size + (node->aligned_address - node->address);
            //Update used memory.
            rsx_heap->usedmem += node->size;
            return (void *)node->aligned_address;
          }
          else //Add it to the end.
            search = 0;
      }
      else if(size < node->size) //Insert new node.
      {
        //Check if the aligned address fits.
        if((node->next != NULL && 
          _ALIGN(node->address, align) + size < node->next->address) ||
          (node->next == NULL && //TODO: will the following check work correctly everytime?
          _ALIGN(node->address, align) + size <= rsx_heap->start + rsx_heap->size))
          {
            //TODO: this looks somehow... yeah... haha
            //Allocate new node.
            if((new = (hnode_t *)malloc(sizeof(hnode_t))) == NULL)
              return NULL;

            //Align the address.
            node->aligned_address = _ALIGN(node->address, align);
            //The new node followes the current one.
            new->size = node->size - (size + (node->aligned_address - node->address));
            new->used = HNODE_UNUSED;
            new->address = node->aligned_address + size;
            //Insert into list.
            new->next = node->next;
            new->prev = node;
            node->size = size + (node->aligned_address - node->address);
            node->used = HNODE_UNUSED;
            //The next one is the new node.
            node->next = new;

            rsx_heap->usedmem += node->size;
            return (void *)node->aligned_address;
          }
          else //Add it to the end.
            search = 0;
      }
    }
    if(node->next != NULL)
      node = node->next;
    else
      search = 0;
  }

  //Add new node to the end.
  //Enough memory available to create a new node?
  //FIXME: This check only works for unaligned allocations.
  if(rsx_heap->usedmem + size > rsx_heap->size)
    return NULL;
  //Allocate new node.
  if((new = (hnode_t *)malloc(sizeof(hnode_t))) == NULL)
    return NULL;

  new->used = HNODE_USED;
  new->address = node->address + node->size;
  new->aligned_address = _ALIGN(node->address, align);
  new->size = size + (node->aligned_address - node->address);
  new->prev = node;
  new->next = NULL;
  node->next = new;

  rsx_heap->usedmem += node->size;
  return (void *)new->aligned_address;  
}

void *rsxAlloc(u32 size) {
	return rsxMemAlign(0, size);
}

void rsxFree(void *ptr)
{
  u64 addr = (u64)ptr;

  //Some basic error checking.
  if(initialized == 0)
    return;
  if(addr == 0 || 
    addr < (rsx_heap->start) ||
    addr > (rsx_heap->start + rsx_heap->size)
  )
    return;

  hnode_t *node = rsx_heap->first;
  u32 found = 0;

  //Find the correspondending node.
  while(node != NULL)
  {
    if(node->address == addr)
    {
      //Set it to unused.
      node->used = HNODE_UNUSED;
      //Update allocated size.
      rsx_heap->size -= node->size;
      found = 1;
      return;
    }
    node = node->next;
  }

  //Collect nodes (only if needed).
  if(found == 1)
    _rsxHeapCollect();
}
