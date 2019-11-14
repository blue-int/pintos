#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

struct ste {
  void * vaddr;
};

void swap_init (void);
bool swap_out (struct fte * fte);
void swap_in (void *vaddr);

#endif
